// window.cpp -- clase base de las ventanas del editor
// Basado en Window.cpp de QuakeEd 3 (docs/quaked3-master/Window.cpp), migrado a ImGui.

#include "imgui_internal.h"     // antes que imgui.h: GImGui, ImGuiWindow, IDs de redimensionado

#include "tools/editor/window.h"
#include "tools/editor/layout.h"
#include "tools/editor/style.h"

#include "core/cmd.h"
#include "engine/console.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#ifdef _WIN32
    #define strcasecmp _stricmp
#else
    #include <strings.h>
#endif

std::vector<editor_window*> editor_window::windex;

/*
==================
editor_window
==================
*/
editor_window::editor_window(const char* name, const char* title, unsigned options) :
    name(name), title(title), options(options),
    open(true), minimized(false), focused(false),
    request_collapse(-1), request_focus(false),
    default_pos(0.05f, 0.05f), default_size(0.30f, 0.30f),
    min_size(ed_style.window_min_size), max_size(0, 0), content_size(0, 0),
    rect{ ImVec2(0, 0), ImVec2(0, 0) },
    dock_region(LAYOUT_FLOAT),
    float_size(0, 0), request_size(0, 0), dock_resizing(false)
{
    // "###" hace que el ID de ImGui (y editor.ini) dependa solo del nombre,
    // así se puede cambiar el título sin perder la posición guardada
    imgui_id = this->title + "###" + this->name;
    windex.push_back(this);
}

/*
==================
~editor_window
==================
*/
editor_window::~editor_window()
{
    windex.erase(std::find(windex.begin(), windex.end(), this));
}

/*
========================================================================

    VISIBILIDAD Y ESTADO

========================================================================
*/

/*
==================
editor_window::show
==================
*/
void editor_window::show()
{
    if (!open)
    {
        open = true;
        on_show();
    }
    request_focus = true;
}

/*
==================
editor_window::hide
==================
*/
void editor_window::hide()
{
    if (open)
    {
        open = false;
        on_hide();
    }
}

/*
==================
editor_window::toggle

Como en QE3: si está abierta pero no tiene el foco, se trae al frente;
si ya tiene el foco, se oculta
==================
*/
void editor_window::toggle()
{
    if (open)
    {
        if (!focused)
            focus();
        else
            hide();
    }
    else
        show();
}

/*
==================
editor_window::focus
==================
*/
void editor_window::focus()
{
    request_focus = true;
    if (minimized)
        restore();
}

