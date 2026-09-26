// layout.cpp -- distribución de ventanas en la capa principal del editor

#include "imgui_internal.h"     // antes que imgui.h: GImGui, ImGuiSettingsHandler, MarkIniSettingsDirty

#include "tools/editor/layout.h"
#include "tools/editor/style.h"

#include "core/cmd.h"
#include "engine/console.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
    #define strcasecmp _stricmp
#else
    #include <strings.h>
#endif

std::vector<editor_layout*> editor_layout::layouts;
editor_layout*              editor_layout::active_layout;

static const char* layout_region_names[LAYOUT_REGION_COUNT] =
{
    "float", "left", "right", "top", "bottom", "center"
};

static const window_rect layout_empty_rect = { ImVec2(0, 0), ImVec2(0, 0) };

static bool Layout_Contains(const window_rect& r, const ImVec2& p)
{
    return p.x >= r.min.x && p.y >= r.min.y && p.x < r.max.x && p.y < r.max.y;
}

/*
==================
editor_layout
==================
*/
editor_layout::editor_layout(const char* name) :
    name(name),
    workspace(layout_empty_rect), inner(layout_empty_rect),
    drag_window(nullptr), drop_region(LAYOUT_FLOAT)
{
    size[LAYOUT_FLOAT]  = 0.0f;
    size[LAYOUT_LEFT]   = 0.20f;
    size[LAYOUT_RIGHT]  = 0.20f;
    size[LAYOUT_TOP]    = 0.15f;
    size[LAYOUT_BOTTOM] = 0.30f;
    size[LAYOUT_CENTER] = 0.0f;     // el centro ocupa lo que sobra

    for (int r = 0; r < LAYOUT_REGION_COUNT; r++)
    {
        bar_rect[r] = layout_empty_rect;
        region_area[r] = layout_empty_rect;
        occupied[r] = false;
        region_min[r] = 0.0f;
        region_max[r] = FLT_MAX;
    }

    layouts.push_back(this);
    if (!active_layout)
        active_layout = this;
}

/*
==================
~editor_layout
==================
*/
editor_layout::~editor_layout()
{
    layouts.erase(std::find(layouts.begin(), layouts.end(), this));
    if (active_layout == this)
        active_layout = layouts.empty() ? nullptr : layouts.front();
}

/*
========================================================================

    ASIGNACIÓN Y TAMAÑOS

========================================================================
*/

editor_layout::slot* editor_layout::find_slot(const editor_window* window)
{
    for (slot& s : slots)
        if (s.window == window)
            return &s;
    return nullptr;
}

const editor_layout::slot* editor_layout::find_slot(const editor_window* window) const
{
    for (const slot& s : slots)
        if (s.window == window)
            return &s;
    return nullptr;
}

/*
==================
editor_layout::dock
==================
*/
void editor_layout::dock(editor_window* window, layout_region region)
{
    if (region <= LAYOUT_FLOAT || region >= LAYOUT_REGION_COUNT)
    {
        undock(window);
        return;
    }

    if (slot* s = find_slot(window))
        s->region = region;
    else
        slots.push_back({ window, region, layout_empty_rect });

    // entra restaurada aunque viniera contraída como flotante
    window->minimized = false;
    window->request_collapse = 0;

    if (ImGui::GetCurrentContext())
        ImGui::MarkIniSettingsDirty();
}

/*
==================
editor_layout::undock
==================
*/
void editor_layout::undock(editor_window* window)
{
    for (auto it = slots.begin(); it != slots.end(); ++it)
    {
        if (it->window == window)
        {
            slots.erase(it);
            window->minimized = false;
            break;
        }
    }

    if (ImGui::GetCurrentContext())
        ImGui::MarkIniSettingsDirty();
}

/*
==================
editor_layout::region_of
==================
*/
layout_region editor_layout::region_of(const editor_window* window) const
{
    const slot* s = find_slot(window);
    return s ? s->region : LAYOUT_FLOAT;
}

/*
==================
editor_layout::set_size / get_size
==================
*/
void editor_layout::set_size(layout_region region, float fraction)
{
    if (region <= LAYOUT_FLOAT || region >= LAYOUT_CENTER)
        return;     // el centro no tiene tamaño propio
    size[region] = std::min(std::max(fraction, 0.0f), 1.0f);
}

float editor_layout::get_size(layout_region region) const
{
    if (region <= LAYOUT_FLOAT || region >= LAYOUT_REGION_COUNT)
        return 0.0f;
    return size[region];
}

