// dock.cpp -- acoplamiento de ventanas del editor
// Basado en imgui_dock (docs/imguiDock), a su vez de LumixEngine.

#include "imgui_internal.h"

#include "tools/editor/dock.h"
#include "tools/editor/style.h"
#include "tools/editor/window.h"

#include "core/cmd.h"
#include "engine/console.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
    #define strcasecmp _stricmp
#else
    #include <strings.h>
#endif

std::vector<editor_dock*>   editor_dock::docks;
editor_dock*                editor_dock::active_dock = nullptr;

static const char* const dock_slot_names[DOCK_SLOT_COUNT] = { "left", "right", "top", "bottom", "tab" };

// el árbol cambió: que ImGui lo guarde en editor.ini (si aún hay contexto)
static void Dock_MarkDirty(void)
{
    if (ImGui::GetCurrentContext())
        ImGui::MarkIniSettingsDirty();
}

/*
==================
dock_node::has_open_tabs
==================
*/
bool dock_node::has_open_tabs() const
{
    for (const editor_window* w : tabs)
        if (w->is_open())
            return true;
    return false;
}

// hoja o contenedor con algo que mostrar
static bool Dock_Shown(const dock_node* node)
{
    if (!node)
        return false;
    if (node->is_leaf())
        return node->central || node->has_open_tabs();
    return Dock_Shown(node->child[0].get()) || Dock_Shown(node->child[1].get());
}

// puntero que posee al nodo: el de su padre o la raíz
static std::unique_ptr<dock_node>& Dock_Owner(std::unique_ptr<dock_node>& tree, dock_node* node)
{
    if (!node->parent)
        return tree;
    return node->parent->child[0].get() == node ? node->parent->child[0] : node->parent->child[1];
}

static bool Dock_Contains(const ImVec2& min, const ImVec2& max, const ImVec2& p)
{
    return p.x >= min.x && p.y >= min.y && p.x < max.x && p.y < max.y;
}

/*
==============================================================================

    editor_dock

==============================================================================
*/

editor_dock::editor_dock(const char* name) :
    name(name)
{
    reset_tree();
    docks.push_back(this);
    if (!active_dock)
        active_dock = this;
}

editor_dock::~editor_dock()
{
    docks.erase(std::find(docks.begin(), docks.end(), this));
    if (active_dock == this)
        active_dock = nullptr;
}

void editor_dock::reset_tree()
{
    tree = std::make_unique<dock_node>();
    tree->central = true;
    central_node = tree.get();
}

/*
==================
editor_dock::find_leaf / node_of
==================
*/
dock_node* editor_dock::find_leaf(dock_node* node, const editor_window* window) const
{
    if (!node)
        return nullptr;
    if (node->is_leaf())
        return std::find(node->tabs.begin(), node->tabs.end(), window) != node->tabs.end() ? node : nullptr;
    if (dock_node* n = find_leaf(node->child[0].get(), window))
        return n;
    return find_leaf(node->child[1].get(), window);
}

dock_node* editor_dock::node_of(const editor_window* window) const
{
    return find_leaf(tree.get(), window);
}

/*
==================
Dock_Fraction

Tamaño de la hoja nueva respecto a target: el pedido o, si no, el de la
ventana flotante (o su tamaño por defecto si nunca lo ha sido), entre el
20% y el 50%
==================
*/
static float Dock_Fraction(const editor_window* window, const ImVec2& target_size, const ImVec2& area_size,
    dock_slot slot, float fraction)
{
    if (fraction > 0.0f)
        return std::min(fraction, 0.9f);

    const int axis = (slot == DOCK_TOP || slot == DOCK_BOTTOM) ? 1 : 0;
    ImVec2 fs = window->get_float_size();
    if (fs.x <= 0 || fs.y <= 0)
    {
        const ImVec2 d = window->get_default_size();
        fs = ImVec2(d.x * area_size.x, d.y * area_size.y);
    }
    if (fs[axis] <= 0 || target_size[axis] <= 0)
        return 0.5f;
    return std::clamp(fs[axis] / target_size[axis], 0.2f, 0.5f);
}

