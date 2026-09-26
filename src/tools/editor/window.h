// window.h -- clase base de las ventanas del editor
// Basado en Window.h de QuakeEd 3 (docs/quaked3-master/Window.h), migrado a ImGui.
//
// Cada ventana del editor hereda de editor_window e implementa on_draw().
// La clase se encarga de:
//   - visibilidad: show/hide/toggle, minimizar/maximizar/restaurar
//   - barra de título al estilo de Windows: título a la izquierda y botones
//     de minimizar, maximizar/restaurar y cerrar a la derecha (en lugar de la
//     flecha y la X de ImGui). Doble clic en el título: maximizar/restaurar
//   - opciones de la ventana: cerrar, minimizar, mover, redimensionar...
//     se activan o desactivan por ventana con set_option()
//   - desplazamiento: al mover o redimensionar, los bordes se acoplan a los
//     bordes de las otras ventanas y del área de trabajo (TryDocking de QE3).
//     Con viewports (ImGuiConfigFlags_ViewportsEnable) una ventana flotante
//     puede salir de la ventana principal como ventana del sistema hija de
//     ella; sin viewports nunca sale del área de trabajo
//   - layout: si el layout activo tiene la ventana asignada a una región
//     (left, right, top, bottom, center), el layout decide su posición.
//     Al minimizarla se convierte en una pestaña de la barra de su borde,
//     y se puede sacar o meter en el layout arrastrándola (ver layout.h)

#pragma once

#include "imgui.h"

#include <string>
#include <vector>

class editor_layout;
enum layout_region : int;

// Opciones de ventana (combinables)
enum
{
    WND_CLOSABLE        = 1 << 0,   // botón de cerrar en la barra de título
    WND_MINIMIZABLE     = 1 << 1,   // botón de minimizar
    WND_MOVABLE         = 1 << 2,
    WND_RESIZABLE       = 1 << 3,
    WND_TITLEBAR        = 1 << 4,
    WND_SNAP            = 1 << 5,   // se acopla a otras ventanas y a los bordes
    WND_MIN_CONTENT     = 1 << 6,   // tamaño mínimo = lo que ocupa el contenido
    WND_MAX_CONTENT     = 1 << 7,   // tamaño máximo = lo que ocupa el contenido
    WND_COVER_LAYOUT    = 1 << 8,   // acoplada: cubre todo su hueco del layout (ignora el máximo)
    WND_MAXIMIZABLE     = 1 << 9,   // botón de maximizar/restaurar

    WND_DEFAULT         = WND_MINIMIZABLE | WND_MAXIMIZABLE | WND_MOVABLE | WND_RESIZABLE | WND_TITLEBAR
                        | WND_SNAP | WND_COVER_LAYOUT
};

// Rectángulo en coordenadas de pantalla; min/max se indexan por eje (0 = x, 1 = y)
struct window_rect
{
    ImVec2  min;
    ImVec2  max;

    float   width() const   { return max.x - min.x; }
    float   height() const  { return max.y - min.y; }
};

class editor_window
{
public:
    editor_window(const char* name, const char* title, unsigned options = WND_DEFAULT);
    virtual ~editor_window();

    editor_window(const editor_window&) = delete;
    editor_window& operator=(const editor_window&) = delete;

    //------------------------------------------------------------------
    // Visibilidad

    void        show();
    void        hide();
    void        toggle();
    void        focus();
    bool        is_open() const         { return open; }

    void        minimize();
    bool        is_minimized() const    { return minimized; }
    // Flotante: queda solo su barra de título.
    // Acoplada en left/right/top/bottom: pasa a la barra de su borde del layout.
    // El centro no se minimiza.

    void        maximize();
    void        toggle_maximize();
    bool        is_maximized() const    { return maximized; }
    bool        can_maximize() const;
    // Ocupa toda el área de trabajo de la ventana principal, encima del
    // layout. No se puede si su tamaño máximo es menor que el área.

    void        restore();
    // Como en Windows: si está minimizada vuelve a su estado anterior; si
    // está maximizada vuelve a su tamaño normal.

    //------------------------------------------------------------------
    // Opciones

    void        set_option(unsigned option, bool enable);
    bool        has_option(unsigned option) const   { return (options & option) != 0; }

    void        set_closable(bool enable)       { set_option(WND_CLOSABLE, enable); }
    void        set_minimizable(bool enable)    { set_option(WND_MINIMIZABLE, enable); }
    void        set_maximizable(bool enable)    { set_option(WND_MAXIMIZABLE, enable); }
    void        set_movable(bool enable)        { set_option(WND_MOVABLE, enable); }
    void        set_resizable(bool enable)      { set_option(WND_RESIZABLE, enable); }
    void        set_titlebar(bool enable)       { set_option(WND_TITLEBAR, enable); }
    void        set_snap(bool enable)           { set_option(WND_SNAP, enable); }
    void        set_cover_layout(bool enable)   { set_option(WND_COVER_LAYOUT, enable); }

    //------------------------------------------------------------------
    // Posición