/*
========================================================================

    DISTRIBUCIÓN

========================================================================
*/

/*
==================
Layout_Fit

Reduce a y b proporcionalmente para que sumen como mucho avail
==================
*/
static void Layout_Fit(float& a, float& b, float avail)
{
    avail = std::max(avail, 0.0f);
    if (a + b > avail && a + b > 0.0f)
    {
        float f = avail / (a + b);
        a *= f;
        b *= f;
    }
}

/*
==================
Layout_Distribute

Reparte total entre elementos con límites [lim.x, lim.y]: a partes iguales,
pero sin pasar del máximo de ninguno ni bajar de su mínimo. Si los mínimos no
caben se reducen en proporción (nunca se invade otra región). Si todos llegan
a su máximo, sobra espacio al final
==================
*/
static std::vector<float> Layout_Distribute(float total, const std::vector<ImVec2>& lim)
{
    const size_t n = lim.size();
    std::vector<float> out(n, 0.0f);
    if (!n)
        return out;

    float sum_min = 0.0f;
    for (const ImVec2& l : lim)
        sum_min += l.x;
    if (sum_min >= total)
    {
        for (size_t i = 0; i < n; i++)
            out[i] = sum_min > 0.0f ? lim[i].x * total / sum_min : total / n;
        return out;
    }

    std::vector<bool> fixed(n, false);
    float remaining = total;
    size_t free = n;
    while (free > 0)
    {
        const float share = remaining / free;
        bool changed = false;

        for (size_t i = 0; i < n; i++)
        {
            if (!fixed[i] && lim[i].y < share)
            {
                out[i] = lim[i].y;
                fixed[i] = true;
                remaining -= out[i];
                free--;
                changed = true;
            }
        }
        if (changed)
            continue;

        for (size_t i = 0; i < n; i++)
        {
            if (!fixed[i] && lim[i].x > share)
            {
                out[i] = lim[i].x;
                fixed[i] = true;
                remaining -= out[i];
                free--;
                changed = true;
            }
        }
        if (changed)
            continue;

        for (size_t i = 0; i < n; i++)
            if (!fixed[i])
                out[i] = share;
        break;
    }
    return out;
}

/*
==================
editor_layout::region_limits

Grosor mínimo y máximo de una región según sus ventanas: el mínimo es el
mayor de los mínimos y el máximo el mayor de los máximos (para que quepa la
que más necesita). Sin ventanas con máximo, no hay máximo
==================
*/
void editor_layout::region_limits(const window_list& list, int axis, float& lo, float& hi) const
{
    lo = ed_style.scaled(ed_style.layout_min_region);
    hi = 0.0f;
    for (const editor_window* w : list)
    {
        ImVec2 wmin, wmax;
        w->get_layout_limits(wmin, wmax);
        lo = std::max(lo, wmin[axis]);
        hi = std::max(hi, wmax[axis]);
    }
    if (list.empty())
        hi = FLT_MAX;
    hi = std::max(hi, lo);
}