/*
==================
editor_dock::dock
==================
*/
void editor_dock::dock(editor_window* window, dock_node* target, dock_slot slot, float fraction)
{
    if (!window || slot < 0 || slot >= DOCK_SLOT_COUNT)
        return;
    if (!target)
        target = tree.get();

    dock_node* own = node_of(window);
    if (own)
    {
        if (own == target && slot == DOCK_TAB)
        {
            activate(window);
            return;
        }

        // al sacarla, su hoja desaparece si se queda vacía y su padre pasa a
        // ser el hermano: hay que acoplar respecto a lo que queda
        const bool leaf_goes = own->tabs.size() == 1 && !own->central;
        if (leaf_goes && own == target)
            return;     // al lado de sí misma: nada que hacer
        if (leaf_goes && own->parent && own->parent == target)
            target = own->parent->child[0].get() == own ? own->parent->child[1].get() : own->parent->child[0].get();

        undock(window);
    }

    drop_node = nullptr;    // el árbol cambia: los punteros guardados dejan de valer

    // como pestaña: en una hoja (en un contenedor, en la central o en su primera hoja)
    if (slot == DOCK_TAB)
    {
        dock_node* leaf = target;
        while (!leaf->is_leaf())
            leaf = leaf->child[0].get();
        leaf->tabs.push_back(window);
        leaf->active = leaf->select = window;
        Dock_MarkDirty();
        return;
    }

    // a un lado: un contenedor nuevo ocupa el sitio de target, con target y
    // una hoja nueva como hijos
    const ImVec2 target_size(target->max.x - target->min.x, target->max.y - target->min.y);
    const ImVec2 area_size(area_max.x - area_min.x, area_max.y - area_min.y);
    const float f = Dock_Fraction(window, target_size, area_size, slot, fraction);
    const bool first = (slot == DOCK_LEFT || slot == DOCK_TOP);

    auto leaf = std::make_unique<dock_node>();
    leaf->tabs.push_back(window);
    leaf->active = window;

    auto container = std::make_unique<dock_node>();
    container->parent = target->parent;
    container->vertical = (slot == DOCK_TOP || slot == DOCK_BOTTOM);
    container->ratio = first ? f : 1.0f - f;
    container->min = target->min;
    container->max = target->max;

    std::unique_ptr<dock_node>& owner = Dock_Owner(tree, target);
    std::unique_ptr<dock_node> old = std::move(owner);
    old->parent = container.get();
    leaf->parent = container.get();
    container->child[first ? 0 : 1] = std::move(leaf);
    container->child[first ? 1 : 0] = std::move(old);
    owner = std::move(container);

    Dock_MarkDirty();
}

/*
==================
editor_dock::undock
==================
*/
void editor_dock::undock(editor_window* window)
{
    dock_node* leaf = node_of(window);
    if (!leaf)
        return;

    leaf->tabs.erase(std::find(leaf->tabs.begin(), leaf->tabs.end(), window));
    if (leaf->select == window)
        leaf->select = nullptr;
    if (leaf->active == window)
        leaf->active = nullptr;     // arrange() elige otra

    if (leaf->tabs.empty() && !leaf->central)
        remove_leaf(leaf);

    drop_node = nullptr;
    Dock_MarkDirty();
}

/*
==================
editor_dock::remove_leaf

La hoja desaparece y su hermano ocupa el sitio del padre
==================
*/
void editor_dock::remove_leaf(dock_node* leaf)
{
    dock_node* parent = leaf->parent;
    if (!parent)
        return;     // la raíz siempre es la central o un contenedor

    const int index = parent->child[0].get() == leaf ? 0 : 1;
    std::unique_ptr<dock_node> sibling = std::move(parent->child[index ^ 1]);
    sibling->parent = parent->parent;
    sibling->min = parent->min;
    sibling->max = parent->max;

    Dock_Owner(tree, parent) = std::move(sibling);  // destruye el padre y la hoja
}

/*
==================
editor_dock::activate
==================
*/
void editor_dock::activate(editor_window* window)
{
    if (dock_node* leaf = node_of(window))
        leaf->active = leaf->select = window;
}

/*
==================
editor_dock::content_rect
==================
*/
bool editor_dock::content_rect(const editor_window* window, ImVec2& min, ImVec2& max) const
{
    const dock_node* leaf = node_of(window);
    if (!leaf || !leaf->visible || leaf->active != window)
        return false;

    min = ImVec2(leaf->min.x, leaf->min.y + tabbar_height());
    max = leaf->max;
    return max.x > min.x && max.y > min.y;
}

/*
==============================================================================

    Disposición

==============================================================================
*/

float editor_dock::tabbar_height() const
{
    return std::floor(ImGui::GetFrameHeight() + ed_style.scaled(4.0f));
}

