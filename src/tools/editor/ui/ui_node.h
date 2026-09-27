// ui_node.h -- árbol de nodos de interfaz sobre Dear ImGui
//
// ImGui dibuja la interfaz cada frame con código imperativo. Estos nodos
// guardan la descripción de la interfaz en un árbol de objetos que se crea una
// vez y se dibuja cada frame con draw(): la estructura (qué hay y dónde) vive
// en el árbol y el dibujado sigue siendo el de ImGui.
//
//   ui_node                  base: id, visibilidad, opacidad, tamaño, márgenes,
//   |                        tooltip y eventos (on_click, on_hover)
//   +- ui_parent             nodo con hijos
//   |  +- ui_vbox, ui_hbox, ui_border_pane, ui_grid_pane      (ui_layout.h)
//   |  +- ui_popup_button, ui_menu_bar, ui_menu               (ui_controls.h)
//   +- ui_label, ui_button, ui_checkbox, ui_text_input...     (ui_controls.h)
//   +- ui_property_card                                       (ui_property_card.h)
//   +- ui_custom             dibujado propio con una función
//
// Los nodos se crean siempre con create() y se manejan con std::shared_ptr.
// Todos los setters devuelven el propio nodo, así que se encadenan:
//
//   auto panel = ui_vbox::create({
//       ui_label::create("Hola")->tooltip("saludo"),
//       ui_hbox::create({
//           ui_button::create("Aceptar")->on_click([] { ... }),
//           ui_button::create("Cancelar")->width(80),
//       })->spacing(4),
//   })->margin(8);
//
//   panel->draw();          // cada frame, dentro de una ventana de ImGui
//
// Para un control nuevo, heredar de ui_fluent<clase> (o de
// ui_container<clase> si tiene hijos) e implementar render(). Ver
// ui_property_card.h como ejemplo.

#pragma once

#include "imgui.h"

#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class ui_node;
class ui_parent;

typedef std::shared_ptr<ui_node>    ui_ptr;
typedef std::function<void()>       ui_callback;
typedef std::function<void(bool)>   ui_hover_callback;     // true al entrar, false al salir

struct ui_margins
{
    float   top = 0.0f;
    float   right = 0.0f;
    float   bottom = 0.0f;
    float   left = 0.0f;

    bool    empty() const { return top <= 0 && right <= 0 && bottom <= 0 && left <= 0; }
};

// Propiedades comunes a todos los nodos
struct ui_node_props
{
    std::string         id;                 // id de ImGui; vacío = uno único automático
    bool                visible = true;     // oculto: ni se dibuja ni ocupa espacio
    bool                disabled = false;   // se dibuja atenuado y no responde
    float               alpha = 1.0f;       // opacidad (0..1), se multiplica por la del padre

    // Tamaño con la convención de ImGui: 0 = automático, > 0 = px,
    // < 0 = hasta el borde derecho/inferior menos ese valor (-FLT_MIN = todo)
    float               width = 0.0f;
    float               height = 0.0f;
    ui_margins          margin;             // espacio alrededor del nodo (px)

    std::string         tooltip;
    ui_callback         on_click;           // clic izquierdo sobre el nodo
    ui_hover_callback   on_hover;           // el ratón entra o sale del nodo
};

/*
==============================================================================

    ui_node

==============================================================================
*/

class ui_node : public std::enable_shared_from_this<ui_node>
{
public:
    virtual ~ui_node() = default;

    ui_node(const ui_node&) = delete;
    ui_node& operator=(const ui_node&) = delete;

    bool                    draw();
    // Dibuja el nodo en la ventana de ImGui actual. Devuelve false si está
    // oculto. Aplica id, opacidad, desactivado, márgenes y eventos alrededor
    // de render().

    ui_node_props&          properties()            { return props; }
    const ui_node_props&    properties() const      { return props; }
    bool                    is_visible() const      { return props.visible; }
    bool                    is_hovered() const      { return hovered; }
    ui_parent*              get_parent() const      { return parent; }
    int                     get_uid() const         { return uid; }

protected:
    ui_node();

    virtual void    render() = 0;
    // Dibujado con ImGui. Se llama dentro del PushID del nodo.

    virtual bool    is_composite() const    { return false; }
    // true si render() dibuja varios elementos: el nodo se agrupa
    // (BeginGroup) para que el padre lo trate como uno solo.

