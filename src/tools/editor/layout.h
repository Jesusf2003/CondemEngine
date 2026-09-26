// layout.h -- distribución de ventanas en la capa principal del editor
//
// Un editor_layout divide el área de trabajo en cinco regiones:
//
//   +---------------------------+
//   |           top             |
//   +------+-------------+------+
//   | left |   center    | right|
//   +------+-------------+------+
//   |          bottom           |
//   +---------------------------+
//
// Las ventanas se asignan a una región con dock(); el layout calcula su
// posición y tamaño cada frame. Varias ventanas en la misma región se apilan
// (en vertical en left/right/center, en horizontal en top/bottom). Una región
// sin ventanas abiertas cede su espacio al centro.
//
// Según su contenedor, las ventanas acopladas son:
//   - reajustables: por el borde que da al centro (cambia toda la región),
//     entre los límites de tamaño de sus ventanas (ver editor_window::
//     get_layout_limits) y sin invadir las otras regiones ocupadas: una región
//     solo crece con el espacio libre, nunca achica a sus vecinas
//   - que cubren su hueco o no: con WND_COVER_LAYOUT (por defecto) la ventana
//     ocupa todo su hueco; sin ella se queda en su tamaño máximo
//   - minimizables (left/right/top/bottom): en lugar de contraerse pasan a
//     una barra en el borde de su región, con una pestaña por ventana.
//     Al hacer clic en la pestaña se restauran
//   - desacoplables: al arrastrarlas por la barra de título salen del layout
//     y quedan flotantes. Al arrastrar una ventana flotante aparecen zonas en
//     los bordes y el centro; si se suelta en una, se acopla a esa región
//     (con Mayús pulsado se suelta sin acoplar)
//
// Es reutilizable: se pueden crear varios layouts y cambiar el activo con
// set_active(). Tamaños, asignaciones y minimizadas se guardan en editor.ini.

#pragma once

#include "tools/editor/window.h"

#include <string>
#include <vector>

struct ImGuiSettingsHandler;

enum layout_region : int
{
    LAYOUT_FLOAT = 0,       // flotante, fuera del layout
    LAYOUT_LEFT,
    LAYOUT_RIGHT,
    LAYOUT_TOP,
    LAYOUT_BOTTOM,
    LAYOUT_CENTER,

    LAYOUT_REGION_COUNT
};

class editor_layout
{
public:
    explicit editor_layout(const char* name);
    ~editor_layout();

    editor_layout(const editor_layout&) = delete;
    editor_layout& operator=(const editor_layout&) = delete;

    //------------------------------------------------------------------
    // Asignación de ventanas

    void            dock(editor_window* window, layout_region region);
    // Coloca la ventana en una región (LAYOUT_FLOAT la deja flotante).

    void            undock(editor_window* window);
    layout_region   region_of(const editor_window* window) const;

    //------------------------------------------------------------------
    // Tamaño de las regiones

    void            set_size(layout_region region, float fraction);
    // left/right: fracción del ancho del área de trabajo
    // top/bottom: fracción del alto
    float           get_size(layout_region region) const;

    //------------------------------------------------------------------
    // Uso desde editor_window::draw_all, en este orden cada frame

    void            update_drag();
    // Saca del layout la ventana acoplada que se arrastra y acopla la
    // flotante que se suelta sobre una zona.

    void            arrange(const window_rect& workspace);
    // Calcula las barras de minimizadas y la posición de las ventanas.

    void            draw_bars();
    // Dibuja las barras de ventanas minimizadas.

    void            draw_drop_zones();
    // Mientras se arrastra una ventana flotante, muestra dónde se puede soltar.

    const window_rect* rect_of(const editor_window* window) const;
    // Rectángulo asignado en el último arrange(), o nullptr si es flotante.

    void            resized(const editor_window* window, const window_rect& r);
    // El usuario redimensionó una ventana acoplada: ajusta su región.

    void            extent_limits(const editor_window* window, float& lo, float& hi) const;
    // Grosor mínimo y máximo que puede tener la región de la ventana (ancho en
    // left/right, alto en top/bottom): límites de sus ventanas y espacio que
    // dejan libre las demás regiones ocupadas.

    const char*     get_name() const    { return name.c_str(); }

    //------------------------------------------------------------------
    // Todos los layouts

    static std::vector<editor_layout*> layouts;

    static void             init();
    // Registra el guardado en editor.ini y los comandos. Llamar después de
    // ImGui::CreateContext() y antes del primer ImGui::NewFrame().

    static editor_layout*   active()    { return active_layout; }
    static void             set_active(editor_layout* layout);
    static editor_layout*   find(const char* name);

    static const char*      region_name(layout_region region);
    static layout_region    parse_region(const char* name);     // LAYOUT_REGION_COUNT si no existe

private:
    struct slot
    {
        editor_window*  window;
        layout_region   region;
        window_rect     rect;
    };

    slot*           find_slot(const editor_window* window);
    const slot*     find_slot(const editor_window* window) const;

    typedef std::vector<const editor_window*> window_list;

    void            region_limits(const window_list& list, int axis, float& lo, float& hi) const;
    void            compute_areas(const window_rect& area, const window_list lists[], window_rect areas[], float lo[], float hi[]) const;
    void            stack(std::vector<slot*>& list, const window_rect& area, layout_region region);
    void            draw_bar(layout_region region);

    layout_region   zone_at(const ImVec2& p) const;
    window_rect     center_zone() const;
    window_rect     preview(layout_region region) const;
    void            undock_for_drag(editor_window* window);

    // guardado en editor.ini
    static void*    settings_read_open(ImGuiContext*, ImGuiSettingsHandler*, const char* name);
    static void     settings_read_line(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line);
    static void     settings_write_all(ImGuiContext*, ImGuiSettingsHandler*, ImGuiTextBuffer* buf);

    std::string         name;
    std::vector<slot>   slots;
    float               size[LAYOUT_REGION_COUNT];

    window_rect         workspace;      // área completa del último arrange()
    window_rect         inner;          // área sin las barras de minimizadas

    // resultado del último compute_areas(), para limitar el redimensionado
    bool                occupied[LAYOUT_REGION_COUNT];
    float               region_min[LAYOUT_REGION_COUNT];
    float               region_max[LAYOUT_REGION_COUNT];
    window_rect         region_area[LAYOUT_REGION_COUNT];

    window_rect                 bar_rect[LAYOUT_REGION_COUNT];
    std::vector<editor_window*> bar_windows[LAYOUT_REGION_COUNT];

    editor_window*      drag_window;    // flotante que se está arrastrando
    layout_region       drop_region;    // zona bajo el ratón mientras se arrastra

    static editor_layout* active_layout;
};