/*
==================
Wnd_CanLeaveMain

Con viewports las ventanas flotantes pueden salir de la ventana principal
(ImGui crea una ventana del sistema para ellas), así que no se limitan a
su área de trabajo
==================
*/
static bool Wnd_CanLeaveMain(void)
{
    return (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0;
}

/*
==================
Wnd_Region

Región de la ventana en el layout activo
==================
*/
static layout_region Wnd_Region(const editor_window* w)
{
    const editor_layout* l = editor_layout::active();
    return l ? l->region_of(w) : LAYOUT_FLOAT;
}

/*
==================
editor_window::minimize

Flotante: se contrae a la barra de título (ImGui).
Acoplada a un lado: deja de dibujarse y el layout la muestra en la barra
de su borde. El centro no se minimiza.
==================
*/
void editor_window::minimize()
{
    if (!has_option(WND_MINIMIZABLE))
        return;

    const layout_region r = Wnd_Region(this);
    if (r == LAYOUT_FLOAT)
        request_collapse = 1;
    else if (r != LAYOUT_CENTER)
        minimized = true;
}

/*
==================
editor_window::restore
==================
*/
void editor_window::restore()
{
    if (Wnd_Region(this) != LAYOUT_FLOAT)
        minimized = false;
    request_collapse = 0;
}

/*
==================
editor_window::set_option
==================
*/
void editor_window::set_option(unsigned option, bool enable)
{
    if (enable)
        options |= option;
    else
        options &= ~option;

    // sin la opción de minimizar no puede quedarse minimizada
    if ((option & WND_MINIMIZABLE) && !enable && minimized)
        restore();
}

/*
==================
editor_window::set_default_rect
==================
*/
void editor_window::set_default_rect(float x, float y, float w, float h)
{
    default_pos = ImVec2(x, y);
    default_size = ImVec2(w, h);
}

/*
==================
editor_window::set_min_size
==================
*/
void editor_window::set_min_size(float w, float h)
{
    min_size = ImVec2(w, h);
}

/*
==================
editor_window::set_max_size
==================
*/
void editor_window::set_max_size(float w, float h)
{
    max_size = ImVec2(w, h);
}

/*
==================
editor_window::get_size_limits

Límites efectivos en px reales: código, luego contenido, luego on_size_limits()
==================
*/
void editor_window::get_size_limits(ImVec2& min, ImVec2& max) const
{
// por código
    min = ImVec2(ed_style.scaled(min_size.x), ed_style.scaled(min_size.y));
    max = ImVec2(max_size.x > 0 ? ed_style.scaled(max_size.x) : FLT_MAX,
                 max_size.y > 0 ? ed_style.scaled(max_size.y) : FLT_MAX);

// por contenido (cuando ya se ha medido)
    if (content_size.x > 0 && content_size.y > 0)
    {
        if (has_option(WND_MIN_CONTENT))
            min = ImVec2(std::max(min.x, content_size.x), std::max(min.y, content_size.y));
        if (has_option(WND_MAX_CONTENT))
            max = ImVec2(std::min(max.x, content_size.x), std::min(max.y, content_size.y));
    }

    on_size_limits(min, max);

    // el máximo nunca por debajo del mínimo
    max = ImVec2(std::max(max.x, min.x), std::max(max.y, min.y));
}

/*
==================
editor_window::get_layout_limits
==================
*/
void editor_window::get_layout_limits(ImVec2& min, ImVec2& max) const
{
    get_size_limits(min, max);
    if (has_option(WND_COVER_LAYOUT))
        max = ImVec2(FLT_MAX, FLT_MAX);
}

/*
==================
editor_window::build_flags
==================
*/
ImGuiWindowFlags editor_window::build_flags() const
{
    ImGuiWindowFlags flags = window_flags();

    if (!has_option(WND_MINIMIZABLE))
        flags |= ImGuiWindowFlags_NoCollapse;
    if (!has_option(WND_MOVABLE))
        flags |= ImGuiWindowFlags_NoMove;
    if (!has_option(WND_RESIZABLE))
        flags |= ImGuiWindowFlags_NoResize;
    if (!has_option(WND_TITLEBAR))
        flags |= ImGuiWindowFlags_NoTitleBar;

    return flags;
}

/*
========================================================================

    DESPLAZAMIENTO Y ACOPLAMIENTO

    Igual que TryDocking en QE3: al mover o redimensionar, los bordes que
    quedan a menos de ed_style.snap_move / snap_resize píxeles de un borde
    de otra ventana o del área de trabajo se pegan a él.

    Mejoras respecto a QE3:
    - solo se acopla a ventanas que se solapan en el otro eje, no a
      cualquier línea de la pantalla
    - las distancias se escalan por el DPI
    - sin viewports la ventana nunca sale del área de trabajo, ni siquiera
      cuando se achica la ventana principal. Con viewports puede salir y
      ImGui la convierte en una ventana del sistema
    - con Mayús pulsado se mueve o redimensiona libremente, sin acoplar

========================================================================
*/

/*
==================
editor_window::workspace

Área donde pueden estar las ventanas
==================
*/
window_rect editor_window::workspace()
{
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    return { vp->WorkPos, ImVec2(vp->WorkPos.x + vp->WorkSize.x, vp->WorkPos.y + vp->WorkSize.y) };
}

/*
==================
editor_window::find_closest

Devuelve la línea (x si axis = 0, y si axis = 1) más cercana a v, entre los
bordes del área de trabajo y los de las demás ventanas abiertas
==================
*/
float editor_window::find_closest(int axis, float v, const window_rect& self, const window_rect& field, float snap) const
{
    const int other = axis ^ 1;
    float best = (std::fabs(field.min[axis] - v) < std::fabs(field.max[axis] - v)) ? field.min[axis] : field.max[axis];

    for (const editor_window* w : windex)
    {
        if (w == this || !w->open)
            continue;

        const window_rect& r = w->rect;
        if (r.width() <= 0 || r.height() <= 0)
            continue;

        // solo ventanas que se solapan (o casi) en el otro eje
        if (r.max[other] < self.min[other] - snap || r.min[other] > self.max[other] + snap)
            continue;

        if (std::fabs(r.min[axis] - v) < std::fabs(best - v))
            best = r.min[axis];
        if (std::fabs(r.max[axis] - v) < std::fabs(best - v))
            best = r.max[axis];
    }
    return best;
}

/*
==================
editor_window::snap_move

Desplaza r para que su borde más cercano se pegue a otro borde
==================
*/
bool editor_window::snap_move(window_rect& r, const window_rect& field) const
{
    const float snap = ed_style.scaled(ed_style.snap_move);
    bool moved = false;

    for (int axis = 0; axis < 2; axis++)
    {
        float lo = find_closest(axis, r.min[axis], r, field, snap) - r.min[axis];
        float hi = find_closest(axis, r.max[axis], r, field, snap) - r.max[axis];
        float margin = (std::fabs(lo) < std::fabs(hi)) ? lo : hi;

        if (margin != 0.0f && std::fabs(margin) < snap)
        {
            r.min[axis] += margin;
            r.max[axis] += margin;
            moved = true;
        }
    }
    return moved;
}

/*
==================
editor_window::clamp_to_field

Mantiene r dentro del área de trabajo (achicándolo si no cabe)
==================
*/
void editor_window::clamp_to_field(window_rect& r, const window_rect& field)
{
    for (int axis = 0; axis < 2; axis++)
    {
        float size = std::min(r.max[axis] - r.min[axis], field.max[axis] - field.min[axis]);

        if (r.min[axis] < field.min[axis])
            r.min[axis] = field.min[axis];
        if (r.min[axis] + size > field.max[axis])
            r.min[axis] = field.max[axis] - size;
        r.max[axis] = r.min[axis] + size;
    }
}

/*
==================
editor_window::size_callback

ImGui la llama mientras se calcula el tamaño de la ventana
==================
*/
void editor_window::size_callback(ImGuiSizeCallbackData* data)
{
    static_cast<const editor_window*>(data->UserData)->snap_resize(data);
}

/*
==================
editor_window::snap_resize

Acopla los bordes que se están arrastrando al redimensionar. Se hace dentro
del cálculo de tamaño de ImGui para que no haya un frame de retraso
==================
*/
void editor_window::snap_resize(ImGuiSizeCallbackData* data) const
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* w = ImGui::FindWindowByName(imgui_id.c_str());
    if (!w || !g.ActiveId)
        return;

// qué bordes se están arrastrando (esquinas: 0 inf-der, 1 inf-izq, 2 sup-izq, 3 sup-der)
    bool edge[2][2] = {};   // [eje][0 = min, 1 = max]
    const ImGuiID id = g.ActiveId;
    if (id == ImGui::GetWindowResizeCornerID(w, 0))         { edge[0][1] = edge[1][1] = true; }
    else if (id == ImGui::GetWindowResizeCornerID(w, 1))    { edge[0][0] = edge[1][1] = true; }
    else if (id == ImGui::GetWindowResizeCornerID(w, 2))    { edge[0][0] = edge[1][0] = true; }
    else if (id == ImGui::GetWindowResizeCornerID(w, 3))    { edge[0][1] = edge[1][0] = true; }
    else if (id == ImGui::GetWindowResizeBorderID(w, ImGuiDir_Left))    edge[0][0] = true;
    else if (id == ImGui::GetWindowResizeBorderID(w, ImGuiDir_Right))   edge[0][1] = true;
    else if (id == ImGui::GetWindowResizeBorderID(w, ImGuiDir_Up))      edge[1][0] = true;
    else if (id == ImGui::GetWindowResizeBorderID(w, ImGuiDir_Down))    edge[1][1] = true;
    else
        return;     // no es un redimensionado con el ratón

// acoplada en el layout: solo se mueve el borde que da al centro
    if (dock_region != LAYOUT_FLOAT)
    {
        bool allowed[2][2] = {};
        switch (dock_region)
        {
        case LAYOUT_LEFT:   allowed[0][1] = true; break;
        case LAYOUT_RIGHT:  allowed[0][0] = true; break;
        case LAYOUT_TOP:    allowed[1][1] = true; break;
        case LAYOUT_BOTTOM: allowed[1][0] = true; break;
        default: break;
        }
        for (int axis = 0; axis < 2; axis++)
        {
            for (int side = 0; side < 2; side++)
                edge[axis][side] = edge[axis][side] && allowed[axis][side];
            if (!edge[axis][0] && !edge[axis][1])
                data->DesiredSize[axis] = data->CurrentSize[axis];
            else
                dock_resizing = true;
        }
    }

    const window_rect field = workspace();
    const float snap = ed_style.scaled(ed_style.snap_resize);
    const bool free = !has_option(WND_SNAP) || g.IO.KeyShift || dock_region != LAYOUT_FLOAT;
    // flotante con viewports: puede crecer fuera de la ventana principal
    const bool outside_ok = dock_region == LAYOUT_FLOAT && Wnd_CanLeaveMain();

// límites de tamaño. Acoplada: en el eje de su región manda el layout, que
// además impide invadir las otras regiones ocupadas
    ImVec2 lo, hi;
    get_size_limits(lo, hi);
    if (dock_region != LAYOUT_FLOAT)
    {
        const int axis = (dock_region == LAYOUT_LEFT || dock_region == LAYOUT_RIGHT) ? 0 : 1;
        if (const editor_layout* l = editor_layout::active())
            l->extent_limits(this, lo[axis], hi[axis]);
    }

// rectángulo propuesto: el borde opuesto al que se arrastra queda fijo
    window_rect r;
    for (int axis = 0; axis < 2; axis++)
    {
        if (edge[axis][0])
            r.min[axis] = data->Pos[axis] + data->CurrentSize[axis] - data->DesiredSize[axis];
        else
            r.min[axis] = data->Pos[axis];
        r.max[axis] = r.min[axis] + data->DesiredSize[axis];
    }

    for (int axis = 0; axis < 2; axis++)
    {
        if (edge[axis][0])
        {
            float best = find_closest(axis, r.min[axis], r, field, snap);
            if (!free && std::fabs(best - r.min[axis]) < snap)
                r.min[axis] = best;
            if (!outside_ok)
                r.min[axis] = std::max(r.min[axis], field.min[axis]);
            r.min[axis] = std::max(r.min[axis], r.max[axis] - hi[axis]);
            r.min[axis] = std::min(r.min[axis], r.max[axis] - lo[axis]);
        }
        if (edge[axis][1])
        {
            float best = find_closest(axis, r.max[axis], r, field, snap);
            if (!free && std::fabs(best - r.max[axis]) < snap)
                r.max[axis] = best;
            if (!outside_ok)
                r.max[axis] = std::min(r.max[axis], field.max[axis]);
            r.max[axis] = std::min(r.max[axis], r.min[axis] + hi[axis]);
            r.max[axis] = std::max(r.max[axis], r.min[axis] + lo[axis]);
        }
        data->DesiredSize[axis] = r.max[axis] - r.min[axis];
    }
}

