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

    ImVec4  bar_bg;                 // barra de ventanas minimizadas del layout
    ImVec4  drop_zone;              // zonas donde se puede soltar una ventana
    ImVec4  drop_preview;           // vista previa de dónde quedará la ventana

    //------------------------------------------------------------------
    // Medidas (px a escala 1.0)

    float   window_rounding;
    float   window_border;
    ImVec2  window_padding;
    ImVec2  window_min_size;        // tamaño mínimo por defecto de las ventanas
    float   frame_rounding;
    ImVec2  frame_padding;
    ImVec2  item_spacing;
    float   scrollbar_size;
    float   scrollbar_rounding;
    float   grab_rounding;

    //------------------------------------------------------------------
    // Desplazamiento de ventanas (ver window.cpp)

    float   snap_move;              // distancia para acoplar un borde al mover
    float   snap_resize;            // distancia para acoplar un borde al redimensionar

    //------------------------------------------------------------------
    // Layout de la capa principal (ver layout.cpp)

    float   layout_min_region;      // grosor mínimo de left/right/top/bottom
    float   layout_min_center;      // tamaño mínimo que se reserva al centro
    float   layout_bar_padding;     // margen del texto en las pestañas de la barra
    float   layout_drop_edge;       // distancia del cursor al borde que activa el acoplamiento
    float   layout_drop_center;     // medio lado del cuadrado de soltar en el centro

    //------------------------------------------------------------------

    float   scale;                  // escala DPI usada en el último apply()
};

extern editor_style ed_style;