/*
==================
editor_layout::compute_areas

Rectángulo de cada región dentro de area según las ventanas que tiene.
lo/hi reciben los límites de grosor de cada región
==================
*/
void editor_layout::compute_areas(const window_rect& area, const window_list lists[], window_rect areas[], float lo[], float hi[]) const
{
    const float W = area.width();
    const float H = area.height();
    const float min_center = ed_style.scaled(ed_style.layout_min_center);

    bool occ[LAYOUT_REGION_COUNT];
    for (int r = 0; r < LAYOUT_REGION_COUNT; r++)
    {
        occ[r] = !lists[r].empty();
        lo[r] = 0.0f;
        hi[r] = FLT_MAX;
    }
    region_limits(lists[LAYOUT_LEFT],   0, lo[LAYOUT_LEFT],   hi[LAYOUT_LEFT]);
    region_limits(lists[LAYOUT_RIGHT],  0, lo[LAYOUT_RIGHT],  hi[LAYOUT_RIGHT]);
    region_limits(lists[LAYOUT_TOP],    1, lo[LAYOUT_TOP],    hi[LAYOUT_TOP]);
    region_limits(lists[LAYOUT_BOTTOM], 1, lo[LAYOUT_BOTTOM], hi[LAYOUT_BOTTOM]);

    auto thickness = [&](layout_region r, float total) -> float
    {
        if (!occ[r])
            return 0.0f;
        return std::min(std::max(size[r] * total, lo[r]), std::min(hi[r], total));
    };

    const bool middle_used = occ[LAYOUT_LEFT] || occ[LAYOUT_RIGHT] || occ[LAYOUT_CENTER];

    // si no cabe todo, las regiones se reducen: nunca se solapan
    float top = thickness(LAYOUT_TOP, H);
    float bottom = thickness(LAYOUT_BOTTOM, H);
    Layout_Fit(top, bottom, H - (middle_used ? min_center : 0.0f));

    float left = thickness(LAYOUT_LEFT, W);
    float right = thickness(LAYOUT_RIGHT, W);
    Layout_Fit(left, right, W - (occ[LAYOUT_CENTER] ? min_center : 0.0f));

    const float y0 = std::floor(area.min.y + top);
    const float y1 = std::floor(area.max.y - bottom);
    const float x0 = std::floor(area.min.x + left);
    const float x1 = std::floor(area.max.x - right);

    areas[LAYOUT_FLOAT]  = layout_empty_rect;
    areas[LAYOUT_TOP]    = { area.min, ImVec2(area.max.x, y0) };
    areas[LAYOUT_BOTTOM] = { ImVec2(area.min.x, y1), area.max };
    areas[LAYOUT_LEFT]   = { ImVec2(area.min.x, y0), ImVec2(x0, y1) };
    areas[LAYOUT_RIGHT]  = { ImVec2(x1, y0), ImVec2(area.max.x, y1) };
    areas[LAYOUT_CENTER] = { ImVec2(x0, y0), ImVec2(x1, y1) };
}

/*
==================
editor_layout::stack

Reparte el área de una región entre sus ventanas (en vertical en left/right/
center, en horizontal en top/bottom) respetando sus límites de tamaño. En el
otro eje cada ventana ocupa el grosor de la región, salvo que su máximo sea
menor: entonces se pega al borde exterior
==================
*/
void editor_layout::stack(std::vector<slot*>& list, const window_rect& area, layout_region region)
{
    if (list.empty())
        return;

    const int axis = (region == LAYOUT_TOP || region == LAYOUT_BOTTOM) ? 0 : 1;
    const int other = axis ^ 1;
    const bool anchor_max = (region == LAYOUT_RIGHT || region == LAYOUT_BOTTOM);
    const float thick = area.max[other] - area.min[other];

    std::vector<ImVec2> lim;
    std::vector<float> max_other;
    for (slot* s : list)
    {
        ImVec2 wmin, wmax;
        s->window->get_layout_limits(wmin, wmax);
        lim.push_back(ImVec2(wmin[axis], wmax[axis]));
        max_other.push_back(wmax[other]);
    }
    const std::vector<float> len = Layout_Distribute(area.max[axis] - area.min[axis], lim);

    float pos = area.min[axis];
    for (size_t i = 0; i < list.size(); i++)
    {
        slot* s = list[i];
        const float t = std::min(thick, max_other[i]);

        s->rect.min[axis] = std::floor(pos);
        s->rect.max[axis] = std::floor(pos + len[i]);
        s->rect.min[other] = anchor_max ? area.max[other] - t : area.min[other];
        s->rect.max[other] = s->rect.min[other] + t;
        pos += len[i];
    }
}

