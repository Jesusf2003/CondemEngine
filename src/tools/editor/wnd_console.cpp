// wnd_console.cpp -- ventana de consola del editor
// Muestra el historial de Con_Printf y ejecuta lo que se escriba con Cbuf.
//
// El log se dibuja como la consola de Quake (docs/WinQuake/console.cpp):
// fondo marrón (conback) con la versión abajo a la derecha, texto resaltado
// en dorado, barras \35\36\37, prompt "]" con cursor "_" parpadeante y
// "^ ^ ^" cuando se está leyendo más arriba del final. Los colores ANSI se
// interpretan como en IMGUIQuakeConsole.h (docs/VirtuosoConsole).

#include "tools/editor/wnd_console.h"

#include "tools/editor/style.h"
#include "tools/editor/ui/ui.h"

#include "common/common.h"
#include "core/cmd.h"
#include "core/cvar.h"
#include "engine/console.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#define MAX_HISTORY         64
#define CON_CURSORSPEED     4.0f    // parpadeos del cursor por segundo (con_cursorspeed)

/*
==============================================================================

    Formato de las líneas

==============================================================================
*/

// trozo de línea con el mismo formato
struct con_segment
{
    std::string text;
    ImVec4      color;
    ImU32       background = 0;     // 0 = sin fondo
};

/*
==================
Con_AnsiColor

Colores ANSI 30-37 (40-47 para el fondo), como getAnsiTextColor de Virtuoso
==================
*/
static ImVec4 Con_AnsiColor(int index, bool bright)
{
    static const float rgb[8][3] =
    {
        {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
        {0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1},
    };
    const float k = bright ? 1.0f : 0.75f;

    return ImVec4(rgb[index][0] * k, rgb[index][1] * k, rgb[index][2] * k, 1.0f);
}

// letras iniciales que coinciden sin distinguir mayúsculas
static size_t Con_MatchLength(const char* a, const char* b)
{
    size_t i = 0;
    while (a[i] && b[i] && tolower((unsigned char)a[i]) == tolower((unsigned char)b[i]))
        i++;
    return i;
}

static bool Con_Contains(const std::string& text, const char* word)
{
    auto it = std::search(text.begin(), text.end(), word, word + strlen(word),
        [](char a, char b) { return tolower((unsigned char)a) == tolower((unsigned char)b); });
    return it != text.end();
}

/*
==================
Con_ParseLine

Separa una línea del historial en trozos con su color y devuelve el texto
visible. El color base sale de la línea (resaltada, eco de un comando,
error, aviso) y los códigos ANSI lo cambian a partir de donde aparecen.
==================
*/
static std::string Con_ParseLine(const std::string& line, std::vector<con_segment>& segments)
{
    std::string plain;
    size_t      i = 0;
    bool        highlight = false;

    segments.clear();

    if (!line.empty() && line[0] == CON_HIGHLIGHT)
    {
        highlight = true;
        i = 1;
    }

    con_segment seg;
    for (; i < line.size(); i++)
    {
        const char c = line[i];

        if (c == '\033' && i + 1 < line.size() && line[i + 1] == '[')
        {
            // ESC [ n ; n ... m
            size_t end = line.find('m', i + 2);
            if (end == std::string::npos)
                break;

            std::vector<int> codes;
            int value = 0;
            bool digits = false;
            for (size_t j = i + 2; j <= end; j++)
            {
                if (isdigit((unsigned char)line[j]))
                {
                    value = value * 10 + (line[j] - '0');
                    digits = true;
                }
                else
                {
                    codes.push_back(digits ? value : 0);
                    value = 0;
                    digits = false;
                }
            }

            if (!seg.text.empty())
                segments.push_back(seg);
            seg.text.clear();

            const bool bright = std::find(codes.begin(), codes.end(), 1) != codes.end();
            for (int code : codes)
            {
                if (code == 0)
                {
                    seg.color = ImVec4(0, 0, 0, 0);     // color base, se asigna al final
                    seg.background = 0;
                }
                else if (code >= 30 && code <= 37)
                    seg.color = Con_AnsiColor(code - 30, bright);
                else if (code >= 40 && code <= 47)
                    seg.background = code == 40 ? 0 : ImGui::GetColorU32(Con_AnsiColor(code - 40, true));
            }

            i = end;
            continue;
        }

        if ((unsigned char)c < ' ' && c != '\35' && c != '\36' && c != '\37')
            continue;   // otros caracteres de control no se dibujan

        seg.text += c;
        plain += c;
    }
    if (!seg.text.empty() || segments.empty())
        segments.push_back(seg);

    // color base de la línea
    ImVec4 base = ed_style.console_text;
    if (highlight)
        base = ed_style.console_highlight;
    else if (!plain.empty() && plain[0] == ']')
        base = ed_style.console_input;
    else if (Con_Contains(plain, "error"))
        base = ed_style.console_error;
    else if (Con_Contains(plain, "warning") || Con_Contains(plain, "aviso"))
        base = ed_style.console_warning;

    for (con_segment& s : segments)
        if (s.color.w == 0.0f)
            s.color = base;

    return plain;
}

/*
==================
Con_IsBar

Las líneas hechas solo de \35\36...\37 son las barras de Quake
(Con_NotifyBox, separadores)
==================
*/
static bool Con_IsBar(const std::string& plain)
{
    if (plain.empty())
        return false;
    for (char c : plain)
        if (c != '\35' && c != '\36' && c != '\37')
            return false;
    return true;
}

/*
==================
Con_DrawLine

Dibuja una línea ya separada por Con_ParseLine
==================
*/
static void Con_DrawLine(const std::string& plain, const std::vector<con_segment>& segments)
{
    if (Con_IsBar(plain))
    {
        // ╞══════╡ dorado, del ancho de los caracteres
        const float     char_w = ImGui::CalcTextSize("=").x;
        const float     h = ImGui::GetTextLineHeight();
        const ImVec2    p = ImGui::GetCursorScreenPos();
        const float     w = char_w * plain.size();
        const float     y = p.y + h * 0.5f;
        const float     cap = ed_style.scaled(3.0f);     // lado de los remates
        const ImU32     col = ImGui::GetColorU32(ed_style.console_highlight);
        ImDrawList*     dl = ImGui::GetWindowDrawList();

        dl->AddLine(ImVec2(p.x + cap * 0.5f, y), ImVec2(p.x + w - cap * 0.5f, y), col, ed_style.scaled(2.0f));
        dl->AddRectFilled(ImVec2(p.x, y - cap), ImVec2(p.x + cap, y + cap), col);
        dl->AddRectFilled(ImVec2(p.x + w - cap, y - cap), ImVec2(p.x + w, y + cap), col);
        ImGui::Dummy(ImVec2(w, h));
        return;
    }

    for (size_t i = 0; i < segments.size(); i++)
    {
        const con_segment& s = segments[i];
        const char* begin = s.text.c_str();
        const char* end = begin + s.text.size();

        if (s.background)
        {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const ImVec2 size = ImGui::CalcTextSize(begin, end);
            ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), s.background);
        }

        ImGui::PushStyleColor(ImGuiCol_Text, s.color);
        ImGui::TextUnformatted(begin, end);
        ImGui::PopStyleColor();
        if (i + 1 < segments.size())
            ImGui::SameLine(0.0f, 0.0f);
    }
}

