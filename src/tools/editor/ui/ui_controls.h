// ui_controls.h -- controles básicos
//
//   ui_label          texto fijo o calculado cada frame
//   ui_button         botón (normal o pequeño)
//   ui_separator      línea horizontal, vertical o con título
//   ui_checkbox       casilla ligada a un bool o a get/set
//   ui_text_input     campo de texto ligado a un buffer o std::string
//   ui_popup_button   botón que abre un popup con sus hijos
//   ui_menu_bar       barra de menús (principal o de la ventana)
//   ui_menu           menú o submenú con sus hijos
//   ui_menu_item      opción de menú, con casilla opcional

#pragma once

#include "tools/editor/ui/ui_node.h"

/*
==============================================================================

    ui_label

==============================================================================
*/

class ui_label : public ui_fluent<ui_label>
{
public:
    typedef std::function<std::string()> text_fn;

    ptr text(const std::string& v)          { label_text = v; label_fn = nullptr; return self(); }
    ptr text(text_fn fn)                    { label_fn = std::move(fn); return self(); }
    // texto calculado cada frame (valores que cambian: FPS, contadores...)
    ptr color(const ImVec4& v)              { label_color = v; return self(); }
    ptr wrap(bool v = true)                 { label_wrap = v; return self(); }
    // salto de línea al ancho del nodo (width) o al borde de la ventana
    ptr align_to_frame(bool v = true)       { label_align = v; return self(); }
    // alineado verticalmente con botones y campos en un ui_hbox

    std::string get_text() const            { return label_fn ? label_fn() : label_text; }

protected:
    explicit ui_label(std::string text = "") : label_text(std::move(text)) {}
    explicit ui_label(text_fn fn) : label_fn(std::move(fn)) {}

    void render() override;

    std::string label_text;
    text_fn     label_fn;
    ImVec4      label_color = ImVec4(0, 0, 0, 0);   // alfa 0 = color del estilo
    bool        label_wrap = false;
    bool        label_align = false;
};

/*
==============================================================================

    ui_button

==============================================================================
*/

class ui_button : public ui_fluent<ui_button>
{
public:
    ptr text(const std::string& v)  { label = v; return self(); }
    ptr compact(bool v = true)      { small_button = v; return self(); }
    // botón sin margen vertical (ImGui::SmallButton)

protected:
    explicit ui_button(std::string text) : label(std::move(text)) {}

    void render() override;
    bool handles_click() const override { return true; }

    std::string label;
    bool        small_button = false;
};

/*
==============================================================================

    ui_separator

==============================================================================
*/

class ui_separator : public ui_fluent<ui_separator>
{
public:
    ptr vertical(bool v = true)     { is_vertical = v; return self(); }
    // línea vertical, para separar elementos de un ui_hbox

protected:
    explicit ui_separator(std::string title = "") : title(std::move(title)) {}

    void render() override;

    std::string title;      // no vacío: separador con texto (SeparatorText)
    bool        is_vertical = false;
};

/*
==============================================================================

    ui_checkbox

==============================================================================
*/

class ui_checkbox : public ui_fluent<ui_checkbox>
{
public:
    ptr bind(bool* v)                                           { value_ptr = v; return self(); }
    ptr bind(std::function<bool()> get, std::function<void(bool)> set)
    {
        getter = std::move(get);
        setter = std::move(set);
        return self();
    }
    ptr on_change(std::function<void(bool)> f)                  { changed = std::move(f); return self(); }

    bool is_checked() const;

protected:
    explicit ui_checkbox(std::string text) : label(std::move(text)) {}

    void render() override;
    bool handles_click() const override { return true; }

    std::string                 label;
    bool                        value = false;          // si no está ligada
    bool*                       value_ptr = nullptr;
    std::function<bool()>       getter;
    std::function<void(bool)>   setter;
    std::function<void(bool)>   changed;
};

/*
==============================================================================

    ui_text_input

==============================================================================
*/

class ui_text_input : public ui_fluent<ui_text_input>
{
public:
    typedef std::function<void(const char*)> text_callback;

    ptr bind(char* buf, size_t size)        { buffer = buf; buffer_size = size; string_ptr = nullptr; return self(); }
    ptr bind(std::string* s)                { string_ptr = s; buffer = nullptr; return self(); }
    ptr hint(const std::string& v)          { hint_text = v; return self(); }
    ptr flags(ImGuiInputTextFlags v)        { input_flags = v; return self(); }
    ptr on_change(text_callback f)          { changed = std::move(f); return self(); }
    ptr on_submit(text_callback f)          { submitted = std::move(f); return self(); }
    // Enter; con on_submit el campo usa ImGuiInputTextFlags_EnterReturnsTrue

    const char* get_text() const;

protected:
    explicit ui_text_input(std::string hint = "") : hint_text(std::move(hint)) {}

    void render() override;

    std::string         hint_text;
    std::string         value;                  // si no está ligado
    char*               buffer = nullptr;
    size_t              buffer_size = 0;
    std::string*        string_ptr = nullptr;
    ImGuiInputTextFlags input_flags = 0;
    text_callback       changed;
    text_callback       submitted;
};

/*
==============================================================================

    ui_popup_button

==============================================================================
*/

class ui_popup_button : public ui_container<ui_popup_button>
{
public:
    ptr compact(bool v = true)      { small_button = v; return self(); }

protected:
    explicit ui_popup_button(std::string text) : label(std::move(text)) {}

    void render() override;
    bool is_composite() const override  { return false; }  // en la ventana solo queda el botón

    std::string label;
    bool        small_button = false;
};

/*
==============================================================================

    ui_menu_bar / ui_menu / ui_menu_item

==============================================================================
*/

class ui_menu_bar : public ui_container<ui_menu_bar>
{
protected:
    explicit ui_menu_bar(bool main = true) : main_bar(main) {}
    // main: barra principal de la aplicación; si no, la de la ventana actual
    // (con ImGuiWindowFlags_MenuBar)

    void render() override;
    bool is_composite() const override  { return false; }

    bool main_bar;
};

class ui_menu : public ui_container<ui_menu>
{
protected:
    explicit ui_menu(std::string text) : label(std::move(text)) {}

    void render() override;
    bool is_composite() const override  { return false; }

    std::string label;
};

class ui_menu_item : public ui_fluent<ui_menu_item>
{
public:
    ptr shortcut(const std::string& v)  { shortcut_text = v; return self(); }
    ptr checked(bool* v)                { value_ptr = v; return self(); }
    ptr checked(std::function<bool()> get, std::function<void(bool)> set)
    {
        getter = std::move(get);
        setter = std::move(set);
        return self();
    }
    // con casilla: se marca o desmarca al elegir la opción

protected:
    explicit ui_menu_item(std::string text) : label(std::move(text)) {}

    void render() override;
    bool handles_click() const override { return true; }

    std::string                 label;
    std::string                 shortcut_text;
    bool*                       value_ptr = nullptr;
    std::function<bool()>       getter;
    std::function<void(bool)>   setter;
};
