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
//     se activan o desactivan por ventana con set_option(). Solo limitan a la
//     ventana acoplada: flotante funciona siempre como una ventana normal
//     (se mueve, se redimensiona, se minimiza, se maximiza y se cierra)
//   - desplazamiento: al mover o redimensionar, los bordes se acoplan a los
//     bordes de las otras ventanas y del área de trabajo (TryDocking de QE3).
//     Con viewports (ImGuiConfigFlags_ViewportsEnable) una ventana flotante
//     puede salir de la ventana principal como ventana del sistema hija de
//     ella; sin viewports nunca sale del área de trabajo
//   - acoplamiento: si la ventana está en el árbol del dock activo, el dock
//     decide su posición y tamaño y la muestra como una pestaña de su hoja.
//     Se acopla arrastrándola sobre los destinos del dock y se saca
//     arrastrando su pestaña (ver dock.h)

#pragma once

#include "imgui.h"

#include <string>
#include <vector>

class editor_dock;
struct ImGuiSettingsHandler;

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
    WND_MAX_CONTENT     = 1 << 7,   // se ajusta a lo que ocupa el contenido; si es
                                    // redimensionable, el usuario puede agrandarla
    WND_MAXIMIZABLE     = 1 << 9,   // botón de maximizar/restaurar

    WND_DEFAULT         = WND_MINIMIZABLE | WND_MAXIMIZABLE | WND_MOVABLE | WND_RESIZABLE | WND_TITLEBAR
                        | WND_SNAP
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
    bool        is_focused() const      { return focused; }
    bool        is_docked() const;

    void        minimize();
    bool        is_minimized() const    { return minimized; }
    // Flotante: queda solo su barra de título. Acoplada no se minimiza (se
    // cierra o se cambia de pestaña).

    void        maximize();
    void        toggle_maximize();
    bool        is_maximized() const    { return maximized; }
    bool        can_maximize() const;
    // Ocupa toda el área de trabajo de la ventana principal, encima de las
    // acopladas. No se puede si su tamaño máximo es menor que el área.

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

    //------------------------------------------------------------------
    // Posición

    void        set_default_rect(float x, float y, float w, float h);
    // Posición y tamaño iniciales, en fracciones (0..1) del área de trabajo.
    // Solo se usa si editor.ini no tiene guardada la ventana. El tamaño
    // también decide cuánto ocupa al acoplarla si nunca ha sido flotante.

    //------------------------------------------------------------------
    // Límites de tamaño
    //
    // Se combinan tres fuentes, de menor a mayor prioridad:
    //   1. por código: set_min_size() / set_max_size()
    //   2. por contenido: opciones WND_MIN_CONTENT / WND_MAX_CONTENT, que usan
    //      el tamaño que necesita el contenido (como el auto-ajuste de ImGui)
    //   3. on_size_limits(): cada clase de ventana puede ajustarlos a su gusto
    // Se aplican a las ventanas flotantes. Acopladas, el mínimo limita cuánto
    // se puede achicar su hoja del dock y el máximo cuánto la agranda el dock.
    //
    // WND_MAX_CONTENT en una ventana que se puede redimensionar (flotante
    // siempre; acoplada, con WND_RESIZABLE) no es un tope: es el tamaño al que
    // se ajusta hasta que el usuario la redimensiona (el borde flotante o el
    // separador del dock). Entonces solo la limita el máximo por código.
    // fit_to_content() (o el comando fitwindow) la vuelve a ajustar.

    void        set_min_size(float w, float h);
    // Tamaño mínimo en px a escala 1.0 (por defecto ed_style.window_min_size).

    void        set_max_size(float w, float h);
    // Tamaño máximo en px a escala 1.0; 0 = sin límite en ese eje (por defecto).

    void        get_size_limits(ImVec2& min, ImVec2& max, bool fit = false) const;
    // Límites efectivos en px reales (ya escalados). max usa FLT_MAX sin límite.
    // fit = false: los que no se pueden pasar; fit = true: con el tamaño de
    // ajuste (WND_MAX_CONTENT aunque sea redimensionable).

    void        fit_to_content();
    bool        is_user_sized() const   { return user_sized; }
    // Flotante redimensionada por el usuario: ya no se ajusta al contenido.

    ImVec2      get_content_size() const    { return content_size; }
    // Tamaño de ventana que necesita el contenido (0 hasta el primer frame).

    ImVec2      get_float_size() const      { return float_size; }
    // Último tamaño como flotante (0 si nunca lo ha sido).

    ImVec2      get_default_size() const    { return default_size; }
    // Tamaño inicial en fracciones del área de trabajo (set_default_rect).

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
    // Dibuja el dock activo y todas las ventanas abiertas.

    static editor_window*   find(const char* name);

protected:
    friend class editor_dock;       // saca una ventana arrastrando su pestaña

    virtual void    on_draw() = 0;          // contenido de la ventana
    virtual void    on_show() {}
    virtual void    on_hide() {}
    virtual void    on_focus() {}
    virtual ImGuiWindowFlags window_flags() const { return 0; }
    // Flags de ImGui adicionales para la ventana (p. ej. NoScrollbar).
    virtual void    on_size_limits(ImVec2& min, ImVec2& max) const { (void)min; (void)max; }
    // Ajuste final de los límites de tamaño (px reales), ver get_size_limits().

private:
    void            draw(editor_dock* dock);
    void            begin_float_drag(const ImVec2& grab);
    // La ventana acaba de salir del dock: flotante bajo el ratón, que la
    // mueve agarrada por grab (px desde su esquina superior izquierda).
    ImGuiWindowFlags build_flags() const;
    bool            is_floating() const;
    bool            allows(unsigned option) const;
    // Flotante: siempre; acoplada: según sus opciones.

    // barra de título al estilo de Windows
    enum caption_icon { CAPTION_MINIMIZE, CAPTION_MAXIMIZE, CAPTION_RESTORE, CAPTION_CLOSE };
    void            draw_caption();
    bool            caption_button(const char* id, float x0, float x1, caption_icon icon, bool* hovered);

    // desplazamiento y acoplamiento
    static window_rect workspace();
    float           find_closest(int axis, float v, const window_rect& self, const window_rect& field, float snap) const;
    bool            snap_move(window_rect& r, const window_rect& field) const;
    static void     size_callback(ImGuiSizeCallbackData* data);

    // editor.ini
    static void*    settings_read_open(ImGuiContext*, ImGuiSettingsHandler*, const char* name);
    static void     settings_read_line(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line);
    static void     settings_write_all(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf);
    void            snap_resize(ImGuiSizeCallbackData* data);
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
    bool            user_sized;     // flotante redimensionada por el usuario (editor.ini)

    window_rect     rect;           // posición en pantalla del último frame
    bool            docked;         // en el frame actual la coloca el dock
    ImVec2          float_size;     // último tamaño como flotante (0 = sin usar)
    ImVec2          request_size;   // tamaño a aplicar en el próximo frame (0 = nada)
    ImVec2          request_pos;    // posición a aplicar en el próximo frame (FLT_MAX = nada)
    ImVec2          restore_pos;    // posición como flotante antes de maximizar
    bool            drag_start;     // empezar a moverla con el ratón (begin_float_drag)
    ImVec2          drag_offset;
};