/*
==================
editor_dock::min_size

Lo mínimo que necesita un nodo: una hoja, lo que piden sus ventanas abiertas
más la barra de pestañas; un contenedor, la suma de sus hijos visibles
==================
*/
ImVec2 editor_dock::min_size(const dock_node* node) const
{
    if (!Dock_Shown(node))
        return ImVec2(0, 0);

    if (node->is_leaf())
    {
        const float leaf_min = ed_style.scaled(ed_style.dock_min_leaf);
        ImVec2 m(leaf_min, leaf_min);
        if (node->central && !node->has_open_tabs())
            return ImVec2(ed_style.scaled(ed_style.dock_min_center), ed_style.scaled(ed_style.dock_min_center));

        for (const editor_window* w : node->tabs)
        {
            if (!w->is_open())
                continue;
            ImVec2 lo, hi;
            w->get_size_limits(lo, hi);
            m = ImVec2(std::max(m.x, lo.x), std::max(m.y, lo.y));
        }
        m.y += tabbar_height();
        return m;
    }

    const bool s0 = Dock_Shown(node->child[0].get());
    const bool s1 = Dock_Shown(node->child[1].get());
    const ImVec2 a = min_size(node->child[0].get());
    const ImVec2 b = min_size(node->child[1].get());
    if (!(s0 && s1))
        return s0 ? a : b;

    const float gap = ed_style.scaled(ed_style.dock_splitter);
    if (node->vertical)
        return ImVec2(std::max(a.x, b.x), a.y + b.y + gap);
    return ImVec2(a.x + b.x + gap, std::max(a.y, b.y));
}

/*
==================
editor_dock::arrange
==================
*/
void editor_dock::arrange(dock_node* node, const ImVec2& pos, const ImVec2& size)
{
    node->min = pos;
    node->max = ImVec2(pos.x + size.x, pos.y + size.y);
    node->visible = size.x > 0 && size.y > 0 && Dock_Shown(node);

    if (node->is_leaf())
    {
        // pestaña visible: la activa si sigue abierta; si no, la primera abierta
        const bool active_ok = node->active && node->active->is_open()
            && std::find(node->tabs.begin(), node->tabs.end(), node->active) != node->tabs.end();
        if (!active_ok)
        {
            node->active = nullptr;
            for (editor_window* w : node->tabs)
                if (w->is_open())
                {
                    node->active = node->select = w;
                    break;
                }
        }
        return;
    }

    dock_node* c0 = node->child[0].get();
    dock_node* c1 = node->child[1].get();
    const bool s0 = node->visible && Dock_Shown(c0);
    const bool s1 = node->visible && Dock_Shown(c1);

    if (s0 && s1)
    {
        const int axis = node->vertical ? 1 : 0;
        const float gap = ed_style.scaled(ed_style.dock_splitter);
        const float total = std::max(size[axis] - gap, 0.0f);
        const float m0 = min_size(c0)[axis];
        const float m1 = min_size(c1)[axis];

        float a = std::floor(total * node->ratio);
        if (m0 + m1 <= total)
            a = std::clamp(a, m0, total - m1);
        else if (m0 + m1 > 0)
            a = std::floor(total * m0 / (m0 + m1));     // no cabe: en proporción a los mínimos

        ImVec2 size0 = size, size1 = size, pos1 = pos;
        size0[axis] = a;
        size1[axis] = total - a;
        pos1[axis] += a + gap;
        arrange(c0, pos, size0);
        arrange(c1, pos1, size1);
    }
    else
    {
        // el hijo visible ocupa todo el sitio; el otro, nada
        arrange(c0, pos, s0 ? size : ImVec2(0, 0));
        arrange(c1, pos, s1 ? size : ImVec2(0, 0));
    }
}

/*
==============================================================================

    Dibujo

==============================================================================
*/

/*
==================
editor_dock::begin_frame
==================
*/
void editor_dock::begin_frame(const ImVec2& pos, const ImVec2& size)
{
    area_min = pos;
    area_max = ImVec2(pos.x + size.x, pos.y + size.y);

    update_drag();                  // puede acoplar la ventana que se soltó
    arrange(tree.get(), pos, size);

// ventana de fondo: pestañas y separadores, detrás de las acopladas
    const std::string host_id = "##dockspace_" + name;
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
        | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus
        | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking;
    ImGui::Begin(host_id.c_str(), nullptr, flags);
    ImGui::PopStyleVar(3);

    draw_node(tree.get());

    ImGui::End();

// pestaña arrastrada fuera de su barra: la ventana sale del árbol y sigue
// al ratón como flotante (se dibuja así ya en este frame)
    if (tear_window)
    {
        editor_window* w = tear_window;
        tear_window = nullptr;
        undock(w);
        w->begin_float_drag(tear_offset);
        drag_window = w;
        arrange(tree.get(), pos, size);
    }
}

/*
==================
editor_dock::end_frame
==================
*/
void editor_dock::end_frame()
{
    const std::string host_id = "##dockspace_" + name;
    if (ImGuiWindow* host = ImGui::FindWindowByName(host_id.c_str()))
        ImGui::BringWindowToDisplayBack(host);    // detrás de las acopladas

    draw_drop_targets();
}