/*
========================================================================

    DIBUJO

========================================================================
*/

/*
==================
editor_window::draw
==================
*/
void editor_window::draw(editor_layout* layout)
{
    ImGuiContext& g = *GImGui;
    const window_rect field = workspace();
    const window_rect* docked = layout ? layout->rect_of(this) : nullptr;

    dock_region = layout ? layout->region_of(this) : LAYOUT_FLOAT;

// acoplada y minimizada: la dibuja el layout como pestaña de su barra
    if (docked && minimized && dock_region != LAYOUT_CENTER)
    {
        rect = { ImVec2(0, 0), ImVec2(0, 0) };
        focused = false;
        return;
    }

    ImGuiWindowFlags flags = build_flags();

    // límites de tamaño. Las acopladas miden exactamente lo que les da el
    // layout (que ya respeta sus límites), así nunca cubren otra región
    ImVec2 size_min, size_max;
    get_size_limits(size_min, size_max);
    if (docked)
    {
        size_min = ImVec2(0, 0);
        size_max = ImVec2(FLT_MAX, FLT_MAX);
    }
    ImGui::SetNextWindowSizeConstraints(size_min, size_max, size_callback, this);
    dock_resizing = false;

    if (docked)
    {
// acoplada: el layout decide posición y tamaño. No se trae al frente al
// hacer clic para que las ventanas flotantes queden siempre encima.
// Se puede arrastrar por la barra de título: el layout la saca al pasar
// el umbral de arrastre (ver editor_layout::update_drag)
        ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
        ImGui::SetNextWindowPos(docked->min, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(docked->width(), docked->height()), ImGuiCond_Always);
        flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
        if (dock_region == LAYOUT_CENTER)
            flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
    }
    else
    {
// posición y tamaño iniciales (si editor.ini no tiene la ventana)
        const ImVec2 ws(field.width(), field.height());
        ImGui::SetNextWindowPos(ImVec2(field.min.x + default_pos.x * ws.x, field.min.y + default_pos.y * ws.y), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(default_size.x * ws.x, default_size.y * ws.y), ImGuiCond_FirstUseEver);
    }

// mover: ImGui ya aplicó el movimiento del ratón en NewFrame(), así que se
// corrige la posición antes de Begin() para que no haya un frame de retraso
    ImGuiWindow* w = docked ? nullptr : ImGui::FindWindowByName(imgui_id.c_str());
    if (w)
    {
        const bool moving = g.MovingWindow && g.MovingWindow->RootWindow == w;
        const bool other_active = g.ActiveId != 0 && !moving;   // p. ej. redimensionando

        window_rect r = { w->Pos, ImVec2(w->Pos.x + w->Size.x, w->Pos.y + w->Size.y) };
        window_rect target = r;

        if (moving && has_option(WND_SNAP) && !g.IO.KeyShift)
            snap_move(target, field);
        if (!other_active)
            if (!Wnd_CanLeaveMain())
                clamp_to_field(target, field);

        if (target.min.x != r.min.x || target.min.y != r.min.y)
            ImGui::SetNextWindowPos(target.min, ImGuiCond_Always);
        if (!w->Collapsed && (target.width() != r.width() || target.height() != r.height()))
            ImGui::SetNextWindowSize(ImVec2(target.width(), target.height()), ImGuiCond_Always);
    }

    // tamaño pedido al sacarla del layout
    if (!docked && request_size.x > 0 && request_size.y > 0)
    {
        ImGui::SetNextWindowSize(request_size, ImGuiCond_Always);
        request_size = ImVec2(0, 0);
    }

    if (request_collapse >= 0)
    {
        ImGui::SetNextWindowCollapsed(request_collapse == 1, ImGuiCond_Always);
        request_collapse = -1;
    }
    if (request_focus)
    {
        ImGui::SetNextWindowFocus();
        request_focus = false;
    }

    const bool was_open = open;
    // el tamaño mínimo global del estilo haría que una acoplada en un hueco
    // pequeño se saliera de él y tapara la región vecina
    if (docked)
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1, 1));
    const bool visible = ImGui::Begin(imgui_id.c_str(), has_option(WND_CLOSABLE) ? &open : nullptr, flags);
    if (docked)
        ImGui::PopStyleVar();

    // tamaño que necesita el contenido (mismo cálculo que el auto-ajuste de ImGui)
    {
        const ImGuiWindow* iw = ImGui::GetCurrentWindow();
        if (iw->ContentSizeIdeal.x > 0 && iw->ContentSizeIdeal.y > 0)
            content_size = ImVec2(
                iw->ContentSizeIdeal.x + iw->WindowPadding.x * 2.0f + iw->DecoOuterSizeX1 + iw->DecoOuterSizeX2 - iw->ScrollbarSizes.x,
                iw->ContentSizeIdeal.y + iw->WindowPadding.y * 2.0f + iw->DecoOuterSizeY1 + iw->DecoOuterSizeY2 - iw->ScrollbarSizes.y);
    }

    const bool collapsed = ImGui::IsWindowCollapsed();
    if (docked && dock_region != LAYOUT_CENTER)
    {
        // flecha de la barra de título o doble clic: en lugar de contraerse,
        // pasa a la barra del layout (y se deja sin contraer para cuando vuelva)
        if (collapsed)
        {
            minimized = true;
            request_collapse = 0;
        }
    }
    else
        minimized = collapsed;

    const ImVec2 pos = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    rect = { pos, ImVec2(pos.x + size.x, pos.y + size.y) };
    if (!docked && !collapsed)
        float_size = size;

    // borde interior arrastrado: el layout ajusta el tamaño de la región
    if (docked && dock_resizing)
        layout->resized(this, rect);

    const bool now_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (now_focused && !focused)
        on_focus();
    focused = now_focused;

    if (visible)
        on_draw();

    ImGui::End();

    // cerrada con el botón de la barra de título
    if (was_open && !open)
        on_hide();
}