/*
==============================================================================

    console_window

==============================================================================
*/

/*
==================
console_window
==================
*/
console_window::console_window() :
    editor_window("console", "Consola", WND_DEFAULT | WND_CLOSABLE),
    history_pos(-1), last_line_count(0),
    auto_scroll(true), at_bottom(true), scroll_to_end(true), reclaim_focus(false), copy_request(false)
{
    input[0] = 0;

    // flotante: abajo, a todo el ancho (acoplada, la coloca el dock)
    set_default_rect(0.0f, 0.60f, 1.0f, 0.40f);
    set_min_size(240.0f, 120.0f);

    root = ui_border_pane::create()
        ->center(ui_custom::create([this] { draw_log(); }))
        ->region_flags(UI_CENTER, ImGuiWindowFlags_HorizontalScrollbar)
        ->bottom(ui_custom::create([this] { draw_input(); }));
}

/*
==================
console_window::on_show
==================
*/
void console_window::on_show()
{
    scroll_to_end = true;
    reclaim_focus = true;
}

/*
==================
console_window::input_callback
==================
*/
int console_window::input_callback(ImGuiInputTextCallbackData* data)
{
    console_window* con = static_cast<console_window*>(data->UserData);

    if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
        con->complete(data);
    else if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
        con->browse_history(data);
    return 0;
}