/*
==================
editor_dock::draw_node
==================
*/
void editor_dock::draw_node(dock_node* node)
{
    if (!node->visible)
        return;

    if (node->is_leaf())
    {
        if (node->has_open_tabs())
            draw_tabbar(node);
        return;
    }

    draw_node(node->child[0].get());
    draw_node(node->child[1].get());
    if (node->child[0]->visible && node->child[1]->visible)
        draw_splitter(node);
}

/*
==================
editor_dock::draw_tabbar
==================
*/
void editor_dock::draw_tabbar(dock_node* leaf)
{
    ImGuiIO& io = ImGui::GetIO();
    const float h = tabbar_height();
    const ImVec2 bar_min = leaf->min;
    const ImVec2 bar_max(leaf->max.x, leaf->min.y + h);

// fondo como la barra de título: más claro si la ventana visible tiene el foco
    const bool focused = leaf->active && leaf->active->is_focused();
    ImGui::GetWindowDrawList()->AddRectFilled(bar_min, bar_max,
        ImGui::GetColorU32(focused ? ed_style.title_bg_active : ed_style.title_bg));

    ImGui::PushID(leaf);
    ImGui::SetCursorScreenPos(bar_min);
    const ImGuiWindowFlags child_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
        | ImGuiWindowFlags_NoBackground;
    if (ImGui::BeginChild("##tabs", ImVec2(bar_max.x - bar_min.x, h), ImGuiChildFlags_None, child_flags))
    {
        // pestañas pegadas al borde inferior de la barra
        ImGui::SetCursorPos(ImVec2(ed_style.scaled(4.0f), h - ImGui::GetFrameHeight()));

        const ImGuiTabBarFlags bar_flags = ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyShrink
            | ImGuiTabBarFlags_NoCloseWithMiddleMouseButton;
        if (ImGui::BeginTabBar("##bar", bar_flags))
        {
            const std::vector<editor_window*> tabs = leaf->tabs;    // cerrar cambia la lista
            for (editor_window* w : tabs)
            {
                if (!w->is_open())
                    continue;

                const std::string label = std::string(w->get_title()) + "###" + w->get_name();
                const bool closable = w->has_option(WND_CLOSABLE);
                bool open = true;
                const ImGuiTabItemFlags tab_flags = (leaf->select == w) ? ImGuiTabItemFlags_SetSelected : 0;

                const bool selected = ImGui::BeginTabItem(label.c_str(), closable ? &open : nullptr, tab_flags);

                // arrastrada fuera de la barra: sale del árbol
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                {
                    const float out = std::max(bar_min.y - io.MousePos.y, io.MousePos.y - bar_max.y);
                    if (out > h * 0.5f)
                    {
                        tear_window = w;
                        tear_offset = ImVec2(io.MousePos.x - ImGui::GetItemRectMin().x, ImGui::GetFrameHeight() * 0.5f);
                    }
                }

                // doble clic: maximizar / restaurar
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    w->toggle_maximize();

                if (selected)
                {
                    leaf->active = w;
                    ImGui::EndTabItem();
                }
                if (!open)
                    w->hide();
            }
            leaf->select = nullptr;
            ImGui::EndTabBar();
        }
    }
    ImGui::EndChild();
    ImGui::PopID();
}

/*
==================
editor_dock::draw_splitter

Separador entre los dos hijos de un contenedor: al arrastrarlo cambia el
reparto, respetando el mínimo de cada lado
==================
*/
void editor_dock::draw_splitter(dock_node* container)
{
    static float grab = 0.0f;   // punto agarrado dentro del separador

    dock_node* c0 = container->child[0].get();
    dock_node* c1 = container->child[1].get();
    const int axis = container->vertical ? 1 : 0;

    ImVec2 min = container->min, max = container->max;
    min[axis] = c0->max[axis];
    max[axis] = c1->min[axis];
    if (max[axis] <= min[axis])
        return;

    ImGui::PushID(container);
    ImGui::SetCursorScreenPos(min);
    ImGui::InvisibleButton("##split", ImVec2(max.x - min.x, max.y - min.y));

    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    if (hovered || active)
        ImGui::SetMouseCursor(container->vertical ? ImGuiMouseCursor_ResizeNS : ImGuiMouseCursor_ResizeEW);

    const float mouse = ImGui::GetIO().MousePos[axis];
    if (ImGui::IsItemActivated())
        grab = mouse - min[axis];

    if (active)
    {
        const float gap = max[axis] - min[axis];
        const float total = (container->max[axis] - container->min[axis]) - gap;
        const float m0 = min_size(c0)[axis];
        const float m1 = min_size(c1)[axis];
        if (total > 0 && m0 + m1 <= total)
        {
            const float a = std::clamp(mouse - grab - container->min[axis], m0, total - m1);
            container->ratio = a / total;
            Dock_MarkDirty();
        }
    }

    if (hovered || active)
        ImGui::GetWindowDrawList()->AddRectFilled(min, max,
            ImGui::GetColorU32(active ? ed_style.accent : ed_style.accent_hovered));
    ImGui::PopID();
}

