// style.cpp -- estilos visuales del editor

#include "tools/editor/style.h"

editor_style ed_style;

static ImVec4 Style_Color(int r, int g, int b, float a = 1.0f)
{
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a);
}

/*
==================
editor_style

Tema por defecto: grises oscuros con acento ámbar
==================
*/
editor_style::editor_style()
{
    background          = Style_Color( 24,  24,  27);

    text                = Style_Color(222, 222, 222);
    text_disabled       = Style_Color(120, 120, 125);

    window_bg           = Style_Color( 36,  36,  40);
    child_bg            = Style_Color( 30,  30,  33);
    popup_bg            = Style_Color( 40,  40,  44);
    border              = Style_Color( 62,  62,  68);

    title_bg            = Style_Color( 44,  44,  49);
    title_bg_active     = Style_Color( 64,  52,  34);
    title_bg_collapsed  = Style_Color( 44,  44,  49, 0.85f);

    frame_bg            = Style_Color( 26,  26,  29);
    frame_bg_hovered    = Style_Color( 50,  50,  56);
    frame_bg_active     = Style_Color( 60,  60,  66);

    accent              = Style_Color(214, 140,  38);
    accent_hovered      = Style_Color(236, 162,  60);
    accent_active       = Style_Color(255, 184,  82);

    scrollbar_bg        = Style_Color( 30,  30,  33);
    scrollbar_grab      = Style_Color( 72,  72,  78);

    caption_hovered         = Style_Color(255, 255, 255, 0.10f);
    caption_active          = Style_Color(255, 255, 255, 0.18f);
    caption_close_hovered   = Style_Color(196,  43,  28);
    caption_close_active    = Style_Color(148,  34,  24);

    drop_preview        = Style_Color(214, 140,  38, 0.30f);

    console_bg_top      = Style_Color( 24,  19,  15);
    console_bg_bottom   = Style_Color( 52,  37,  25);
    console_text        = Style_Color(214, 208, 198);
    console_highlight   = Style_Color(214, 160,  88);
    console_input       = Style_Color(236, 232, 224);
    console_warning     = Style_Color(230, 200,  60);
    console_error       = Style_Color(240,  80,  70);
    console_version     = Style_Color(214, 160,  88, 0.70f);

    window_rounding     = 3.0f;
    window_border       = 1.0f;
    window_padding      = ImVec2(8.0f, 8.0f);
    window_min_size     = ImVec2(160.0f, 80.0f);
    frame_rounding      = 2.0f;
    frame_padding       = ImVec2(6.0f, 4.0f);
    item_spacing        = ImVec2(8.0f, 5.0f);
    caption_button_width = 34.0f;
    scrollbar_size      = 12.0f;
    scrollbar_rounding  = 2.0f;
    grab_rounding       = 2.0f;

    snap_move           = 10.0f;
    snap_resize         = 8.0f;

    dock_splitter       = 4.0f;
    dock_min_leaf       = 64.0f;
    dock_min_center     = 160.0f;
    dock_drop_button    = 32.0f;

    scale               = 1.0f;
}

/*
==================
editor_style::apply
==================
*/
void editor_style::apply(float dpi_scale)
{
    scale = dpi_scale;

    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle();
    ImGui::StyleColorsDark(&s);

// medidas
    s.WindowRounding    = window_rounding;
    s.WindowBorderSize  = window_border;
    s.WindowPadding     = window_padding;
    s.WindowMinSize     = window_min_size;
    s.WindowTitleAlign  = ImVec2(0.0f, 0.5f);
    s.WindowMenuButtonPosition = ImGuiDir_None;     // sin la flecha de ImGui: botones propios
    s.ChildRounding     = window_rounding;
    s.PopupRounding     = window_rounding;
    s.FrameRounding     = frame_rounding;
    s.FramePadding      = frame_padding;
    s.ItemSpacing       = item_spacing;
    s.ScrollbarSize     = scrollbar_size;
    s.ScrollbarRounding = scrollbar_rounding;
    s.GrabRounding      = grab_rounding;
    // con varias ventanas del sistema (viewports), una ventana que sale de la
    // principal debe verse igual que dentro: sin esquinas redondeadas
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        s.WindowRounding = 0.0f;
    s.ScaleAllSizes(dpi_scale);
    s.FontScaleDpi      = dpi_scale;

// colores
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                    = text;
    c[ImGuiCol_TextDisabled]            = text_disabled;
    c[ImGuiCol_WindowBg]                = window_bg;
    c[ImGuiCol_ChildBg]                 = child_bg;
    c[ImGuiCol_PopupBg]                 = popup_bg;
    c[ImGuiCol_Border]                  = border;
    c[ImGuiCol_BorderShadow]            = ImVec4(0, 0, 0, 0);

    c[ImGuiCol_TitleBg]                 = title_bg;
    c[ImGuiCol_TitleBgActive]           = title_bg_active;
    c[ImGuiCol_TitleBgCollapsed]        = title_bg_collapsed;
    c[ImGuiCol_MenuBarBg]               = title_bg;

    c[ImGuiCol_FrameBg]                 = frame_bg;
    c[ImGuiCol_FrameBgHovered]          = frame_bg_hovered;
    c[ImGuiCol_FrameBgActive]           = frame_bg_active;

    c[ImGuiCol_ScrollbarBg]             = scrollbar_bg;
    c[ImGuiCol_ScrollbarGrab]           = scrollbar_grab;
    c[ImGuiCol_ScrollbarGrabHovered]    = frame_bg_active;
    c[ImGuiCol_ScrollbarGrabActive]     = accent;

    c[ImGuiCol_CheckMark]               = accent;
    c[ImGuiCol_SliderGrab]              = accent;
    c[ImGuiCol_SliderGrabActive]        = accent_active;

    c[ImGuiCol_Button]                  = frame_bg_hovered;
    c[ImGuiCol_ButtonHovered]           = frame_bg_active;
    c[ImGuiCol_ButtonActive]            = accent;

    c[ImGuiCol_Header]                  = frame_bg_hovered;
    c[ImGuiCol_HeaderHovered]           = frame_bg_active;
    c[ImGuiCol_HeaderActive]            = accent;

    c[ImGuiCol_Separator]               = border;
    c[ImGuiCol_SeparatorHovered]        = accent_hovered;
    c[ImGuiCol_SeparatorActive]         = accent_active;

    c[ImGuiCol_ResizeGrip]              = ImVec4(accent.x, accent.y, accent.z, 0.25f);
    c[ImGuiCol_ResizeGripHovered]       = accent_hovered;
    c[ImGuiCol_ResizeGripActive]        = accent_active;

    // pestañas del dock: la seleccionada con el fondo de la ventana, como
    // si la ventana saliera de ella, y una línea del color de acento
    c[ImGuiCol_Tab]                     = frame_bg;
    c[ImGuiCol_TabHovered]              = frame_bg_active;
    c[ImGuiCol_TabSelected]             = window_bg;
    c[ImGuiCol_TabSelectedOverline]     = accent;
    c[ImGuiCol_TabDimmed]               = frame_bg;
    c[ImGuiCol_TabDimmedSelected]       = window_bg;
    c[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(accent.x, accent.y, accent.z, 0.50f);

    c[ImGuiCol_TextSelectedBg]          = ImVec4(accent.x, accent.y, accent.z, 0.35f);
    c[ImGuiCol_NavCursor]               = accent;
}