/*
==================
editor_layout::arrange
==================
*/
void editor_layout::arrange(const window_rect& ws)
{
    workspace = ws;

// ventanas abiertas de cada región; las minimizadas van a la barra de su borde
    std::vector<slot*> list[LAYOUT_REGION_COUNT];
    window_list windows[LAYOUT_REGION_COUNT];
    for (int r = 0; r < LAYOUT_REGION_COUNT; r++)
    {
        bar_windows[r].clear();
        bar_rect[r] = layout_empty_rect;
    }
    for (slot& s : slots)
    {
        if (!s.window->is_open())
            continue;
        if (s.window->is_minimized() && s.region != LAYOUT_CENTER)
            bar_windows[s.region].push_back(s.window);
        else
        {
            list[s.region].push_back(&s);
            windows[s.region].push_back(s.window);
        }
    }

// barras en los bordes exteriores: top y bottom a todo el ancho,
// left y right entre ellas
    const float bar = ImGui::GetFrameHeight();
    window_rect area = ws;
    if (!bar_windows[LAYOUT_TOP].empty())
    {
        bar_rect[LAYOUT_TOP] = { area.min, ImVec2(area.max.x, area.min.y + bar) };
        area.min.y += bar;
    }
    if (!bar_windows[LAYOUT_BOTTOM].empty())
    {
        bar_rect[LAYOUT_BOTTOM] = { ImVec2(area.min.x, area.max.y - bar), area.max };
        area.max.y -= bar;
    }
    if (!bar_windows[LAYOUT_LEFT].empty())
    {
        bar_rect[LAYOUT_LEFT] = { area.min, ImVec2(area.min.x + bar, area.max.y) };
        area.min.x += bar;
    }
    if (!bar_windows[LAYOUT_RIGHT].empty())
    {
        bar_rect[LAYOUT_RIGHT] = { ImVec2(area.max.x - bar, area.min.y), area.max };
        area.max.x -= bar;
    }
    inner = area;

// regiones
    compute_areas(inner, windows, region_area, region_min, region_max);
    for (int r = 0; r < LAYOUT_REGION_COUNT; r++)
        occupied[r] = !windows[r].empty();

    for (int r = LAYOUT_LEFT; r < LAYOUT_REGION_COUNT; r++)
        stack(list[r], region_area[r], (layout_region)r);
}

/*
==================
editor_layout::rect_of
==================
*/
const window_rect* editor_layout::rect_of(const editor_window* window) const
{
    const slot* s = find_slot(window);
    return s ? &s->rect : nullptr;
}

/*
==================
editor_layout::extent_limits

Grosor que puede tener la región de la ventana. El máximo es el menor entre
el de sus ventanas y el espacio que dejan libre las otras regiones ocupadas
(y el mínimo del centro), así una región nunca cubre ni achica a otra
==================
*/
void editor_layout::extent_limits(const editor_window* window, float& lo, float& hi) const
{
    lo = 0.0f;
    hi = FLT_MAX;

    const slot* s = find_slot(window);
    if (!s || s->region <= LAYOUT_FLOAT || s->region >= LAYOUT_CENTER)
        return;

    const layout_region r = s->region;
    const float min_center = ed_style.scaled(ed_style.layout_min_center);
    const bool middle_used = occupied[LAYOUT_LEFT] || occupied[LAYOUT_RIGHT] || occupied[LAYOUT_CENTER];

    float avail;
    switch (r)
    {
    case LAYOUT_LEFT:
        avail = inner.width() - (occupied[LAYOUT_RIGHT] ? region_area[LAYOUT_RIGHT].width() : 0.0f)
            - (occupied[LAYOUT_CENTER] ? min_center : 0.0f);
        break;
    case LAYOUT_RIGHT:
        avail = inner.width() - (occupied[LAYOUT_LEFT] ? region_area[LAYOUT_LEFT].width() : 0.0f)
            - (occupied[LAYOUT_CENTER] ? min_center : 0.0f);
        break;
    case LAYOUT_TOP:
        avail = inner.height() - (occupied[LAYOUT_BOTTOM] ? region_area[LAYOUT_BOTTOM].height() : 0.0f)
            - (middle_used ? min_center : 0.0f);
        break;
    default:    // LAYOUT_BOTTOM
        avail = inner.height() - (occupied[LAYOUT_TOP] ? region_area[LAYOUT_TOP].height() : 0.0f)
            - (middle_used ? min_center : 0.0f);
        break;
    }

    hi = std::max(0.0f, std::min(region_max[r], avail));
    lo = std::min(region_min[r], hi);
}

/*
==================
editor_layout::resized

El usuario arrastró el borde interior de una ventana acoplada
==================
*/
void editor_layout::resized(const editor_window* window, const window_rect& r)
{
    const slot* s = find_slot(window);
    if (!s || inner.width() <= 0 || inner.height() <= 0)
        return;

    float extent, total;
    switch (s->region)
    {
    case LAYOUT_LEFT:
    case LAYOUT_RIGHT:
        extent = r.width();
        total = inner.width();
        break;
    case LAYOUT_TOP:
    case LAYOUT_BOTTOM:
        extent = r.height();
        total = inner.height();
        break;
    default:
        return;
    }

    float lo, hi;
    extent_limits(window, lo, hi);
    extent = std::min(std::max(extent, lo), hi);

    const float fraction = extent / total;
    if (std::fabs(fraction - size[s->region]) > 0.0001f)
    {
        size[s->region] = fraction;
        ImGui::MarkIniSettingsDirty();
    }
}