/*
==============================================================================

    Arrastre

    Mientras ImGui mueve una ventana flotante del editor se buscan destinos
    bajo el ratón; al soltarla sobre uno se acopla (como handleDrag y
    dockSlots de imgui_dock).

==============================================================================
*/

// botón de destino: s = lado del botón, centrado en c
static void Dock_ButtonRect(const ImVec2& c, dock_slot slot, float b, ImVec2& min, ImVec2& max)
{
    const float step = b * 1.25f;
    ImVec2 p = c;
    switch (slot)
    {
    case DOCK_LEFT:     p.x -= step; break;
    case DOCK_RIGHT:    p.x += step; break;
    case DOCK_TOP:      p.y -= step; break;
    case DOCK_BOTTOM:   p.y += step; break;
    default:            break;
    }
    min = ImVec2(p.x - b * 0.5f, p.y - b * 0.5f);
    max = ImVec2(p.x + b * 0.5f, p.y + b * 0.5f);
}

// botones de los bordes del área de trabajo
static void Dock_BorderRect(const ImVec2& amin, const ImVec2& amax, dock_slot slot, float b, ImVec2& min, ImVec2& max)
{
    const float margin = b * 0.35f;
    const ImVec2 c((amin.x + amax.x) * 0.5f, (amin.y + amax.y) * 0.5f);
    ImVec2 p = c;
    switch (slot)
    {
    case DOCK_LEFT:     p.x = amin.x + margin + b * 0.5f; break;
    case DOCK_RIGHT:    p.x = amax.x - margin - b * 0.5f; break;
    case DOCK_TOP:      p.y = amin.y + margin + b * 0.5f; break;
    case DOCK_BOTTOM:   p.y = amax.y - margin - b * 0.5f; break;
    default:            break;
    }
    min = ImVec2(p.x - b * 0.5f, p.y - b * 0.5f);
    max = ImVec2(p.x + b * 0.5f, p.y + b * 0.5f);
}

/*
==================
editor_dock::update_drag
==================
*/
void editor_dock::update_drag()
{
    ImGuiContext& g = *GImGui;

// ventana flotante del editor que ImGui está moviendo
    editor_window* moving = nullptr;
    if (g.MovingWindow)
    {
        const char* id = g.MovingWindow->RootWindow->Name;
        for (editor_window* w : editor_window::windex)
            if (!strcmp(w->get_imgui_id(), id) && !node_of(w) && !w->is_maximized())
                moving = w;
    }

    if (moving)
    {
        drag_window = moving;
        drop_node = nullptr;
        drop_slot = DOCK_SLOT_COUNT;
        if (!g.IO.KeyShift)
            find_drop(g.IO.MousePos);
        return;
    }

// se soltó: acoplar si estaba sobre un destino
    if (drag_window)
    {
        editor_window* w = drag_window;
        const dock_slot slot = drop_slot;
        dock_node* target = drop_node;
        drag_window = nullptr;
        drop_node = nullptr;
        drop_slot = DOCK_SLOT_COUNT;

        const bool alive = std::find(editor_window::windex.begin(), editor_window::windex.end(), w) != editor_window::windex.end();
        if (alive && slot != DOCK_SLOT_COUNT)
        {
            dock(w, target, slot);
            w->focus();
        }
    }
}

/*
==================
editor_dock::leaf_at
==================
*/
dock_node* editor_dock::leaf_at(dock_node* node, const ImVec2& p) const
{
    if (!node || !node->visible || !Dock_Contains(node->min, node->max, p))
        return nullptr;
    if (node->is_leaf())
        return node;
    if (dock_node* n = leaf_at(node->child[0].get(), p))
        return n;
    return leaf_at(node->child[1].get(), p);
}

/*
==================
editor_dock::find_drop
==================
*/
void editor_dock::find_drop(const ImVec2& p)
{
    const float b = ed_style.scaled(ed_style.dock_drop_button);
    ImVec2 min, max;

    // bordes del área: a un lado de todo el árbol
    for (int s = DOCK_LEFT; s <= DOCK_BOTTOM; s++)
    {
        Dock_BorderRect(area_min, area_max, (dock_slot)s, b, min, max);
        if (Dock_Contains(min, max, p))
        {
            drop_node = nullptr;
            drop_slot = (dock_slot)s;
            return;
        }
    }

    // centro de la hoja bajo el ratón
    dock_node* leaf = leaf_at(tree.get(), p);
    if (!leaf)
        return;
    const ImVec2 c((leaf->min.x + leaf->max.x) * 0.5f, (leaf->min.y + leaf->max.y) * 0.5f);
    for (int s = 0; s < DOCK_SLOT_COUNT; s++)
    {
        Dock_ButtonRect(c, (dock_slot)s, b, min, max);
        if (Dock_Contains(min, max, p))
        {
            drop_node = leaf;
            drop_slot = (dock_slot)s;
            return;
        }
    }
}