/*
==================
console_window::complete

TAB: completa la palabra bajo el cursor con comandos y cvars. Si hay varios
candidatos, completa la parte común y los lista en la consola.
==================
*/
void console_window::complete(ImGuiInputTextCallbackData* data)
{
    // palabra que termina en el cursor
    const char* word_end = data->Buf + data->CursorPos;
    const char* word_start = word_end;
    while (word_start > data->Buf && word_start[-1] != ' ' && word_start[-1] != ';')
        word_start--;

    const std::string word(word_start, word_end);
    if (word.empty())
        return;

    std::vector<const char*> candidates;
    Cmd_CompleteCommandList(word.c_str(), candidates);
    for (cvar_t* var = cvar_vars; var; var = var->next)
        if (Con_MatchLength(var->name, word.c_str()) == word.size())
            candidates.push_back(var->name);

    if (candidates.empty())
        return;

    // parte común a todos los candidatos
    size_t match_len = strlen(candidates[0]);
    for (const char* c : candidates)
        match_len = std::min(match_len, Con_MatchLength(c, candidates[0]));

    const int start = (int)(word_start - data->Buf);
    data->DeleteChars(start, (int)(word_end - word_start));
    data->InsertChars(start, candidates[0], candidates[0] + match_len);

    if (candidates.size() == 1)
    {
        data->InsertChars(data->CursorPos, " ");
        return;
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const char* a, const char* b) { return strcmp(a, b) < 0; });

    Con_Printf("]%s\n", data->Buf);
    for (const char* c : candidates)
    {
        if (cvar_t* var = Cvar_FindVar(c))
            Con_Printf("  %s \"%s\"\n", c, var->string.c_str());
        else
            Con_Printf("  %s\n", c);
    }
    scroll_to_end = true;
}

/*
==================
console_window::browse_history
==================
*/
void console_window::browse_history(ImGuiInputTextCallbackData* data)
{
    const int prev = history_pos;

    if (data->EventKey == ImGuiKey_UpArrow)
    {
        if (history_pos == -1)
            history_pos = (int)history.size() - 1;
        else if (history_pos > 0)
            history_pos--;
    }
    else if (data->EventKey == ImGuiKey_DownArrow)
    {
        if (history_pos != -1 && ++history_pos >= (int)history.size())
            history_pos = -1;
    }

    if (prev != history_pos)
    {
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, history_pos >= 0 ? history[history_pos].c_str() : "");
    }
}

/*
==================
console_window::submit
==================
*/
void console_window::submit()
{
    // sin espacios al final
    size_t len = strlen(input);
    while (len && input[len - 1] == ' ')
        input[--len] = 0;

    if (len)
    {
        Con_Printf("]%s\n", input);     // eco como en Quake
        Cbuf_AddText(input);
        Cbuf_AddText("\n");

        if (history.empty() || history.back() != input)
            history.push_back(input);
        if (history.size() > MAX_HISTORY)
            history.erase(history.begin());
    }

    input[0] = 0;
    history_pos = -1;
    reclaim_focus = true;
    scroll_to_end = true;
}

/*
==================
console_window::draw_log
==================
*/
void console_window::draw_log()
{
    bool copy = copy_request;
    copy_request = false;

    const std::vector<std::string>& lines = Con_GetLines();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (ImGui::BeginPopupContextWindow())
    {
        if (ImGui::Selectable("Limpiar"))
            Con_Clear();
        if (ImGui::Selectable("Copiar"))
            copy = true;
        ImGui::EndPopup();
    }

    // canal 0: versión, debajo del texto (canal 1)
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
    if (copy)
        ImGui::LogToClipboard();

    std::vector<con_segment> segments;      // se reutiliza para todas las líneas
    if (filter.IsActive() || copy)
    {
        // con filtro no se sabe la altura total; al copiar hacen falta todas
        for (const std::string& line : lines)
        {
            const std::string plain = Con_ParseLine(line, segments);
            if (filter.PassFilter(plain.c_str(), plain.c_str() + plain.size()))
                Con_DrawLine(plain, segments);
        }
    }
    else
    {
        ImGuiListClipper clipper;
        clipper.Begin((int)lines.size());
        while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                const std::string plain = Con_ParseLine(lines[i], segments);
                Con_DrawLine(plain, segments);
            }
    }

    if (copy)
        ImGui::LogFinish();
    ImGui::PopStyleVar();

    // bajar al final al escribir un comando, o con líneas nuevas si ya se
    // estaba al final (si se está leyendo más arriba, no se mueve)
    const bool new_lines = lines.size() != last_line_count;
    if (scroll_to_end || (auto_scroll && new_lines && at_bottom))
        ImGui::SetScrollHereY(1.0f);
    scroll_to_end = false;
    last_line_count = lines.size();
    at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;

    // área visible del log (sin barras de scroll)
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const ImRect view = window->InnerRect;
    const float line_h = ImGui::GetTextLineHeightWithSpacing();

    // "^   ^   ^" en la última fila si no se está al final (QuakeWorld)
    if (!at_bottom)
    {
        const float y = view.Max.y - line_h;
        dl->AddRectFilled(ImVec2(view.Min.x, y), view.Max, ImGui::GetColorU32(ed_style.console_bg_bottom));

        const float step = ImGui::CalcTextSize("^   ").x;
        const ImU32 col = ImGui::GetColorU32(ed_style.console_highlight);
        for (float x = view.Min.x + ImGui::GetStyle().WindowPadding.x; x < view.Max.x - step * 0.5f; x += step)
            dl->AddText(ImVec2(x, y), col, "^");
    }

    // versión abajo a la derecha, como la que Quake escribe en conback
    dl->ChannelsSetCurrent(0);
    {
        const char* version = ENGINE_NAME " " ENGINE_VERSION;
        const ImVec2 size = ImGui::CalcTextSize(version);
        const float y = view.Max.y - size.y - ed_style.scaled(2.0f) - (at_bottom ? 0.0f : line_h);
        dl->AddText(ImVec2(view.Max.x - size.x - ed_style.scaled(8.0f), y), ImGui::GetColorU32(ed_style.console_version), version);
    }
    dl->ChannelsMerge();
}