    void        set_default_rect(float x, float y, float w, float h);
    // Posición y tamaño iniciales, en fracciones (0..1) del área de trabajo.
    // Solo se usa si editor.ini no tiene guardada la ventana.

    //------------------------------------------------------------------
    // Límites de tamaño
    //
    // Se combinan tres fuentes, de menor a mayor prioridad:
    //   1. por código: set_min_size() / set_max_size()
    //   2. por contenido: opciones WND_MIN_CONTENT / WND_MAX_CONTENT, que usan
    //      el tamaño que necesita el contenido (como el auto-ajuste de ImGui)
    //   3. on_size_limits(): cada clase de ventana puede ajustarlos a su gusto
    // Se aplican a las ventanas flotantes y también a las acopladas: una
    // región del layout no crece más allá de lo que admiten sus ventanas.

    void        set_min_size(float w, float h);
    // Tamaño mínimo en px a escala 1.0 (por defecto ed_style.window_min_size).

    void        set_max_size(float w, float h);
    // Tamaño máximo en px a escala 1.0; 0 = sin límite en ese eje (por defecto).

    void        get_size_limits(ImVec2& min, ImVec2& max) const;
    // Límites efectivos en px reales (ya escalados). max usa FLT_MAX sin límite.

    void        get_layout_limits(ImVec2& min, ImVec2& max) const;
    // Límites que usa el layout cuando está acoplada: con WND_COVER_LAYOUT
    // (activa por defecto) no hay máximo y la ventana cubre todo su hueco;
    // sin ella respeta su máximo y deja libre el resto de la región.

    ImVec2      get_content_size() const    { return content_size; }
    // Tamaño de ventana que necesita el contenido (0 hasta el primer frame).

    const char* get_name() const        { return name.c_str(); }
    const char* get_title() const       { return title.c_str(); }
    const char* get_imgui_id() const    { return imgui_id.c_str(); }
    const window_rect& get_rect() const { return rect; }

    //------------------------------------------------------------------
    // Todas las ventanas

    static std::vector<editor_window*> windex;

    static void             init();
    // Configura ImGui para el sistema de ventanas y registra los comandos.

    static void             draw_all();
    // Coloca las ventanas del layout activo y dibuja todas las abiertas.

    static editor_window*   find(const char* name);

protected:
    friend class editor_layout;     // acopla, minimiza en la barra y saca ventanas

    virtual void    on_draw() = 0;          // contenido de la ventana
    virtual void    on_show() {}
    virtual void    on_hide() {}
    virtual void    on_focus() {}
    virtual ImGuiWindowFlags window_flags() const { return 0; }
    // Flags de ImGui adicionales para la ventana (p. ej. NoScrollbar).
    virtual void    on_size_limits(ImVec2& min, ImVec2& max) const { (void)min; (void)max; }
    // Ajuste final de los límites de tamaño (px reales), ver get_size_limits().

private:
    void            draw(editor_layout* layout);
    ImGuiWindowFlags build_flags() const;

    // barra de título al estilo de Windows
    enum caption_icon { CAPTION_MINIMIZE, CAPTION_MAXIMIZE, CAPTION_RESTORE, CAPTION_CLOSE };
    void            draw_caption();
    bool            caption_button(const char* id, float x0, float x1, caption_icon icon, bool* hovered);

    // desplazamiento y acoplamiento
    static window_rect workspace();
    float           find_closest(int axis, float v, const window_rect& self, const window_rect& field, float snap) const;
    bool            snap_move(window_rect& r, const window_rect& field) const;
    static void     size_callback(ImGuiSizeCallbackData* data);
    void            snap_resize(ImGuiSizeCallbackData* data) const;
    static void     clamp_to_field(window_rect& r, const window_rect& field);

    std::string     name;           // identificador (comandos, editor.ini)
    std::string     title;          // texto de la barra de título
    std::string     imgui_id;       // "###name": ImGui no dibuja el título, lo dibuja draw_caption()
    unsigned        options;

    bool            open;
    bool            minimized;
    bool            focused;
    bool            maximized;
    bool            request_focus;

    ImVec2          default_pos;    // fracciones del área de trabajo
    ImVec2          default_size;
    ImVec2          min_size;       // px a escala 1.0
    ImVec2          max_size;       // px a escala 1.0 (0 = sin límite)
    ImVec2          content_size;   // tamaño que necesita el contenido (px reales)

    window_rect     rect;           // posición en pantalla del último frame
    int             dock_region;    // layout_region en el frame actual (0 = flotante)
    ImVec2          float_size;     // último tamaño como flotante (0 = sin usar)
    ImVec2          request_size;   // tamaño a aplicar en el próximo frame (0 = nada)
    ImVec2          request_pos;    // posición a aplicar en el próximo frame (FLT_MAX = nada)
    ImVec2          restore_pos;    // posición como flotante antes de maximizar
    mutable bool    dock_resizing;  // el usuario arrastra el borde interior (lo marca snap_resize)
};