/*
==================
editor_dock::preview_min

Dónde quedaría la ventana arrastrada si se suelta en el destino actual
==================
*/
ImVec2 editor_dock::preview_min(dock_node* target, dock_slot slot, ImVec2& max) const
{
    ImVec2 min = target ? target->min : area_min;
    max = target ? target->max : area_max;
    if (slot == DOCK_TAB)
        return min;

    const ImVec2 size(max.x - min.x, max.y - min.y);
    const ImVec2 area_size(area_max.x - area_min.x, area_max.y - area_min.y);
    const float f = Dock_Fraction(drag_window, size, area_size, slot, 0.0f);
    switch (slot)
    {
    case DOCK_LEFT:     max.x = min.x + size.x * f; break;
    case DOCK_RIGHT:    min.x = max.x - size.x * f; break;
    case DOCK_TOP:      max.y = min.y + size.y * f; break;
    case DOCK_BOTTOM:   min.y = max.y - size.y * f; break;
    default:            break;
    }
    return min;
}

/*
==================
Dock_DrawButton

Cuadrado con un icono: el lado ocupado (o todo, para pestaña)
==================
*/
static void Dock_DrawButton(ImDrawList* dl, const ImVec2& min, const ImVec2& max, dock_slot slot, bool hovered)
{
    const float rounding = ed_style.scaled(3.0f);
    dl->AddRectFilled(min, max, ImGui::GetColorU32(ed_style.popup_bg), rounding);
    dl->AddRect(min, max, ImGui::GetColorU32(hovered ? ed_style.accent : ed_style.border), rounding);

    const float pad = std::floor((max.x - min.x) * 0.2f);
    ImVec2 imin(min.x + pad, min.y + pad), imax(max.x - pad, max.y - pad);
    const ImU32 frame = ImGui::GetColorU32(ed_style.text_disabled);
    const ImU32 fill = ImGui::GetColorU32(hovered ? ed_style.accent : ed_style.text_disabled);
    dl->AddRect(imin, imax, frame);

    const ImVec2 half((imax.x - imin.x) * 0.5f, (imax.y - imin.y) * 0.5f);
    switch (slot)
    {
    case DOCK_LEFT:     imax.x -= half.x; break;
    case DOCK_RIGHT:    imin.x += half.x; break;
    case DOCK_TOP:      imax.y -= half.y; break;
    case DOCK_BOTTOM:   imin.y += half.y; break;
    default:            break;
    }
    dl->AddRectFilled(imin, imax, fill);
}

/*
==================
editor_dock::draw_drop_targets
==================
*/
void editor_dock::draw_drop_targets()
{
    if (!drag_window || ImGui::GetIO().KeyShift)
        return;

    ImDrawList* fg = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float b = ed_style.scaled(ed_style.dock_drop_button);
    ImVec2 min, max;

// dónde quedaría
    if (drop_slot != DOCK_SLOT_COUNT)
    {
        const ImVec2 pmin = preview_min(drop_node, drop_slot, max);
        const float rounding = ed_style.scaled(ed_style.window_rounding);
        fg->AddRectFilled(pmin, max, ImGui::GetColorU32(ed_style.drop_preview), rounding);
        fg->AddRect(pmin, max, ImGui::GetColorU32(ed_style.accent), rounding, 0, std::max(1.0f, ed_style.scaled(2.0f)));
    }

// botones de los bordes
    for (int s = DOCK_LEFT; s <= DOCK_BOTTOM; s++)
    {
        Dock_BorderRect(area_min, area_max, (dock_slot)s, b, min, max);
        Dock_DrawButton(fg, min, max, (dock_slot)s, !drop_node && drop_slot == s);
    }

// botones de la hoja bajo el ratón
    if (dock_node* leaf = leaf_at(tree.get(), mouse))
    {
        const ImVec2 c((leaf->min.x + leaf->max.x) * 0.5f, (leaf->min.y + leaf->max.y) * 0.5f);
        for (int s = 0; s < DOCK_SLOT_COUNT; s++)
        {
            Dock_ButtonRect(c, (dock_slot)s, b, min, max);
            Dock_DrawButton(fg, min, max, (dock_slot)s, drop_node == leaf && drop_slot == s);
        }
    }
}

