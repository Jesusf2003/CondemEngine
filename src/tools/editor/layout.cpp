// layout.cpp -- distribución de ventanas en la capa principal del editor

#include "imgui_internal.h"     // antes que imgui.h: GImGui, ImGuiSettingsHandler, MarkIniSettingsDirty

#include "tools/editor/layout.h"
#include "tools/editor/style.h"

#include "core/cmd.h"
#include "engine/console.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
    #define strcasecmp  _stricmp
    #define strncasecmp _strnicmp
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

static void Layout_MarkDirty(void)
{
    if (ImGui::GetCurrentContext())
        ImGui::MarkIniSettingsDirty();
}

/*
==================
editor_layout
==================
*/
editor_layout::editor_layout(const char* name, editor_layout* parent) :
    name(name), parent(parent),
    workspace(layout_empty_rect), inner(layout_empty_rect),
    drag_window(nullptr)
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
    if (!parent)
    {
        // el centro del layout base es otro layout con sus propias regiones
        center_layout.reset(new editor_layout((this->name + ".center").c_str(), this));
        if (!active_layout)
            active_layout = this;
    }
}

/*
==================
~editor_layout
==================
*/
editor_layout::~editor_layout()
{
    center_layout.reset();
    layouts.erase(std::find(layouts.begin(), layouts.end(), this));
    if (active_layout == this)
    {
        active_layout = nullptr;
        for (editor_layout* l : layouts)
        {
            if (!l->parent)
            {
                active_layout = l;
                break;
            }
        }
    }
}

editor_layout* editor_layout::root()
{
    editor_layout* l = this;
    while (l->parent)
        l = l->parent;
    return l;
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

editor_layout* editor_layout::owner_of(const editor_window* window)
{
    if (find_slot(window))
        return this;
    return center_layout ? center_layout->owner_of(window) : nullptr;
}

const editor_layout* editor_layout::owner_of(const editor_window* window) const
{
    if (find_slot(window))
        return this;
    return center_layout ? static_cast<const editor_layout*>(center_layout.get())->owner_of(window) : nullptr;
}

/*
==================
editor_layout::panel

Ventanas visibles de una región, en orden (las minimizadas están en la barra)
==================
*/
std::vector<editor_layout::slot*> editor_layout::panel(layout_region region)
{
    std::vector<slot*> list;
    for (slot& s : slots)
        if (s.region == region && s.window->is_open() && !(s.window->is_minimized() && region != LAYOUT_CENTER))
            list.push_back(&s);
    return list;
}

std::vector<const editor_layout::slot*> editor_layout::panel(layout_region region) const
{
    std::vector<const slot*> list;
    for (const slot& s : slots)
        if (s.region == region && s.window->is_open() && !(s.window->is_minimized() && region != LAYOUT_CENTER))
            list.push_back(&s);
    return list;
}

bool editor_layout::has_open_windows() const
{
    for (const slot& s : slots)
        if (s.window->is_open())
            return true;
    return center_layout && center_layout->has_open_windows();
}

/*
==================
editor_layout::dock
==================
*/
void editor_layout::dock(editor_window* window, layout_region region, int index)
{
    if (region <= LAYOUT_FLOAT || region >= LAYOUT_REGION_COUNT)
    {
        root()->undock(window);
        return;
    }

    // el centro del layout base es el layout del centro
    if (region == LAYOUT_CENTER && center_layout)
    {
        center_layout->dock(window, LAYOUT_CENTER, index);
        return;
    }

    root()->undock(window);

// peso: el medio de los paneles de la región, para entrar con un tamaño parecido
    const std::vector<slot*> list = panel(region);
    float weight = 1.0f;
    if (!list.empty())
    {
        weight = 0.0f;
        for (const slot* s : list)
            weight += s->weight;
        weight /= list.size();
    }

// posición: antes del panel visible número index, o al final de la región
    size_t pos = slots.size();
    if (index >= 0 && index < (int)list.size())
        pos = list[index] - slots.data();
    else
    {
        for (size_t i = 0; i < slots.size(); i++)
            if (slots[i].region == region)
                pos = i + 1;
    }

    slots.insert(slots.begin() + pos, { window, region, layout_empty_rect, weight });

    // entra restaurada aunque viniera minimizada o maximizada como flotante
    window->minimized = false;
    window->maximized = false;

    Layout_MarkDirty();
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
            window->maximized = false;
            Layout_MarkDirty();
            return;
        }
    }

    if (center_layout)
        center_layout->undock(window);
}