/*
==================
console_window::draw_input

Prompt "]" y cursor "_" parpadeante de Quake sobre un InputText sin marco
==================
*/
void console_window::draw_input()
{
    const ImVec4 clear(0, 0, 0, 0);

    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, ed_style.console_text);
    ImGui::TextUnformatted("]");
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, clear);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, clear);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, clear);
    ImGui::PushStyleColor(ImGuiCol_InputTextCursor, clear);     // se dibuja "_" en su lugar
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, ImGui::GetStyle().FramePadding.y));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool entered = ImGui::InputTextWithHint("##input", "comando (TAB autocompleta, flechas: historial)", input, sizeof(input),
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory,
        input_callback, this);

    // cursor de Quake: '_' que parpadea donde está el cursor de edición
    if (ImGuiInputTextState* state = ImGui::GetInputTextState(ImGui::GetItemID()))
    {
        if ((int)(ImGui::GetTime() * CON_CURSORSPEED) & 1)
        {
            const char* text = state->GetText();
            const int cursor = std::min(state->GetCursorPos(), state->TextLen);
            const ImVec2 min = ImGui::GetItemRectMin();
            const float x = min.x + ImGui::CalcTextSize(text, text + cursor).x - state->Scroll.x;
            const float y = min.y + ImGui::GetStyle().FramePadding.y;

            ImGui::GetWindowDrawList()->AddText(ImVec2(x, y), ImGui::GetColorU32(ed_style.console_text), "_");
        }
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(5);

    if (entered)
        submit();

    ImGui::SetItemDefaultFocus();
    if (reclaim_focus)
    {
        ImGui::SetKeyboardFocusHere(-1);
        reclaim_focus = false;
    }
}

/*
==================
console_window::draw_background

Fondo de la consola de Quake (conback) bajo el log y la línea de entrada. Se
dibuja en la ventana; las regiones del panel son ventanas hijas
transparentes que ImGui pinta encima.
==================
*/
void console_window::draw_background()
{
    const ImVec2 log_min = root->region_min(UI_CENTER);
    if (log_min.x == 0 && log_min.y == 0)
        return;     // el log no se dibujó (ventana sin espacio)

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const ImRect inner = window->InnerRect;
    const float top = log_min.y - ImGui::GetStyle().ItemSpacing.y * 0.5f;
    const ImU32 col_top = ImGui::GetColorU32(ed_style.console_bg_top);
    const ImU32 col_bottom = ImGui::GetColorU32(ed_style.console_bg_bottom);

    window->DrawList->PushClipRect(inner.Min, inner.Max, false);
    window->DrawList->AddRectFilledMultiColor(ImVec2(inner.Min.x, top), inner.Max,
        col_top, col_top, col_bottom, col_bottom);
    window->DrawList->PopClipRect();
}

/*
==================
console_window::on_draw
==================
*/
void console_window::on_draw()
{
    root->draw();
    draw_background();  // después: ya con las regiones de este frame
}