/*
==============================================================================

    editor.ini

    [Dock][main]
    Tree=V0.7000(H0.2200(L0[info],C0[]),L0[console])

    H/V: contenedor lado a lado / uno encima de otro, con la fracción del
    primer hijo. L/C: hoja / hoja central, con el índice de la pestaña
    activa y los nombres de sus ventanas.

==============================================================================
*/

/*
==================
editor_dock::write_node
==================
*/
void editor_dock::write_node(ImGuiTextBuffer* buf, const dock_node* node) const
{
    if (!node->is_leaf())
    {
        buf->appendf("%c%.4f(", node->vertical ? 'V' : 'H', node->ratio);
        write_node(buf, node->child[0].get());
        buf->append(",");
        write_node(buf, node->child[1].get());
        buf->append(")");
        return;
    }

    int active = 0;
    for (size_t i = 0; i < node->tabs.size(); i++)
        if (node->tabs[i] == node->active)
            active = (int)i;

    buf->appendf("%c%d[", node->central ? 'C' : 'L', active);
    for (size_t i = 0; i < node->tabs.size(); i++)
        buf->appendf("%s%s", i ? "," : "", node->tabs[i]->get_name());
    buf->append("]");
}

/*
==================
editor_dock::parse_node

nullptr si el texto no es válido
==================
*/
std::unique_ptr<dock_node> editor_dock::parse_node(const char*& p, dock_node* parent)
{
    auto node = std::make_unique<dock_node>();
    node->parent = parent;

    const char type = *p++;
    if (type == 'H' || type == 'V')
    {
        char* end;
        node->vertical = (type == 'V');
        node->ratio = std::strtof(p, &end);
        if (end == p || !std::isfinite(node->ratio))
            return nullptr;
        node->ratio = std::clamp(node->ratio, 0.02f, 0.98f);
        p = end;

        if (*p++ != '(')
            return nullptr;
        node->child[0] = parse_node(p, node.get());
        if (!node->child[0] || *p++ != ',')
            return nullptr;
        node->child[1] = parse_node(p, node.get());
        if (!node->child[1] || *p++ != ')')
            return nullptr;
        return node;
    }

    if (type != 'L' && type != 'C')
        return nullptr;

    node->central = (type == 'C');
    char* end;
    const long active = std::strtol(p, &end, 10);
    p = end;
    if (*p++ != '[')
        return nullptr;

    while (*p && *p != ']')
    {
        const char* start = p;
        while (*p && *p != ',' && *p != ']')
            p++;
        const std::string window_name(start, p);
        if (*p == ',')
            p++;

        // ventanas que ya no existen o repetidas: se ignoran
        editor_window* w = editor_window::find(window_name.c_str());
        if (w && std::find(parse_seen.begin(), parse_seen.end(), w) == parse_seen.end())
        {
            node->tabs.push_back(w);
            parse_seen.push_back(w);
        }
    }
    if (*p++ != ']')
        return nullptr;

    if (active >= 0 && active < (long)node->tabs.size())
        node->active = node->select = node->tabs[active];
    return node;
}

// Quita las hojas vacías (no centrales) que dejó una ventana que ya no existe
static void Dock_Prune(std::unique_ptr<dock_node>& node)
{
    if (node->is_leaf())
        return;

    Dock_Prune(node->child[0]);
    Dock_Prune(node->child[1]);

    for (int i = 0; i < 2; i++)
    {
        const dock_node* c = node->child[i].get();
        if (c->is_leaf() && c->tabs.empty() && !c->central)
        {
            std::unique_ptr<dock_node> other = std::move(node->child[i ^ 1]);
            other->parent = node->parent;
            node = std::move(other);
            return;
        }
    }
}

static int Dock_CountCentral(const dock_node* node, dock_node** central)
{
    if (node->is_leaf())
    {
        if (node->central)
            *central = const_cast<dock_node*>(node);
        return node->central ? 1 : 0;
    }
    return Dock_CountCentral(node->child[0].get(), central) + Dock_CountCentral(node->child[1].get(), central);
}

void* editor_dock::settings_read_open(ImGuiContext*, ImGuiSettingsHandler*, const char* name)
{
    for (editor_dock* d : docks)
        if (d->name == name)
            return d;
    return nullptr;
}