/*
==================
editor_layout::dock_default / dock_defaults
==================
*/
void editor_layout::dock_default(editor_window* window)
{
    const layout_region r = window->get_default_dock();
    if (r != LAYOUT_FLOAT && !root()->owner_of(window))
        root()->dock(window, r);
}

void editor_layout::dock_defaults()
{
    for (editor_window* w : editor_window::windex)
        dock_default(w);
}

/*
==================
editor_layout::region_of
==================
*/
layout_region editor_layout::region_of(const editor_window* window) const
{
    if (const slot* s = find_slot(window))
        return s->region;
    return center_layout ? center_layout->region_of(window) : LAYOUT_FLOAT;
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

Reparte total entre elementos según su peso, sin pasar del máximo de ninguno
ni bajar de su mínimo (lim.x, lim.y). Si los mínimos no caben se reducen en
proporción (nunca se invade otra región). Si todos llegan a su máximo, sobra
espacio al final
==================
*/
static std::vector<float> Layout_Distribute(float total, const std::vector<ImVec2>& lim, const std::vector<float>& weight)
{
    const size_t n = lim.size();
    std::vector<float> out(n, 0.0f);
    if (!n)
        return out;

    auto w = [&](size_t i) { return (i < weight.size() && weight[i] > 0.0f) ? weight[i] : 1.0f; };

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
    while (true)
    {
        float wsum = 0.0f;
        for (size_t i = 0; i < n; i++)
            if (!fixed[i])
                wsum += w(i);
        if (wsum <= 0.0f)
            break;

        // primero los que pasan de su máximo, luego los que no llegan al mínimo
        bool changed = false;
        for (size_t i = 0; i < n; i++)
        {
            if (!fixed[i] && lim[i].y < remaining * w(i) / wsum)
            {
                out[i] = lim[i].y;
                fixed[i] = true;
                changed = true;
            }
        }
        if (!changed)
        {
            for (size_t i = 0; i < n; i++)
            {
                if (!fixed[i] && lim[i].x > remaining * w(i) / wsum)
                {
                    out[i] = lim[i].x;
                    fixed[i] = true;
                    changed = true;
                }
            }
        }
        if (!changed)
        {
            for (size_t i = 0; i < n; i++)
                if (!fixed[i])
                    out[i] = remaining * w(i) / wsum;
            break;
        }

        remaining = total;
        for (size_t i = 0; i < n; i++)
            if (fixed[i])
                remaining -= out[i];
    }
    return out;
}

/*
==================
editor_layout::stack_axis

Eje en el que se apilan los paneles de una región
==================
*/
int editor_layout::stack_axis(layout_region region)
{
    return (region == LAYOUT_TOP || region == LAYOUT_BOTTOM) ? 0 : 1;
}

/*
==================
editor_layout::region_limits

Grosor mínimo y máximo de una región según sus ventanas: el mínimo es el
mayor de los mínimos y el máximo el mayor de los máximos (para que quepa la
que más necesita). Sin ventanas, no hay máximo
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
center_used: el layout del centro tiene ventanas. lo/hi reciben los límites
de grosor de cada región
==================
*/
void editor_layout::compute_areas(const window_rect& area, const window_list lists[], bool center_used,
    window_rect areas[], float lo[], float hi[]) const
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
    occ[LAYOUT_CENTER] = occ[LAYOUT_CENTER] || center_used;

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
editor_layout::stack_rects

Reparte el área de una región entre sus paneles (en vertical en left/right/
center, en horizontal en top/bottom) según sus pesos y límites de tamaño. En
el otro eje cada ventana ocupa el grosor de la región, salvo que su máximo
sea menor: entonces se pega al borde exterior
==================
*/
std::vector<window_rect> editor_layout::stack_rects(const window_list& list, const std::vector<float>& weights,
    const window_rect& area, layout_region region) const
{
    std::vector<window_rect> rects(list.size(), layout_empty_rect);
    if (list.empty())
        return rects;

    const int axis = stack_axis(region);
    const int other = axis ^ 1;
    const bool anchor_max = (region == LAYOUT_RIGHT || region == LAYOUT_BOTTOM);
    const float thick = area.max[other] - area.min[other];

    std::vector<ImVec2> lim;
    std::vector<float> max_other;
    for (const editor_window* w : list)
    {
        ImVec2 wmin, wmax;
        w->get_layout_limits(wmin, wmax);
        lim.push_back(ImVec2(wmin[axis], wmax[axis]));
        max_other.push_back(wmax[other]);
    }
    const std::vector<float> len = Layout_Distribute(area.max[axis] - area.min[axis], lim, weights);

    float pos = area.min[axis];
    for (size_t i = 0; i < list.size(); i++)
    {
        const float t = std::min(thick, max_other[i]);
        window_rect& r = rects[i];
        r.min[axis] = std::floor(pos);
        r.max[axis] = std::floor(pos + len[i]);
        r.min[other] = anchor_max ? area.max[other] - t : area.min[other];
        r.max[other] = r.min[other] + t;
        pos += len[i];
    }
    return rects;
}

/*
==================
editor_layout::arrange
==================
*/
void editor_layout::arrange(const window_rect& ws)
{
    workspace = ws;

// paneles de cada región; las minimizadas van a la barra de su borde
    std::vector<slot*> list[LAYOUT_REGION_COUNT];
    window_list windows[LAYOUT_REGION_COUNT];
    std::vector<float> weights[LAYOUT_REGION_COUNT];
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
            weights[s.region].push_back(s.weight);
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
    const bool center_used = center_layout && center_layout->has_open_windows();
    compute_areas(inner, windows, center_used, region_area, region_min, region_max);
    for (int r = 0; r < LAYOUT_REGION_COUNT; r++)
        occupied[r] = !windows[r].empty();
    occupied[LAYOUT_CENTER] = occupied[LAYOUT_CENTER] || center_used;

// paneles
    for (int r = LAYOUT_LEFT; r < LAYOUT_REGION_COUNT; r++)
    {
        const std::vector<window_rect> rects = stack_rects(windows[r], weights[r], region_area[r], (layout_region)r);
        for (size_t i = 0; i < list[r].size(); i++)
            list[r][i]->rect = rects[i];
    }

// el centro es otro layout
    if (center_layout)
        center_layout->arrange(region_area[LAYOUT_CENTER]);
}

/*
==================
editor_layout::rect_of
==================
*/
const window_rect* editor_layout::rect_of(const editor_window* window) const
{
    if (const slot* s = find_slot(window))
        return &s->rect;
    return center_layout ? center_layout->rect_of(window) : nullptr;
}

/*
========================================================================

    REDIMENSIONADO DE VENTANAS ACOPLADAS

========================================================================
*/

/*
==================
editor_layout::extent_limits

Grosor que puede tener la región de la ventana. El máximo es el menor entre
el de sus ventanas y el espacio que dejan libre las otras regiones ocupadas
(y el mínimo del centro), así una región nunca cubre ni achica a otra
==================
*/
void editor_layout::extent_limits(const slot* s, float& lo, float& hi) const
{
    lo = 0.0f;
    hi = FLT_MAX;
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
editor_layout::resize_edges
==================
*/
void editor_layout::resize_edges(const editor_window* window, bool allowed[2][2]) const
{
    allowed[0][0] = allowed[0][1] = allowed[1][0] = allowed[1][1] = false;

    const editor_layout* owner = owner_of(window);
    if (owner && owner != this)
    {
        owner->resize_edges(window, allowed);
        return;
    }
    const slot* s = find_slot(window);
    if (!s)
        return;

// el borde que da al centro cambia el grosor de la región
    switch (s->region)
    {
    case LAYOUT_LEFT:   allowed[0][1] = true; break;
    case LAYOUT_RIGHT:  allowed[0][0] = true; break;
    case LAYOUT_TOP:    allowed[1][1] = true; break;
    case LAYOUT_BOTTOM: allowed[1][0] = true; break;
    default: break;
    }

// los bordes entre paneles reparten el espacio con el vecino
    const std::vector<const slot*> list = panel(s->region);
    const size_t k = std::find(list.begin(), list.end(), s) - list.begin();
    const int axis = stack_axis(s->region);
    if (k < list.size())
    {
        if (k > 0)
            allowed[axis][0] = true;
        if (k + 1 < list.size())
            allowed[axis][1] = true;
    }
}

/*
==================
editor_layout::constrain_resize
==================
*/
void editor_layout::constrain_resize(const editor_window* window, const bool edge[2][2], window_rect& r) const
{
    const editor_layout* owner = owner_of(window);
    if (owner && owner != this)
    {
        owner->constrain_resize(window, edge, r);
        return;
    }
    const slot* s = find_slot(window);
    if (!s)
        return;

// grosor de la región
    if (s->region != LAYOUT_CENTER)
    {
        float lo, hi;
        extent_limits(s, lo, hi);
        const int t = stack_axis(s->region) ^ 1;
        if (edge[t][0])
            r.min[t] = std::min(std::max(r.min[t], r.max[t] - hi), r.max[t] - lo);
        if (edge[t][1])
            r.max[t] = std::max(std::min(r.max[t], r.min[t] + hi), r.min[t] + lo);
    }

// bordes entre paneles: ni esta ventana ni la vecina salen de sus límites
    const std::vector<const slot*> list = panel(s->region);
    const size_t k = std::find(list.begin(), list.end(), s) - list.begin();
    if (k >= list.size())
        return;

    const int axis = stack_axis(s->region);
    ImVec2 wlo, whi;
    window->get_layout_limits(wlo, whi);

    if (edge[axis][0] && k > 0)
    {
        const slot* prev = list[k - 1];
        ImVec2 plo, phi;
        prev->window->get_layout_limits(plo, phi);
        const float a = std::max(prev->rect.min[axis] + plo[axis], r.max[axis] - whi[axis]);
        const float b = std::min(prev->rect.min[axis] + phi[axis], r.max[axis] - wlo[axis]);
        r.min[axis] = (a <= b) ? std::min(std::max(r.min[axis], a), b) : s->rect.min[axis];
    }
    if (edge[axis][1] && k + 1 < list.size())
    {
        const slot* next = list[k + 1];
        ImVec2 nlo, nhi;
        next->window->get_layout_limits(nlo, nhi);
        const float a = std::max(r.min[axis] + wlo[axis], next->rect.max[axis] - nhi[axis]);
        const float b = std::min(r.min[axis] + whi[axis], next->rect.max[axis] - nlo[axis]);
        r.max[axis] = (a <= b) ? std::min(std::max(r.max[axis], a), b) : s->rect.max[axis];
    }
}

/*
==================
editor_layout::resized

El usuario arrastró un borde de una ventana acoplada: el que da al centro
cambia el tamaño de la región; uno entre paneles, los pesos de los paneles
==================
*/
void editor_layout::resized(const editor_window* window, const window_rect& r)
{
    editor_layout* owner = owner_of(window);
    if (owner && owner != this)
    {
        owner->resized(window, r);
        return;
    }
    slot* s = find_slot(window);
    if (!s || inner.width() <= 0 || inner.height() <= 0)
        return;

    const int axis = stack_axis(s->region);

// grosor de la región
    if (s->region != LAYOUT_CENTER)
    {
        const int t = axis ^ 1;
        float extent = r.max[t] - r.min[t];
        if (std::fabs(extent - (s->rect.max[t] - s->rect.min[t])) > 0.5f)
        {
            float lo, hi;
            extent_limits(s, lo, hi);
            extent = std::min(std::max(extent, lo), hi);
            size[s->region] = extent / (t == 0 ? inner.width() : inner.height());
            Layout_MarkDirty();
        }
    }

// bordes entre paneles: los pesos pasan a ser el tamaño de cada panel
    std::vector<slot*> list = panel(s->region);
    const size_t k = std::find(list.begin(), list.end(), s) - list.begin();
    if (k >= list.size())
        return;

    const bool moved_min = k > 0 && std::fabs(r.min[axis] - s->rect.min[axis]) > 0.5f;
    const bool moved_max = k + 1 < list.size() && std::fabs(r.max[axis] - s->rect.max[axis]) > 0.5f;
    if (!moved_min && !moved_max)
        return;

    std::vector<float> len(list.size());
    for (size_t i = 0; i < list.size(); i++)
        len[i] = list[i]->rect.max[axis] - list[i]->rect.min[axis];

    const float new_len = r.max[axis] - r.min[axis];
    const float delta = new_len - len[k];
    len[k] = new_len;
    if (moved_min)
        len[k - 1] -= delta;
    else
        len[k + 1] -= delta;

    float total = 0.0f;
    for (float l : len)
        total += std::max(l, 1.0f);
    for (size_t i = 0; i < list.size(); i++)
        list[i]->weight = std::max(len[i], 1.0f) / total;

    Layout_MarkDirty();
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

    char id[96];
    snprintf(id, sizeof(id), "##bar_%s_%s", name.c_str(), region_name(region));

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

    if (center_layout)
        center_layout->draw_bars();
}

/*
========================================================================

    ARRASTRAR Y SOLTAR

    Zonas donde se puede soltar una ventana flotante, por orden:
    1. franja junto a cada borde del layout -> esa región
    2. sobre un panel acoplado -> su región, antes o después de él
    3. dentro del centro -> lo mismo en el layout del centro
    4. cuadrado en medio del layout del centro -> su región center
    La posición dentro del panel sale de dónde está el cursor.

========================================================================
*/

/*
==================
editor_layout::center_zone

Zona para soltar en el centro: un cuadrado en medio del área del layout
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
editor_layout::index_at

Posición dentro del panel de la región para el cursor p: delante del
primer panel cuya mitad queda después del cursor
==================
*/
int editor_layout::index_at(layout_region region, const ImVec2& p) const
{
    const std::vector<const slot*> list = panel(region);
    const int axis = stack_axis(region);
    int index = 0;
    for (const slot* s : list)
        if ((s->rect.min[axis] + s->rect.max[axis]) * 0.5f < p[axis])
            index++;
    return index;
}

/*
==================
editor_layout::find_target
==================
*/
bool editor_layout::find_target(const ImVec2& p, dock_target& target)
{
    const window_rect& ws = workspace;
    if (ws.width() <= 0 || ws.height() <= 0)
        return false;

// 1. franja de ed_style.layout_drop_edge px a cada lado de los bordes
    const float edge = ed_style.scaled(ed_style.layout_drop_edge);
    const bool in_x = p.x > ws.min.x - edge && p.x < ws.max.x + edge;
    const bool in_y = p.y > ws.min.y - edge && p.y < ws.max.y + edge;
    if (in_x && in_y)
    {
        layout_region r = LAYOUT_FLOAT;
        if (p.x < ws.min.x + edge)
            r = LAYOUT_LEFT;
        else if (p.x > ws.max.x - edge)
            r = LAYOUT_RIGHT;
        else if (p.y < ws.min.y + edge)
            r = LAYOUT_TOP;
        else if (p.y > ws.max.y - edge)
            r = LAYOUT_BOTTOM;

        if (r != LAYOUT_FLOAT)
        {
            target = { this, r, index_at(r, p) };
            return true;
        }
    }

// 2. sobre un panel acoplado de este layout
    for (int r = LAYOUT_LEFT; r < LAYOUT_REGION_COUNT; r++)
    {
        for (const slot* s : panel((layout_region)r))
        {
            if (Layout_Contains(s->rect, p))
            {
                target = { this, (layout_region)r, index_at((layout_region)r, p) };
                return true;
            }
        }
    }

// 3. el layout del centro
    if (center_layout)
        return Layout_Contains(region_area[LAYOUT_CENTER], p) && center_layout->find_target(p, target);

// 4. cuadrado del centro
    if (Layout_Contains(center_zone(), p))
    {
        target = { this, LAYOUT_CENTER, index_at(LAYOUT_CENTER, p) };
        return true;
    }
    return false;
}

/*
==================
editor_layout::preview

Hueco que ocuparía la ventana si se acoplara en target
==================
*/
window_rect editor_layout::preview(const dock_target& target, const editor_window* window) const
{
    const editor_layout* l = target.layout;
    if (!l || target.region <= LAYOUT_FLOAT || target.region >= LAYOUT_REGION_COUNT)
        return layout_empty_rect;

    window_list windows[LAYOUT_REGION_COUNT];
    std::vector<float> weights[LAYOUT_REGION_COUNT];
    for (int r = LAYOUT_LEFT; r < LAYOUT_REGION_COUNT; r++)
    {
        for (const slot* s : l->panel((layout_region)r))
        {
            windows[r].push_back(s->window);
            weights[r].push_back(s->weight);
        }
    }

    // insertar la ventana donde iría, con el peso medio de su panel
    window_list& list = windows[target.region];
    std::vector<float>& wl = weights[target.region];
    float weight = 1.0f;
    if (!wl.empty())
    {
        weight = 0.0f;
        for (float w : wl)
            weight += w;
        weight /= wl.size();
    }
    const size_t pos = (target.index >= 0 && target.index < (int)list.size()) ? (size_t)target.index : list.size();
    list.insert(list.begin() + pos, window);
    wl.insert(wl.begin() + pos, weight);

    window_rect areas[LAYOUT_REGION_COUNT];
    float lo[LAYOUT_REGION_COUNT], hi[LAYOUT_REGION_COUNT];
    const bool center_used = l->center_layout && l->center_layout->has_open_windows();
    l->compute_areas(l->inner, windows, center_used, areas, lo, hi);
    return l->stack_rects(list, wl, areas[target.region], target.region)[pos];
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
        if (owner_of(moving))
        {
            if (!ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Left, g.IO.MouseDragThreshold * 2.0f))
                return;
            undock_for_drag(moving);
        }

        // flotante: zona bajo el ratón (Mayús: soltar sin acoplar)
        drag_window = moving;
        drop_target = dock_target();
        if (!g.IO.KeyShift)
            find_target(g.IO.MousePos, drop_target);
        return;
    }

// se soltó la ventana que se arrastraba
    if (drag_window)
    {
        const bool alive = std::find(editor_window::windex.begin(), editor_window::windex.end(), drag_window) != editor_window::windex.end();
        if (alive && drop_target.layout)
            drop_target.layout->dock(drag_window, drop_target.region, drop_target.index);
        drag_window = nullptr;
        drop_target = dock_target();
    }
}