    virtual bool    handles_click() const   { return false; }
    // true si el control llama él mismo a fire_click() (un botón, por
    // ejemplo) en lugar de detectar el clic sobre todo el nodo.

    ImVec2          item_size() const       { return ImVec2(props.width, props.height); }
    void            fire_click()            { if (props.on_click) props.on_click(); }

    ui_node_props   props;

private:
    friend class ui_parent;

    ui_parent*      parent = nullptr;
    int             uid;
    bool            hovered = false;
};

/*
==============================================================================

    ui_fluent

    Da a la clase T su create() y los setters encadenables comunes, que
    devuelven std::shared_ptr<T> para poder seguir con los setters de T.

==============================================================================
*/

// Permite a create() usar los constructores protegidos de T
template <class T>
struct ui_make_enabler final : T
{
    template <class... A>
    explicit ui_make_enabler(A&&... args) : T(std::forward<A>(args)...) {}
};

template <class T, class B = ui_node>
class ui_fluent : public B
{
public:
    typedef std::shared_ptr<T> ptr;

    template <class... A>
    static ptr create(A&&... args)
    {
        return std::make_shared<ui_make_enabler<T>>(std::forward<A>(args)...);
    }

    ptr id(const std::string& v)                { this->props.id = v; return self(); }
    ptr visible(bool v = true)                  { this->props.visible = v; return self(); }
    ptr disabled(bool v = true)                 { this->props.disabled = v; return self(); }
    ptr alpha(float v)                          { this->props.alpha = v; return self(); }
    ptr width(float v)                          { this->props.width = v; return self(); }
    ptr height(float v)                         { this->props.height = v; return self(); }
    ptr size(float w, float h)                  { this->props.width = w; this->props.height = h; return self(); }
    ptr margin(float all)                       { return margin(all, all, all, all); }
    ptr margin(float vertical, float horizontal){ return margin(vertical, horizontal, vertical, horizontal); }
    ptr margin(float top, float right, float bottom, float left)
    {
        this->props.margin = { top, right, bottom, left };
        return self();
    }
    ptr tooltip(const std::string& v)           { this->props.tooltip = v; return self(); }
    ptr on_click(ui_callback f)                 { this->props.on_click = std::move(f); return self(); }
    ptr on_hover(ui_hover_callback f)           { this->props.on_hover = std::move(f); return self(); }

protected:
    using B::B;

    ptr self() { return std::static_pointer_cast<T>(this->shared_from_this()); }
};

/*
==============================================================================

    ui_parent

==============================================================================
*/

class ui_parent : public ui_node
{
public:
    ~ui_parent() override;

    void                        add_child(ui_ptr child);
    // Si el hijo ya tenía padre, se quita de él.
    void                        add_children(std::initializer_list<ui_ptr> list);
    void                        remove_child(const ui_ptr& child);
    void                        clear_children();
    const std::vector<ui_ptr>&  get_children() const    { return children; }

protected:
    ui_parent() = default;

    void            render() override;      // dibuja los hijos en orden
    bool            is_composite() const override   { return true; }

    virtual void    on_child_removed(ui_node* child)    { (void)child; }

    std::vector<ui_ptr> children;
};

// Contenedor con create({ hijos... }) y add() encadenables
template <class T, class B = ui_parent>
class ui_container : public ui_fluent<T, B>
{
public:
    typedef std::shared_ptr<T> ptr;

    using ui_fluent<T, B>::create;
    static ptr create(std::initializer_list<ui_ptr> list)
    {
        ptr p = ui_fluent<T, B>::create();
        p->add_children(list);
        return p;
    }

    ptr add(ui_ptr child)                       { this->add_child(std::move(child)); return this->self(); }
    ptr add(std::initializer_list<ui_ptr> list) { this->add_children(list); return this->self(); }

protected:
    using ui_fluent<T, B>::ui_fluent;
};

/*
==============================================================================

    ui_custom

    Nodo cuyo dibujado es una función: para partes que no merece la pena
    convertir en control (un log, un gráfico...).

==============================================================================
*/

class ui_custom : public ui_fluent<ui_custom>
{
protected:
    explicit ui_custom(ui_callback fn) : draw_fn(std::move(fn)) {}

    void render() override              { if (draw_fn) draw_fn(); }
    bool is_composite() const override  { return true; }

    ui_callback draw_fn;
};