void editor_dock::settings_read_line(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line)
{
    editor_dock* d = static_cast<editor_dock*>(entry);
    if (strncmp(line, "Tree=", 5))
        return;

    // se lee sobre un árbol aparte: si el texto no vale, se queda el actual
    std::unique_ptr<dock_node> saved = std::move(d->tree);
    dock_node* saved_central = d->central_node;
    d->tree.reset();

    const char* p = line + 5;
    d->parse_seen.clear();
    std::unique_ptr<dock_node> parsed = d->parse_node(p, nullptr);
    d->parse_seen.clear();
    dock_node* central = nullptr;
    if (parsed)
    {
        Dock_Prune(parsed);
        parsed->parent = nullptr;
    }
    if (!parsed || Dock_CountCentral(parsed.get(), &central) != 1)
    {
        d->tree = std::move(saved);
        d->central_node = saved_central;
        return;
    }

    d->tree = std::move(parsed);
    d->central_node = central;
}

void editor_dock::settings_write_all(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    for (const editor_dock* d : docks)
    {
        buf->appendf("[%s][%s]\n", handler->TypeName, d->name.c_str());
        buf->append("Tree=");
        d->write_node(buf, d->tree.get());
        buf->append("\n\n");
    }
}

/*
==============================================================================

    Comandos

==============================================================================
*/

static void Dock_PrintNode(const dock_node* node, int depth)
{
    const int indent = depth * 2;
    if (!node->is_leaf())
    {
        Con_Printf("%*s%s %.0f%%\n", indent, "", node->vertical ? "vertical" : "horizontal", node->ratio * 100.0f);
        Dock_PrintNode(node->child[0].get(), depth + 1);
        Dock_PrintNode(node->child[1].get(), depth + 1);
        return;
    }

    std::string tabs;
    for (const editor_window* w : node->tabs)
    {
        if (!tabs.empty())
            tabs += ", ";
        tabs += w->get_name();
        if (w == node->active)
            tabs += "*";
        if (!w->is_open())
            tabs += " (oculta)";
    }
    Con_Printf("%*s%s [%s]\n", indent, "", node->central ? "central" : "hoja", tabs.c_str());
}

void editor_dock::print() const
{
    Con_Printf("dock \"%s\"\n", name.c_str());
    Dock_PrintNode(tree.get(), 1);
}

/*
==================
Dock_Window_f

dockwindow <ventana> float
dockwindow <ventana> <destino> [left|right|top|bottom|tab]
  destino: root (todo el área), central o el nombre de otra ventana acoplada
==================
*/
static void Dock_Window_f(void)
{
    editor_dock* dock = editor_dock::active();
    if (Cmd_Argc() < 3 || Cmd_Argc() > 4 || !dock)
    {
        Con_Printf("dockwindow <name> float\n");
        Con_Printf("dockwindow <name> <root|central|window> [left|right|top|bottom|tab]\n");
        return;
    }

    editor_window* w = editor_window::find(Cmd_Argv(1));
    if (!w)
    {
        Con_Printf("dockwindow: window %s not found\n", Cmd_Argv(1));
        return;
    }

    const char* target_name = Cmd_Argv(2);
    if (!strcasecmp(target_name, "float"))
    {
        dock->undock(w);
        return;
    }

    dock_node* target = nullptr;
    if (!strcasecmp(target_name, "central"))
        target = dock->central();
    else if (strcasecmp(target_name, "root"))
    {
        editor_window* other = editor_window::find(target_name);
        target = other ? dock->node_of(other) : nullptr;
        if (!target)
        {
            Con_Printf("dockwindow: %s is not a docked window\n", target_name);
            return;
        }
    }

    dock_slot slot = DOCK_TAB;
    if (Cmd_Argc() == 4)
    {
        slot = DOCK_SLOT_COUNT;
        for (int s = 0; s < DOCK_SLOT_COUNT; s++)
            if (!strcasecmp(Cmd_Argv(3), dock_slot_names[s]))
                slot = (dock_slot)s;
        if (slot == DOCK_SLOT_COUNT)
        {
            Con_Printf("dockwindow: unknown slot %s\n", Cmd_Argv(3));
            return;
        }
    }
    if (!target && slot == DOCK_TAB)
        target = dock->central();   // pestaña "del área": en la central

    dock->dock(w, target, slot);
    w->show();
}

static void Dock_Info_f(void)
{
    if (editor_dock* dock = editor_dock::active())
        dock->print();
}

/*
==================
editor_dock::init
==================
*/
void editor_dock::init()
{
    ImGuiSettingsHandler handler;
    handler.TypeName = "Dock";
    handler.TypeHash = ImHashStr("Dock");
    handler.ReadOpenFn = settings_read_open;
    handler.ReadLineFn = settings_read_line;
    handler.WriteAllFn = settings_write_all;
    ImGui::AddSettingsHandler(&handler);

    Cmd_AddCommand("dockwindow", Dock_Window_f);
    Cmd_AddCommand("dockinfo", Dock_Info_f);
}