/*
==================
editor_layout::draw_zone_bands

Franjas de los bordes de este layout (y las del centro)
==================
*/
void editor_layout::draw_zone_bands(ImDrawList* fg) const
{
    const window_rect& ws = workspace;
    if (ws.width() <= 0 || ws.height() <= 0)
        return;

    const ImU32 zone = ImGui::ColorConvertFloat4ToU32(ed_style.drop_zone);
    const ImU32 accent = ImGui::ColorConvertFloat4ToU32(ed_style.accent);
    const float edge = ed_style.scaled(ed_style.layout_drop_edge);
    const float rounding = ed_style.scaled(ed_style.window_rounding);
    const float border = std::max(1.0f, ed_style.scaled(1.0f));

    fg->AddRectFilled(ws.min, ImVec2(ws.min.x + edge, ws.max.y), zone);
    fg->AddRectFilled(ImVec2(ws.max.x - edge, ws.min.y), ws.max, zone);
    fg->AddRectFilled(ImVec2(ws.min.x + edge, ws.min.y), ImVec2(ws.max.x - edge, ws.min.y + edge), zone);
    fg->AddRectFilled(ImVec2(ws.min.x + edge, ws.max.y - edge), ImVec2(ws.max.x - edge, ws.max.y), zone);

    if (center_layout)
        center_layout->draw_zone_bands(fg);
    else
    {
        const window_rect c = center_zone();
        fg->AddRectFilled(c.min, c.max, zone, rounding);
        fg->AddRect(c.min, c.max, accent, rounding, 0, border);
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
    draw_zone_bands(fg);

// dónde quedaría
    if (drop_target.layout)
    {
        const ImU32 fill = ImGui::ColorConvertFloat4ToU32(ed_style.drop_preview);
        const ImU32 accent = ImGui::ColorConvertFloat4ToU32(ed_style.accent);
        const float rounding = ed_style.scaled(ed_style.window_rounding);
        const float border = std::max(1.0f, ed_style.scaled(2.0f));
        const window_rect p = preview(drop_target, drag_window);
        fg->AddRectFilled(p.min, p.max, fill, rounding);
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
    active_layout = layout ? layout->root() : nullptr;
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
==================
editor_layout::parse_target

"left", "center", "center.top"...
==================
*/
bool editor_layout::parse_target(const char* text, dock_target& target)
{
    const char* dot = strchr(text, '.');
    if (dot)
    {
        // "center.<región>": región del layout del centro
        const size_t len = dot - text;
        if (len != strlen("center") || strncasecmp(text, "center", len) || !center_layout)
            return false;
        return center_layout->parse_target(dot + 1, target);
    }

    const layout_region r = parse_region(text);
    if (r == LAYOUT_REGION_COUNT)
        return false;

    target = { this, r, -1 };
    return true;
}

/*
========================================================================

    GUARDADO EN EDITOR.INI

    [Layout][main]
    Size=left,right,top,bottom
    Dock=console,bottom,w=1.0000,min

    [Layout][main.center]
    ...

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
    float s[4];

    if (sscanf(line, "Size=%f,%f,%f,%f", &s[0], &s[1], &s[2], &s[3]) == 4)
    {
        l->set_size(LAYOUT_LEFT, s[0]);
        l->set_size(LAYOUT_RIGHT, s[1]);
        l->set_size(LAYOUT_TOP, s[2]);
        l->set_size(LAYOUT_BOTTOM, s[3]);
        return;
    }

    if (strncmp(line, "Dock=", 5))
        return;

// Dock=<ventana>,<región>[,w=<peso>][,min]
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", line + 5);
    char* tokens[8];
    int count = 0;
    for (char* t = strtok(buf, ","); t && count < 8; t = strtok(nullptr, ","))
        tokens[count++] = t;
    if (count < 2)
        return;

    editor_window* w = editor_window::find(tokens[0]);
    const layout_region r = parse_region(tokens[1]);
    if (!w || r <= LAYOUT_FLOAT || r == LAYOUT_REGION_COUNT)
        return;

    l->dock(w, r);
    slot* sl = l->find_slot(w);
    if (!sl)
        return;

    for (int i = 2; i < count; i++)
    {
        if (!strncmp(tokens[i], "w=", 2))
            sl->weight = std::max((float)atof(tokens[i] + 2), 0.01f);
        else if (!strcmp(tokens[i], "min") && r != LAYOUT_CENTER)
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
            buf->appendf("Dock=%s,%s,w=%.4f%s\n", s.window->get_name(), region_name(s.region), s.weight,
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

dockwindow <ventana> <región> [posición]
==================
*/
static void Layout_Dock_f(void)
{
    editor_layout* l = editor_layout::active();
    if ((Cmd_Argc() != 3 && Cmd_Argc() != 4) || !l)
    {
        Con_Printf("dockwindow <name> <region> [index] : move a window to a layout region\n");
        Con_Printf("  region: left right top bottom center float, or center.left center.top...\n");
        return;
    }

    editor_window* w = editor_window::find(Cmd_Argv(1));
    if (!w)
    {
        Con_Printf("dockwindow: window %s not found\n", Cmd_Argv(1));
        return;
    }

    dock_target t;
    if (!l->parse_target(Cmd_Argv(2), t))
    {
        Con_Printf("dockwindow: unknown region %s\n", Cmd_Argv(2));
        return;
    }

    t.layout->dock(w, t.region, Cmd_Argc() == 4 ? atoi(Cmd_Argv(3)) : -1);
    w->show();
}

/*
==================
Layout_Minimize_f

minimizewindow / maximizewindow / restorewindow <ventana>
==================
*/
static void Layout_Minimize_f(void)
{
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

    if (!strcasecmp(Cmd_Argv(0), "minimizewindow"))
        w->minimize();
    else if (!strcasecmp(Cmd_Argv(0), "maximizewindow"))
        w->maximize();
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
    for (editor_layout* l = editor_layout::active(); l; l = l->center())
    {
        Con_Printf("layout \"%s\"\n", l->get_name());
        for (int r = LAYOUT_LEFT; r < LAYOUT_CENTER; r++)
            Con_Printf("  %-8s %.0f%%\n", editor_layout::region_name((layout_region)r), l->get_size((layout_region)r) * 100.0f);
    }
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
    Cmd_AddCommand("maximizewindow", Layout_Minimize_f);
    Cmd_AddCommand("restorewindow", Layout_Minimize_f);
    Cmd_AddCommand("layoutinfo", Layout_Info_f);
}