/*
========================================================================

    BARRAS DE VENTANAS MINIMIZADAS

========================================================================
*/

/*
==================
Layout_AddVerticalText

Dibuja el texto girado 90 grados. ccw: se lee de abajo arriba (barra
izquierda); si no, de arriba abajo (barra derecha). origin es el punto
donde empieza la línea base del texto antes de girarlo
==================
*/
static void Layout_AddVerticalText(ImDrawList* dl, const ImVec2& origin, ImU32 col, const char* text, bool ccw)
{
    // sin recorte mientras se genera: el texto horizontal se sale de la barra
    dl->PushClipRect(ImVec2(-8192.0f, -8192.0f), ImVec2(8192.0f, 8192.0f), false);
    const int start = dl->VtxBuffer.Size;
    dl->AddText(origin, col, text);
    for (int i = start; i < dl->VtxBuffer.Size; i++)
    {
        ImDrawVert& v = dl->VtxBuffer[i];
        const float x = v.pos.x - origin.x;
        const float y = v.pos.y - origin.y;
        v.pos = ccw ? ImVec2(origin.x + y, origin.y - x) : ImVec2(origin.x - y, origin.y + x);
    }
    dl->PopClipRect();
}

/*
==================
editor_layout::draw_bar

Una pestaña por ventana minimizada; al hacer clic se restaura
==================
*/
void editor_layout::draw_bar(layout_region region)
{
    const window_rect& b = bar_rect[region];
    const bool vertical = (region == LAYOUT_LEFT || region == LAYOUT_RIGHT);
    const float thick = vertical ? b.width() : b.height();
    const float pad = ed_style.scaled(ed_style.layout_bar_padding);
    const float gap = std::max(1.0f, ed_style.scaled(1.0f));
    const float line = std::max(1.0f, ed_style.scaled(2.0f));

    char id[32];
    snprintf(id, sizeof(id), "##bar_%s", region_name(region));

    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    ImGui::SetNextWindowPos(b.min, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(b.width(), b.height()), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1, 1));    // la barra mide una línea
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ed_style.bar_bg);

    ImGui::Begin(id, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav
        | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 text_col = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 accent = ImGui::ColorConvertFloat4ToU32(ed_style.accent);
    ImVec2 cursor = b.min;

    editor_window* restore = nullptr;
    for (editor_window* w : bar_windows[region])
    {
        const char* label = w->get_title();
        const ImVec2 ts = ImGui::CalcTextSize(label);
        const float len = ts.x + pad * 2.0f;
        const ImVec2 tab_size = vertical ? ImVec2(thick, len) : ImVec2(len, thick);
        const ImVec2 p0 = cursor;
        const ImVec2 p1 = ImVec2(cursor.x + tab_size.x, cursor.y + tab_size.y);

        ImGui::SetCursorScreenPos(p0);
        ImGui::PushID(w);
        if (ImGui::InvisibleButton("##tab", tab_size))
            restore = w;
        const bool hovered = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();
        if (hovered)
            ImGui::SetTooltip("Restaurar %s", label);
        ImGui::PopID();

        dl->AddRectFilled(p0, p1, ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));

        // línea de acento en el lado que da al centro
        switch (region)
        {
        case LAYOUT_BOTTOM: dl->AddRectFilled(p0, ImVec2(p1.x, p0.y + line), accent); break;
        case LAYOUT_TOP:    dl->AddRectFilled(ImVec2(p0.x, p1.y - line), p1, accent); break;
        case LAYOUT_LEFT:   dl->AddRectFilled(ImVec2(p1.x - line, p0.y), p1, accent); break;
        case LAYOUT_RIGHT:  dl->AddRectFilled(p0, ImVec2(p0.x + line, p1.y), accent); break;
        default: break;
        }

        if (!vertical)
            dl->AddText(ImVec2(p0.x + pad, p0.y + (thick - ts.y) * 0.5f), text_col, label);
        else if (region == LAYOUT_LEFT)
            Layout_AddVerticalText(dl, ImVec2(p0.x + (thick - ts.y) * 0.5f, p1.y - pad), text_col, label, true);
        else
            Layout_AddVerticalText(dl, ImVec2(p0.x + (thick + ts.y) * 0.5f, p0.y + pad), text_col, label, false);

        if (vertical)
            cursor.y += len + gap;
        else
            cursor.x += len + gap;
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(5);

    if (restore)
    {
        restore->restore();
        restore->focus();
    }
}

