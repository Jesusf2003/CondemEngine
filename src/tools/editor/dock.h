// dock.h -- acoplamiento de ventanas del editor
// Basado en imgui_dock (docs/imguiDock), a su vez de LumixEngine.
//
// El área de trabajo es un árbol binario de nodos:
//   - un contenedor divide su rectángulo en dos hijos, lado a lado o uno
//     encima de otro, con un separador arrastrable entre ellos
//   - una hoja tiene pestañas: una ventana por pestaña, se ve la activa
//   - la hoja central es el hueco libre del área de trabajo (la futura vista
//     3D). Siempre existe; se ve vacía mientras no tenga pestañas
//
//   +--------+-------------------+
//   | [info] |                   |
//   |        |      central      |
//   +--------+-------------------+
//   | [Consola] [otra pestaña]   |
//   +----------------------------+
//
// Uso:
//   - arrastrar una ventana flotante por su barra de título muestra los
//     destinos: los 5 botones del centro de la hoja bajo el ratón
//     (izquierda, derecha, arriba, abajo y pestaña) y los 4 de los bordes del
//     área de trabajo. Al soltar sobre uno, la ventana se acopla ahí. Con
//     Mayús pulsado no se acopla
//   - arrastrar una pestaña fuera de su barra saca la ventana del árbol y
//     queda flotante, siguiendo al ratón
//   - una ventana acoplada que se oculta (su X o View > Show) conserva su
//     sitio: al volver a mostrarla aparece en la misma hoja. Mientras una
//     hoja no tiene ventanas abiertas, su hermano ocupa su espacio
//   - doble clic en una pestaña: maximizar/restaurar la ventana
//
// El árbol se guarda en editor.ini ([Dock][nombre]).

#pragma once

#include "imgui.h"

#include <memory>
#include <string>
#include <vector>

class editor_window;
struct ImGuiSettingsHandler;
struct ImGuiTextBuffer;

enum dock_slot : int
{
    DOCK_LEFT,
    DOCK_RIGHT,
    DOCK_TOP,
    DOCK_BOTTOM,
    DOCK_TAB,

    DOCK_SLOT_COUNT
};

struct dock_node
{
    dock_node*                  parent = nullptr;
    std::unique_ptr<dock_node>  child[2];           // contenedor: 0 = izquierda/arriba, 1 = derecha/abajo
    bool                        vertical = false;   // hijos uno encima de otro (si no, lado a lado)
    float                       ratio = 0.5f;       // fracción del hijo 0

    std::vector<editor_window*> tabs;               // hoja: ventanas en orden (abiertas u ocultas)
    editor_window*              active = nullptr;   // pestaña visible
    editor_window*              select = nullptr;   // pestaña a seleccionar en el próximo frame
    bool                        central = false;    // hoja central

    // resultado del último arrange()
    ImVec2                      min = ImVec2(0, 0);
    ImVec2                      max = ImVec2(0, 0);
    bool                        visible = false;

    bool    is_leaf() const     { return !child[0]; }
    bool    has_open_tabs() const;
};

class editor_dock
{
public:
    explicit editor_dock(const char* name);
    ~editor_dock();

    editor_dock(const editor_dock&) = delete;
    editor_dock& operator=(const editor_dock&) = delete;

    //------------------------------------------------------------------
    // Árbol

    dock_node*  root() const        { return tree.get(); }
    dock_node*  central() const     { return central_node; }
    dock_node*  node_of(const editor_window* window) const;
    // Hoja que contiene la ventana (abierta u oculta), o nullptr si es flotante.

    void        dock(editor_window* window, dock_node* target, dock_slot slot, float fraction = 0.0f);
    // Acopla la ventana junto a target (nullptr = todo el área de trabajo) o
    // como pestaña suya. fraction: tamaño de la nueva hoja respecto a target
    // (0 = según el tamaño flotante de la ventana, entre el 20% y el 50%).
    // Si la ventana ya estaba acoplada, se mueve.

    void        undock(editor_window* window);
    // La saca del árbol: queda flotante. Una hoja vacía desaparece.

    void        activate(editor_window* window);
    // La hace pestaña visible de su hoja.

    bool        content_rect(const editor_window* window, ImVec2& min, ImVec2& max) const;
    // Rectángulo que ocupa la ventana acoplada (bajo la barra de pestañas).
    // false si no está acoplada o no es la pestaña visible de una hoja visible.

    //------------------------------------------------------------------
    // Cada frame, desde editor_window::draw_all()

    void        begin_frame(const ImVec2& pos, const ImVec2& size);
    // Coloca los nodos en el área y dibuja pestañas y separadores (en una
    // ventana de fondo, detrás de las acopladas). Gestiona el arrastre.

    void        end_frame();
    // Manda el fondo detrás de todo y dibuja los destinos de arrastre.

    void        print() const;      // árbol en la consola (dockinfo)
    const char* get_name() const    { return name.c_str(); }

    //------------------------------------------------------------------
    // Todos los docks

    static void         init();
    // Registra el guardado en editor.ini y los comandos. Llamar después de
    // ImGui::CreateContext() y antes del primer ImGui::NewFrame().

    static editor_dock* active()    { return active_dock; }
    static void         set_active(editor_dock* dock)   { active_dock = dock; }

private:
    // árbol
    dock_node*  find_leaf(dock_node* node, const editor_window* window) const;
    void        remove_leaf(dock_node* leaf);
    void        clear();
    void        reset_tree();

    // disposición
    ImVec2      min_size(const dock_node* node) const;
    void        arrange(dock_node* node, const ImVec2& pos, const ImVec2& size);
    float       tabbar_height() const;

    // dibujo
    void        draw_node(dock_node* node);
    void        draw_tabbar(dock_node* leaf);
    void        draw_splitter(dock_node* container);

    // arrastre
    void        update_drag();
    dock_node*  leaf_at(dock_node* node, const ImVec2& p) const;
    void        find_drop(const ImVec2& p);
    void        draw_drop_targets();
    ImVec2      preview_min(dock_node* target, dock_slot slot, ImVec2& max) const;

    // editor.ini
    void        write_node(ImGuiTextBuffer* buf, const dock_node* node) const;
    std::unique_ptr<dock_node> parse_node(const char*& p, dock_node* parent);
    std::vector<editor_window*> parse_seen;     // ventanas ya colocadas al leer

    static void*    settings_read_open(ImGuiContext*, ImGuiSettingsHandler*, const char* name);
    static void     settings_read_line(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line);
    static void     settings_write_all(ImGuiContext*, ImGuiSettingsHandler*, ImGuiTextBuffer* buf);

    std::string                 name;
    std::unique_ptr<dock_node>  tree;
    dock_node*                  central_node = nullptr;

    ImVec2                      area_min = ImVec2(0, 0);    // área de trabajo del último frame
    ImVec2                      area_max = ImVec2(0, 0);

    // arrastre
    editor_window*              drag_window = nullptr;      // flotante que se está moviendo
    dock_node*                  drop_node = nullptr;        // destino bajo el ratón (nullptr = raíz)
    dock_slot                   drop_slot = DOCK_SLOT_COUNT;// DOCK_SLOT_COUNT = ninguno
    editor_window*              tear_window = nullptr;      // pestaña arrastrada fuera de su barra
    ImVec2                      tear_offset = ImVec2(0, 0);

    static std::vector<editor_dock*>    docks;
    static editor_dock*                 active_dock;
};
