// layout.h -- distribución de ventanas en la capa principal del editor
//
// Un editor_layout divide el área de trabajo en cinco regiones:
//
//   +---------------------------------+
//   |              top                |
//   +------+-------------------+------+
//   |      |    center.top     |      |
//   |      +----+---------+----+      |
//   | left |c.l | center  | c.r| right|
//   |      +----+---------+----+      |
//   |      |   center.bottom   |      |
//   +------+-------------------+------+
//   |             bottom              |
//   +---------------------------------+
//
// Cada región es un panel acoplable: sus ventanas se colocan en orden
//   - left/right/center: de arriba a abajo
//   - top/bottom:        de izquierda a derecha
// y se puede acoplar una ventana en cualquier posición de la lista. Cada
// panel tiene su propio tamaño (peso) y se reajusta arrastrando el borde
// entre dos paneles.
//
// El centro del layout base es a su vez un layout (hijo) con sus propias
// regiones top, bottom, left, right y center ("center.left", etc.).
// Una región sin ventanas abiertas cede su espacio al centro.
//
// Según su contenedor, las ventanas acopladas son:
//   - reajustables: por el borde que da al centro (cambia toda la región) y
//     por los bordes entre paneles, entre los límites de tamaño de sus
//     ventanas (ver editor_window::get_layout_limits) y sin invadir las otras
//     regiones ocupadas: una región solo crece con el espacio libre
//   - que cubren su hueco o no: con WND_COVER_LAYOUT (por defecto) la ventana
//     ocupa todo su hueco; sin ella se queda en su tamaño máximo
//   - minimizables (left/right/top/bottom): pasan a una barra en el borde de
//     su región, con una pestaña por ventana. Al hacer clic se restauran
//   - desacoplables: al arrastrarlas por la barra de título salen del layout
//     y quedan flotantes. Al arrastrar una ventana flotante aparecen zonas en
//     los bordes de cada layout; también se puede soltar sobre un panel
//     acoplado (antes o después de él, según la mitad) o en el centro.
//     Con Mayús pulsado se suelta sin acoplar
//
// Es reutilizable: se pueden crear varios layouts y cambiar el activo con
// set_active(). Tamaños, orden, pesos y minimizadas se guardan en editor.ini.

#pragma once

#include "tools/editor/window.h"

#include <memory>
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

class editor_layout;

// Dónde se acoplaría una ventana: layout (base o del centro), región y
// posición dentro del panel de la región (-1 = al final)
struct dock_target
{
    editor_layout*  layout = nullptr;
    layout_region   region = LAYOUT_FLOAT;
    int             index = -1;
};

class editor_layout
{
public:
    explicit editor_layout(const char* name, editor_layout* parent = nullptr);
    ~editor_layout();

    editor_layout(const editor_layout&) = delete;
    editor_layout& operator=(const editor_layout&) = delete;

    //------------------------------------------------------------------
    // Asignación de ventanas

    void            dock(editor_window* window, layout_region region, int index = -1);
    // Coloca la ventana en una región, en la posición index de su panel
    // (-1 = al final). LAYOUT_FLOAT la deja flotante. En el layout base,
    // LAYOUT_CENTER la manda a la región center del layout del centro.
    // Si la ventana estaba en otro sitio del árbol de layouts, se mueve.

    void            undock(editor_window* window);
    // La saca de este layout o del de su centro.

    layout_region   region_of(const editor_window* window) const;
    // Región dentro del layout que la contiene (este o el del centro).

    editor_layout*  owner_of(const editor_window* window);
    const editor_layout* owner_of(const editor_window* window) const;
    // Layout que contiene la ventana (este o el del centro), o nullptr.

    editor_layout*  center()            { return center_layout.get(); }
    // Layout del centro (nullptr en el propio layout del centro).

    editor_layout*  root();
    // Layout base del árbol.

    //------------------------------------------------------------------
    // Tamaño de las regiones

    void            set_size(layout_region region, float fraction);
    // left/right: fracción del ancho del área del layout
    // top/bottom: fracción del alto
    float           get_size(layout_region region) const;

    //------------------------------------------------------------------
    // Uso desde editor_window::draw_all, en este orden cada frame
    // (sobre el layout base; los del centro se tratan desde él)

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