/*
==================
editor_layout::draw_bars
==================
*/
void editor_layout::draw_bars()
{
    for (int r = LAYOUT_LEFT; r < LAYOUT_CENTER; r++)
        if (!bar_windows[r].empty())
            draw_bar((layout_region)r);
}

/*
========================================================================

    ARRASTRAR Y SOLTAR

========================================================================
*/

/*
==================
editor_layout::center_zone

Zona para soltar en el centro: un cuadrado en medio del área de trabajo
==================
*/
window_rect editor_layout::center_zone() const
{
    const float half = ed_style.scaled(ed_style.layout_drop_center);
    const ImVec2 c((workspace.min.x + workspace.max.x) * 0.5f, (workspace.min.y + workspace.max.y) * 0.5f);
    return { ImVec2(c.x - half, c.y - half), ImVec2(c.x + half, c.y + half) };
}

/*
==================
editor_layout::zone_at

Región en la que se acoplaría una ventana soltada en p
==================
*/
layout_region editor_layout::zone_at(const ImVec2& p) const
{
    // franja de ed_style.layout_drop_edge px a cada lado del borde, para que
    // también acople si el cursor se pasa un poco fuera del área de trabajo
    const float edge = ed_style.scaled(ed_style.layout_drop_edge);
    const window_rect& ws = workspace;

    const bool in_x = p.x > ws.min.x - edge && p.x < ws.max.x + edge;
    const bool in_y = p.y > ws.min.y - edge && p.y < ws.max.y + edge;
    if (!in_x || !in_y)
        return LAYOUT_FLOAT;

    if (p.x < ws.min.x + edge)
        return LAYOUT_LEFT;
    if (p.x > ws.max.x - edge)
        return LAYOUT_RIGHT;
    if (p.y < ws.min.y + edge)
        return LAYOUT_TOP;
    if (p.y > ws.max.y - edge)
        return LAYOUT_BOTTOM;
    if (Layout_Contains(center_zone(), p))
        return LAYOUT_CENTER;
    return LAYOUT_FLOAT;
}

/*
==================
editor_layout::preview

Área que ocuparía la región si se acoplara ahí la ventana arrastrada
==================
*/
window_rect editor_layout::preview(layout_region region) const
{
    if (region <= LAYOUT_FLOAT || region >= LAYOUT_REGION_COUNT)
        return layout_empty_rect;

    window_list lists[LAYOUT_REGION_COUNT];
    for (const slot& sl : slots)
        if (sl.window->is_open() && !(sl.window->is_minimized() && sl.region != LAYOUT_CENTER))
            lists[sl.region].push_back(sl.window);
    if (drag_window)
        lists[region].push_back(drag_window);

    window_rect areas[LAYOUT_REGION_COUNT];
    float lo[LAYOUT_REGION_COUNT], hi[LAYOUT_REGION_COUNT];
    compute_areas(inner, lists, areas, lo, hi);
    return areas[region];
}

/*
==================
editor_layout::undock_for_drag

Saca una ventana acoplada que se empieza a arrastrar y la deja flotante con
un tamaño razonable, manteniendo el punto agarrado dentro de su barra de título
==================
*/
void editor_layout::undock_for_drag(editor_window* window)
{
    ImGuiContext& g = *GImGui;
    const window_rect* r = rect_of(window);
    const ImVec2 docked_size = r ? ImVec2(r->width(), r->height()) : ImVec2(0, 0);

    undock(window);

    ImVec2 fs = window->float_size;
    if (fs.x <= 0 || fs.y <= 0)
        fs = ImVec2(std::min(docked_size.x, workspace.width() * 0.4f), std::min(docked_size.y, workspace.height() * 0.4f));
    ImVec2 lo, hi;
    window->get_size_limits(lo, hi);
    fs = ImVec2(std::min(std::max(fs.x, lo.x), hi.x), std::min(std::max(fs.y, lo.y), hi.y));
    window->request_size = fs;

    // ImGui mueve la ventana con pos = ratón - ActiveIdClickOffset
    if (g.ActiveIdClickOffset.x > fs.x - ed_style.scaled(24.0f))
        g.ActiveIdClickOffset.x = fs.x * 0.5f;
}

