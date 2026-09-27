// ui_property_card.cpp -- tarjeta de propiedades (nombre: valor)

#include "tools/editor/ui/ui_property_card.h"

#include "tools/editor/style.h"

#include <algorithm>

/*
==================
ui_property_card
==================
*/
ui_property_card::ui_property_card(std::string title) :
    title_label(ui_label::create(std::move(title))),
    grid(ui_grid_pane::create()->gap(16, 2))
{
    title_label->visible(!title_label->get_text().empty());
}

ui_property_card::ptr ui_property_card::title(const std::string& v)
{
    title_label->text(v)->visible(!v.empty());
    return self();
}

ui_property_card::ptr ui_property_card::property(const std::string& name, const std::string& value)
{
    return add_row(name, ui_label::create(value));
}

ui_property_card::ptr ui_property_card::property(const std::string& name, ui_label::text_fn value)
{
    return add_row(name, ui_label::create(std::move(value)));
}

ui_property_card::ptr ui_property_card::add_row(const std::string& name, ui_label::ptr value)
{
    grid->add(ui_label::create(name)->color(ed_style.text_disabled), 0, rows);
    grid->add(value, 1, rows);
    rows++;
    return self();
}

/*
==================
ui_property_card::render

El fondo se dibuja antes que el contenido con el tamaño del frame anterior,
así queda debajo sin partir la lista de dibujado en canales.
==================
*/
void ui_property_card::render()
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float rounding = ImGui::GetStyle().FrameRounding;

    if (last_size.x > 0 && last_size.y > 0)
    {
        const ImVec2 end(origin.x + last_size.x, origin.y + last_size.y);
        dl->AddRectFilled(origin, end, ImGui::GetColorU32(ed_style.frame_bg), rounding);
        dl->AddRect(origin, end, ImGui::GetColorU32(ed_style.border), rounding);
    }

    // contenido con margen interior
    const float pad_y = std::max(card_padding - ImGui::GetStyle().ItemSpacing.y, 0.0f);
    ImGui::BeginGroup();
    ImGui::Dummy(ImVec2(0, pad_y));
    if (card_padding > 0)
        ImGui::Indent(card_padding);     // Indent(0) usaría el sangrado del estilo

    if (title_label->is_visible())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ed_style.accent);
        title_label->draw();
        ImGui::PopStyleColor();
    }
    grid->draw();

    if (card_padding > 0)
        ImGui::Unindent(card_padding);
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::Dummy(ImVec2(card_padding, 0));
    ImGui::Dummy(ImVec2(0, pad_y));
    ImGui::EndGroup();

    last_size = ImGui::GetItemRectSize();
}
