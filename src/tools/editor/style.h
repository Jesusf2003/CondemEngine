// style.h -- estilos visuales del editor
//
// Todos los colores y medidas del editor se definen aquí, en un solo lugar.
// Las medidas están en píxeles a escala 1.0 (96 dpi); apply() las escala
// según el DPI del monitor.

#pragma once

#include "imgui.h"

class editor_style
{
public:
    editor_style();

    void apply(float dpi_scale);
    // Copia el estilo a ImGui::GetStyle() escalado por dpi_scale.

    float scaled(float px) const { return px * scale; }
    // Convierte una medida a escala 1.0 a píxeles reales.

    //------------------------------------------------------------------
    // Colores

    ImVec4  background;             // fondo del área de trabajo (clear de Vulkan)

    ImVec4  text;
    ImVec4  text_disabled;

    ImVec4  window_bg;
    ImVec4  child_bg;
    ImVec4  popup_bg;
    ImVec4  border;

    ImVec4  title_bg;               // ventana sin foco
    ImVec4  title_bg_active;        // ventana con foco
    ImVec4  title_bg_collapsed;     // ventana minimizada

    ImVec4  frame_bg;               // campos de texto, checkbox...
    ImVec4  frame_bg_hovered;
    ImVec4  frame_bg_active;

    ImVec4  accent;                 // color principal: selección, grips, sliders
    ImVec4  accent_hovered;
    ImVec4  accent_active;

    ImVec4  scrollbar_bg;
    ImVec4  scrollbar_grab;

    ImVec4  caption_hovered;        // botones de la barra de título (minimizar, maximizar)
    ImVec4  caption_active;
    ImVec4  caption_close_hovered;  // botón de cerrar, rojo como en Windows
    ImVec4  caption_close_active;

    ImVec4  drop_preview;           // vista previa de dónde quedará la ventana

    // consola, con la paleta de la consola de Quake
    ImVec4  console_bg_top;         // degradado del fondo (conback)
    ImVec4  console_bg_bottom;
    ImVec4  console_text;           // texto normal
    ImVec4  console_highlight;      // texto resaltado ('\1'/'\2'), la mitad dorada de la fuente
    ImVec4  console_input;          // eco de lo escrito en la consola ("]comando")
    ImVec4  console_warning;        // líneas con "warning"/"aviso"
    ImVec4  console_error;          // líneas con "error"
    ImVec4  console_version;        // versión en la esquina inferior derecha

    //------------------------------------------------------------------
    // Medidas (px a escala 1.0)

    float   window_rounding;
    float   window_border;
    ImVec2  window_padding;
    ImVec2  window_min_size;        // tamaño mínimo por defecto de las ventanas
    float   frame_rounding;
    ImVec2  frame_padding;
    ImVec2  item_spacing;
    float   caption_button_width;   // ancho de los botones de la barra de título
    float   scrollbar_size;
    float   scrollbar_rounding;
    float   grab_rounding;

    //------------------------------------------------------------------
    // Desplazamiento de ventanas (ver window.cpp)

    float   snap_move;              // distancia para acoplar un borde al mover
    float   snap_resize;            // distancia para acoplar un borde al redimensionar

    //------------------------------------------------------------------
    // Acoplamiento (ver dock.cpp)

    float   dock_splitter;          // grosor del separador entre dos nodos
    float   dock_min_leaf;          // lado mínimo de una hoja
    float   dock_min_center;        // lado mínimo de la hoja central vacía
    float   dock_drop_button;       // lado de los botones de destino al arrastrar

    //------------------------------------------------------------------

    float   scale;                  // escala DPI usada en el último apply()
};

extern editor_style ed_style;