/*
==================
editor_layout::update_drag
==================
*/
void editor_layout::update_drag()
{
    ImGuiContext& g = *GImGui;

// ventana del editor que ImGui está moviendo
    editor_window* moving = nullptr;
    if (g.MovingWindow)
    {
        const char* id = g.MovingWindow->RootWindow->Name;
        for (editor_window* w : editor_window::windex)
            if (!strcmp(w->get_imgui_id(), id))
                moving = w;
    }

    if (moving)
    {
        // acoplada: sale del layout al pasar el umbral de arrastre
        if (find_slot(moving))
        {
            if (!ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Left, g.IO.MouseDragThreshold * 2.0f))
                return;
            undock_for_drag(moving);
        }

        // flotante: zona bajo el ratón (Mayús: soltar sin acoplar)
        drag_window = moving;
        drop_region = g.IO.KeyShift ? LAYOUT_FLOAT : zone_at(g.IO.MousePos);
        return;
    }

// se soltó la ventana que se arrastraba
    if (drag_window)
    {
        const bool alive = std::find(editor_window::windex.begin(), editor_window::windex.end(), drag_window) != editor_window::windex.end();
        if (alive && drop_region != LAYOUT_FLOAT)
            dock(drag_window, drop_region);
        drag_window = nullptr;
        drop_region = LAYOUT_FLOAT;
    }
}

/*
==================
editor_layout::draw_drop_zones
==================
*/
void editor_layout::draw_drop_zones()
{
    if (!drag_window)
        return;

    ImDrawList* fg = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
    const ImU32 zone = ImGui::ColorConvertFloat4ToU32(ed_style.drop_zone);
    const ImU32 preview_fill = ImGui::ColorConvertFloat4ToU32(ed_style.drop_preview);
    const ImU32 accent = ImGui::ColorConvertFloat4ToU32(ed_style.accent);
    const float edge = ed_style.scaled(ed_style.layout_drop_edge);
    const float rounding = ed_style.scaled(ed_style.window_rounding);
    const float border = std::max(1.0f, ed_style.scaled(2.0f));
    const window_rect& ws = workspace;

// zonas
    fg->AddRectFilled(ws.min, ImVec2(ws.min.x + edge, ws.max.y), zone);
    fg->AddRectFilled(ImVec2(ws.max.x - edge, ws.min.y), ws.max, zone);
    fg->AddRectFilled(ImVec2(ws.min.x + edge, ws.min.y), ImVec2(ws.max.x - edge, ws.min.y + edge), zone);
    fg->AddRectFilled(ImVec2(ws.min.x + edge, ws.max.y - edge), ImVec2(ws.max.x - edge, ws.max.y), zone);
    const window_rect c = center_zone();
    fg->AddRectFilled(c.min, c.max, zone, rounding);
    fg->AddRect(c.min, c.max, accent, rounding, 0, border * 0.5f);

// dónde quedaría
    if (drop_region != LAYOUT_FLOAT)
    {
        const window_rect p = preview(drop_region);
        fg->AddRectFilled(p.min, p.max, preview_fill, rounding);
        fg->AddRect(p.min, p.max, accent, rounding, 0, border);
    }
}

/*
========================================================================

    TODOS LOS LAYOUTS

========================================================================
*/

void editor_layout::set_active(editor_layout* layout)
{
    active_layout = layout;
}

editor_layout* editor_layout::find(const char* name)
{
    for (editor_layout* l : layouts)
        if (!strcasecmp(l->name.c_str(), name))
            return l;
    return nullptr;
}

const char* editor_layout::region_name(layout_region region)
{
    if (region < 0 || region >= LAYOUT_REGION_COUNT)
        return "?";
    return layout_region_names[region];
}

layout_region editor_layout::parse_region(const char* name)
{
    for (int i = 0; i < LAYOUT_REGION_COUNT; i++)
        if (!strcasecmp(name, layout_region_names[i]))
            return (layout_region)i;
    return LAYOUT_REGION_COUNT;
}

/*
========================================================================

    GUARDADO EN EDITOR.INI

    [Layout][main]
    Size=left,right,top,bottom
    Dock=console,bottom,min

========================================================================
*/

void* editor_layout::settings_read_open(ImGuiContext*, ImGuiSettingsHandler*, const char* name)
{
    editor_layout* l = find(name);
    if (l)
        l->slots.clear();   // las asignaciones guardadas sustituyen a las del código
    return l;
}

