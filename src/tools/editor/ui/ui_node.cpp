// ui_node.cpp -- árbol de nodos de interfaz sobre Dear ImGui

#include "tools/editor/ui/ui_node.h"

#include <algorithm>

static int ui_next_uid = 1;

// hueco de alto h sin el espaciado entre elementos de ImGui
static void UI_Spacer(float w, float h)
{
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::Dummy(ImVec2(w, h));
    ImGui::PopStyleVar();
}

/*
==============================================================================

    ui_node

==============================================================================
*/

ui_node::ui_node() :
    uid(ui_next_uid++)
{
}

/*
==================
ui_node::draw
==================
*/
bool ui_node::draw()
{
    if (!props.visible)
    {
        hovered = false;
        return false;
    }

    if (props.id.empty())
        ImGui::PushID(uid);
    else
        ImGui::PushID(props.id.c_str());

    const bool fade = props.alpha < 1.0f;
    if (fade)
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * std::max(props.alpha, 0.0f));
    if (props.disabled)
        ImGui::BeginDisabled();

    // Se agrupa lo que dibuja varios elementos y lo que necesita un
    // rectángulo propio (márgenes, eventos): para ImGui pasa a ser un solo
    // elemento. Un control simple sin nada de eso se dibuja tal cual.
    const ui_margins& m = props.margin;
    const bool margins = !m.empty();
    const bool click = props.on_click && !handles_click();
    const bool events = click || props.on_hover || !props.tooltip.empty();
    const bool group = is_composite() || margins || events;

    if (group)
        ImGui::BeginGroup();
    if (margins)
    {
        if (m.top > 0)
            UI_Spacer(0, m.top);
        if (m.left > 0)
            ImGui::Indent(m.left);
    }

    render();

    if (margins)
    {
        if (m.left > 0)
            ImGui::Unindent(m.left);
        if (m.right > 0)
        {
            ImGui::SameLine(0.0f, 0.0f);
            UI_Spacer(m.right, 0);
        }
        if (m.bottom > 0)
            UI_Spacer(0, m.bottom);
    }
    if (group)
        ImGui::EndGroup();

    if (events)
    {
        const bool now = ImGui::IsItemHovered();
        if (now != hovered)
        {
            hovered = now;
            if (props.on_hover)
                props.on_hover(hovered);
        }
        if (!props.tooltip.empty())
            ImGui::SetItemTooltip("%s", props.tooltip.c_str());
        if (click && ImGui::IsItemClicked(ImGuiMouseButton_Left))
            props.on_click();
    }

    if (props.disabled)
        ImGui::EndDisabled();
    if (fade)
        ImGui::PopStyleVar();
    ImGui::PopID();
    return true;
}

/*
==============================================================================

    ui_parent

==============================================================================
*/

ui_parent::~ui_parent()
{
    for (const ui_ptr& child : children)
        child->parent = nullptr;
}

/*
==================
ui_parent::add_child
==================
*/
void ui_parent::add_child(ui_ptr child)
{
    if (!child || child.get() == this)
        return;

    if (child->parent)
    {
        ui_ptr keep = child;    // que no se destruya al quitarlo del padre anterior
        child->parent->remove_child(keep);
    }

    child->parent = this;
    children.push_back(std::move(child));
}

void ui_parent::add_children(std::initializer_list<ui_ptr> list)
{
    for (const ui_ptr& child : list)
        add_child(child);
}

/*
==================
ui_parent::remove_child
==================
*/
void ui_parent::remove_child(const ui_ptr& child)
{
    auto it = std::find(children.begin(), children.end(), child);
    if (it == children.end())
        return;

    ui_ptr removed = *it;
    children.erase(it);
    removed->parent = nullptr;
    on_child_removed(removed.get());
}

void ui_parent::clear_children()
{
    while (!children.empty())
        remove_child(children.back());
}

/*
==================
ui_parent::render
==================
*/
void ui_parent::render()
{
    // copia: un evento puede cambiar los hijos mientras se dibujan
    const std::vector<ui_ptr> list = children;
    for (const ui_ptr& child : list)
        child->draw();
}