    //------------------------------------------------------------------
    // Redimensionado de ventanas acopladas (desde editor_window)

    void            resize_edges(const editor_window* window, bool allowed[2][2]) const;
    // Qué bordes se pueden arrastrar [eje][0 = min, 1 = max]: el que da al
    // centro y los que están entre dos paneles.

    void            constrain_resize(const editor_window* window, const bool edge[2][2], window_rect& r) const;
    // Ajusta el rectángulo propuesto a los límites de la ventana, de la región
    // (sin invadir otras) y del panel vecino.

    void            resized(const editor_window* window, const window_rect& r);
    // El usuario redimensionó una ventana acoplada: ajusta la región y los
    // pesos de los paneles.

    const char*     get_name() const    { return name.c_str(); }

    //------------------------------------------------------------------
    // Todos los layouts

    static std::vector<editor_layout*> layouts;     // incluye los del centro

    static void             init();
    // Registra el guardado en editor.ini y los comandos. Llamar después de
    // ImGui::CreateContext() y antes del primer ImGui::NewFrame().

    static editor_layout*   active()    { return active_layout; }
    static void             set_active(editor_layout* layout);
    static editor_layout*   find(const char* name);

    static const char*      region_name(layout_region region);
    static layout_region    parse_region(const char* name);     // LAYOUT_REGION_COUNT si no existe

    bool                    parse_target(const char* text, dock_target& target);
    // "left", "center", "center.top"... -> layout y región. false si no existe.

private:
    struct slot
    {
        editor_window*  window;
        layout_region   region;
        window_rect     rect;
        float           weight;     // tamaño relativo dentro de su panel
    };

    typedef std::vector<const editor_window*> window_list;

    slot*           find_slot(const editor_window* window);
    const slot*     find_slot(const editor_window* window) const;
    std::vector<slot*> panel(layout_region region);
    std::vector<const slot*> panel(layout_region region) const;
    bool            has_open_windows() const;

    static int      stack_axis(layout_region region);
    void            region_limits(const window_list& list, int axis, float& lo, float& hi) const;
    void            compute_areas(const window_rect& area, const window_list lists[], bool center_used,
                        window_rect areas[], float lo[], float hi[]) const;
    std::vector<window_rect> stack_rects(const window_list& list, const std::vector<float>& weights,
                        const window_rect& area, layout_region region) const;
    void            extent_limits(const slot* s, float& lo, float& hi) const;
    void            draw_bar(layout_region region);

    // arrastrar y soltar
    bool            find_target(const ImVec2& p, dock_target& target);
    int             index_at(layout_region region, const ImVec2& p) const;
    window_rect     center_zone() const;
    window_rect     preview(const dock_target& target, const editor_window* window) const;
    void            undock_for_drag(editor_window* window);
    void            draw_zone_bands(ImDrawList* fg) const;

    // guardado en editor.ini
    static void*    settings_read_open(ImGuiContext*, ImGuiSettingsHandler*, const char* name);
    static void     settings_read_line(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line);
    static void     settings_write_all(ImGuiContext*, ImGuiSettingsHandler*, ImGuiTextBuffer* buf);

    std::string         name;
    editor_layout*      parent;
    std::unique_ptr<editor_layout> center_layout;

    std::vector<slot>   slots;
    float               size[LAYOUT_REGION_COUNT];

    window_rect         workspace;      // área completa del último arrange()
    window_rect         inner;          // área sin las barras de minimizadas

    // resultado del último arrange(), para limitar el redimensionado
    bool                occupied[LAYOUT_REGION_COUNT];
    float               region_min[LAYOUT_REGION_COUNT];
    float               region_max[LAYOUT_REGION_COUNT];
    window_rect         region_area[LAYOUT_REGION_COUNT];

    window_rect                 bar_rect[LAYOUT_REGION_COUNT];
    std::vector<editor_window*> bar_windows[LAYOUT_REGION_COUNT];

    // arrastre (solo en el layout base)
    editor_window*      drag_window;    // flotante que se está arrastrando
    dock_target         drop_target;    // zona bajo el ratón mientras se arrastra

    static editor_layout* active_layout;
};