void editor_layout::settings_read_line(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line)
{
    editor_layout* l = static_cast<editor_layout*>(entry);
    float   s[4];
    char    wname[64], rname[16], state[8] = "";

    if (sscanf(line, "Size=%f,%f,%f,%f", &s[0], &s[1], &s[2], &s[3]) == 4)
    {
        l->set_size(LAYOUT_LEFT, s[0]);
        l->set_size(LAYOUT_RIGHT, s[1]);
        l->set_size(LAYOUT_TOP, s[2]);
        l->set_size(LAYOUT_BOTTOM, s[3]);
    }
    else if (sscanf(line, "Dock=%63[^,],%15[^,],%7s", wname, rname, state) >= 2)
    {
        editor_window* w = editor_window::find(wname);
        layout_region r = parse_region(rname);
        if (!w || r == LAYOUT_REGION_COUNT)
            return;

        l->dock(w, r);
        if (!strcmp(state, "min") && r != LAYOUT_CENTER)
            w->minimized = true;
    }
}

void editor_layout::settings_write_all(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    for (const editor_layout* l : layouts)
    {
        buf->appendf("[%s][%s]\n", handler->TypeName, l->name.c_str());
        buf->appendf("Size=%.4f,%.4f,%.4f,%.4f\n",
            l->size[LAYOUT_LEFT], l->size[LAYOUT_RIGHT], l->size[LAYOUT_TOP], l->size[LAYOUT_BOTTOM]);
        for (const slot& s : l->slots)
            buf->appendf("Dock=%s,%s%s\n", s.window->get_name(), region_name(s.region),
                (s.window->is_minimized() && s.region != LAYOUT_CENTER) ? ",min" : "");
        buf->append("\n");
    }
}

/*
========================================================================

    COMANDOS

========================================================================
*/

/*
==================
Layout_Dock_f

dockwindow <ventana> <left|right|top|bottom|center|float>
==================
*/
static void Layout_Dock_f(void)
{
    editor_layout* l = editor_layout::active();
    if (Cmd_Argc() != 3 || !l)
    {
        Con_Printf("dockwindow <name> <left|right|top|bottom|center|float> : move a window to a layout region\n");
        return;
    }

    editor_window* w = editor_window::find(Cmd_Argv(1));
    if (!w)
    {
        Con_Printf("dockwindow: window %s not found\n", Cmd_Argv(1));
        return;
    }

    layout_region r = editor_layout::parse_region(Cmd_Argv(2));
    if (r == LAYOUT_REGION_COUNT)
    {
        Con_Printf("dockwindow: unknown region %s\n", Cmd_Argv(2));
        return;
    }

    l->dock(w, r);
    w->show();
}

/*
==================
Layout_Minimize_f

minimizewindow <ventana> / restorewindow <ventana>
==================
*/
static void Layout_Minimize_f(void)
{
    const bool minimize = !strcasecmp(Cmd_Argv(0), "minimizewindow");
    if (Cmd_Argc() != 2)
    {
        Con_Printf("%s <name>\n", Cmd_Argv(0));
        return;
    }

    editor_window* w = editor_window::find(Cmd_Argv(1));
    if (!w)
    {
        Con_Printf("%s: window %s not found\n", Cmd_Argv(0), Cmd_Argv(1));
        return;
    }

    if (minimize)
        w->minimize();
    else
        w->restore();
}

/*
==================
Layout_Info_f
==================
*/
static void Layout_Info_f(void)
{
    editor_layout* l = editor_layout::active();
    if (!l)
        return;

    Con_Printf("layout \"%s\"\n", l->get_name());
    for (int r = LAYOUT_LEFT; r < LAYOUT_CENTER; r++)
        Con_Printf("  %-8s %.0f%%\n", editor_layout::region_name((layout_region)r), l->get_size((layout_region)r) * 100.0f);
}

/*
==================
editor_layout::init
==================
*/
void editor_layout::init()
{
    ImGuiSettingsHandler handler;
    handler.TypeName = "Layout";
    handler.TypeHash = ImHashStr("Layout");
    handler.ReadOpenFn = settings_read_open;
    handler.ReadLineFn = settings_read_line;
    handler.WriteAllFn = settings_write_all;
    ImGui::AddSettingsHandler(&handler);

    Cmd_AddCommand("dockwindow", Layout_Dock_f);
    Cmd_AddCommand("minimizewindow", Layout_Minimize_f);
    Cmd_AddCommand("restorewindow", Layout_Minimize_f);
    Cmd_AddCommand("layoutinfo", Layout_Info_f);
}
