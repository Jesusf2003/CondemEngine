// ui_controls.cpp -- controles básicos

#include "tools/editor/ui/ui_controls.h"

#include "imgui_internal.h"     // SeparatorEx, RenderTextEllipsis
#include "misc/cpp/imgui_stdlib.h"

#include <algorithm>

/*
==================
ui_label::render
==================
*/
void ui_label::render()
{
    const std::string text = get_text();
    const bool colored = label_color.w > 0.0f;

    if (label_align)
        ImGui::AlignTextToFramePadding();
    if (colored)
        ImGui::PushStyleColor(ImGuiCol_Text, label_color);

    const ImVec2 text_size = label_ellipsis ? ImGui::CalcTextSize(text.c_str(), text.c_str() + text.size()) : ImVec2(0, 0);
    const float max_w = props.width > 0 ? props.width : ImGui::GetContentRegionAvail().x;

    if (label_ellipsis && !label_wrap && text_size.x > max_w && max_w > 0)
    {
        // como TextUnformatted pero de ancho max_w: el contenido declara el
        // ancho completo (CursorMaxPos/IdealMaxPos) para que el auto-ajuste
        // de la ventana o de la columna de una tabla lo sigan viendo
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        const ImVec2 pos(window->DC.CursorPos.x, window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);
        const ImVec2 size(max_w, text_size.y);

        window->DC.CursorMaxPos.x = std::max(window->DC.CursorMaxPos.x, pos.x + text_size.x);
        window->DC.IdealMaxPos.x = std::max(window->DC.IdealMaxPos.x, pos.x + text_size.x);
        const float backup_max_x = window->DC.CursorMaxPos.x;
        ImGui::ItemSize(size, 0.0f);
        window->DC.CursorMaxPos.x = backup_max_x;

        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        if (ImGui::ItemAdd(bb, 0))
        {
            ImGui::RenderTextEllipsis(window->DrawList, bb.Min, bb.Max, bb.Max.x,
                text.c_str(), text.c_str() + text.size(), &text_size);
            if (props.tooltip.empty() && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", text.c_str());
        }
    }
    else if (label_wrap)
    {
        ImGui::PushTextWrapPos(props.width > 0 ? ImGui::GetCursorPosX() + props.width : 0.0f);
        ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());
        ImGui::PopTextWrapPos();
    }
    else
        ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());

    if (colored)
        ImGui::PopStyleColor();
}

/*
==================
ui_button::render
==================
*/
void ui_button::render()
{
    const bool pressed = small_button ? ImGui::SmallButton(label.c_str())
                                      : ImGui::Button(label.c_str(), item_size());
    if (pressed)
        fire_click();
}

/*
==================
ui_separator::render
==================
*/
void ui_separator::render()
{
    if (is_vertical)
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    else if (!title.empty())
        ImGui::SeparatorText(title.c_str());
    else
        ImGui::Separator();
}

/*
==============================================================================

    ui_checkbox

==============================================================================
*/

bool ui_checkbox::is_checked() const
{
    if (value_ptr)
        return *value_ptr;
    if (getter)
        return getter();
    return value;
}

void ui_checkbox::render()
{
    bool v = is_checked();
    if (!ImGui::Checkbox(label.c_str(), &v))
        return;

    if (value_ptr)
        *value_ptr = v;
    else if (setter)
        setter(v);
    else
        value = v;

    if (changed)
        changed(v);
    fire_click();
}

/*
==============================================================================

    ui_text_input

==============================================================================
*/

const char* ui_text_input::get_text() const
{
    if (buffer)
        return buffer;
    if (string_ptr)
        return string_ptr->c_str();
    return value.c_str();
}

void ui_text_input::render()
{
    ImGuiInputTextFlags f = input_flags;
    if (submitted)
        f |= ImGuiInputTextFlags_EnterReturnsTrue;

    if (props.width != 0)
        ImGui::SetNextItemWidth(props.width);

    const char* hint = hint_text.c_str();
    bool entered;
    if (buffer)
        entered = ImGui::InputTextWithHint("##text", hint, buffer, buffer_size, f);
    else
        entered = ImGui::InputTextWithHint("##text", hint, string_ptr ? string_ptr : &value, f);

    if (changed && ImGui::IsItemEdited())
        changed(get_text());
    if (submitted && entered)
        submitted(get_text());
}

/*
==================
ui_popup_button::render
==================
*/
void ui_popup_button::render()
{
    const bool pressed = small_button ? ImGui::SmallButton(label.c_str()) : ImGui::Button(label.c_str(), item_size());
    if (pressed)
        ImGui::OpenPopup("##popup");

    if (ImGui::BeginPopup("##popup"))
    {
        ui_parent::render();
        ImGui::EndPopup();
    }
}

/*
==============================================================================

    Menús

==============================================================================
*/

void ui_menu_bar::render()
{
    if (main_bar)
    {
        if (!ImGui::BeginMainMenuBar())
            return;
        ui_parent::render();
        ImGui::EndMainMenuBar();
    }
    else
    {
        if (!ImGui::BeginMenuBar())
            return;
        ui_parent::render();
        ImGui::EndMenuBar();
    }
}

void ui_menu::render()
{
    if (!ImGui::BeginMenu(label.c_str()))
        return;
    ui_parent::render();
    ImGui::EndMenu();
}

void ui_menu_item::render()
{
    const char* sc = shortcut_text.empty() ? nullptr : shortcut_text.c_str();
    const bool checkable = value_ptr || getter;

    bool selected = false;
    if (value_ptr)
        selected = *value_ptr;
    else if (getter)
        selected = getter();

    if (!ImGui::MenuItem(label.c_str(), sc, checkable ? &selected : nullptr))
        return;

    if (value_ptr)
        *value_ptr = selected;
    else if (setter)
        setter(selected);
    fire_click();
}