/*
==================
editor_window::draw_all
==================
*/
void editor_window::draw_all()
{
    editor_layout* layout = editor_layout::active();
    if (layout)
    {
        layout->update_drag();          // antes de arrange: puede sacar o acoplar ventanas
        layout->arrange(workspace());
        layout->draw_bars();
    }

    for (editor_window* w : windex)
        if (w->open)
            w->draw(layout);

    if (layout)
        layout->draw_drop_zones();
}

/*
==================
editor_window::find
==================
*/
editor_window* editor_window::find(const char* name)
{
    for (editor_window* w : windex)
        if (!strcasecmp(w->name.c_str(), name))
            return w;
    return nullptr;
}

/*
========================================================================

    COMANDOS

========================================================================
*/

/*
==================
Wnd_Toggle_f

togglewindow <nombre>
==================
*/
static void Wnd_Toggle_f(void)
{
    if (Cmd_Argc() != 2)
    {
        Con_Printf("togglewindow <name> : show/hide an editor window\n");
        return;
    }

    editor_window* w = editor_window::find(Cmd_Argv(1));
    if (!w)
    {
        Con_Printf("togglewindow: window %s not found\n", Cmd_Argv(1));
        return;
    }

    // desde la consola el foco siempre lo tiene la consola, así que no se
    // usa toggle() (que primero trae al frente): se muestra u oculta directamente
    if (w->is_open())
        w->hide();
    else
        w->show();
}

/*
==================
Wnd_List_f
==================
*/
static void Wnd_List_f(void)
{
    const editor_layout* layout = editor_layout::active();
    for (const editor_window* w : editor_window::windex)
        Con_Printf("  %-12s %-8s %s%s\n", w->get_name(),
            editor_layout::region_name(layout ? layout->region_of(w) : LAYOUT_FLOAT),
            w->is_open() ? "abierta" : "oculta",
            w->is_minimized() ? ", minimizada" : "");
    Con_Printf("%i window(s)\n", (int)editor_window::windex.size());
}

/*
==================
editor_window::init
==================
*/
void editor_window::init()
{
    // como en QE3: las ventanas se arrastran solo por la barra de título
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;

    Cmd_AddCommand("togglewindow", Wnd_Toggle_f);
    Cmd_AddCommand("windowlist", Wnd_List_f);
}
