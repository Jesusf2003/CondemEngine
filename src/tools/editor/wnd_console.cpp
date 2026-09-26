// wnd_console.cpp -- ventana de consola del editor
// Muestra el historial de Con_Printf y ejecuta lo que se escriba con Cbuf.

#include "tools/editor/wnd_console.h"

#include "core/cmd.h"
#include "core/cvar.h"
#include "engine/console.h"

/*
==================
console_window
==================
*/
console_window::console_window() :
    editor_window("console", "Consola"),
    scroll_to_end(true), reclaim_focus(false)
{
    input[0] = 0;

    // acoplada abajo, a todo el ancho del área de trabajo
    set_default_rect(0.0f, 0.60f, 1.0f, 0.40f);
    set_min_size(240.0f, 120.0f);
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
console_window::completion_callback

Autocompletado con TAB: primero comandos, luego cvars
==================
*/
int console_window::completion_callback(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag != ImGuiInputTextFlags_CallbackCompletion)
        return 0;

    const char* match = Cmd_CompleteCommand(data->Buf);
    if (!match)
        match = Cvar_CompleteVariable(data->Buf);
    if (match)
    {
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, match);
        data->InsertChars(data->CursorPos, " ");
    }
    return 0;
}

/*
==================
console_window::on_draw
==================
*/
void console_window::on_draw()
{
    const float footer = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginChild("##log", ImVec2(0, -footer), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
    {
        const std::vector<std::string>& lines = Con_GetLines();
        ImGuiListClipper clipper;
        clipper.Begin((int)lines.size());
        while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                ImGui::TextUnformatted(lines[i].c_str());

        // mantener el scroll abajo si ya estaba abajo
        if (scroll_to_end || ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        scroll_to_end = false;
    }
    ImGui::EndChild();
    ImGui::Separator();

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##input", "comando (TAB para autocompletar)", input, sizeof(input),
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion, completion_callback))
    {
        if (input[0])
        {
            Con_Printf("] %s\n", input);
            Cbuf_AddText(input);
            Cbuf_AddText("\n");
        }
        input[0] = 0;
        reclaim_focus = true;
        scroll_to_end = true;
    }
    ImGui::SetItemDefaultFocus();
    if (reclaim_focus)
    {
        ImGui::SetKeyboardFocusHere(-1);
        reclaim_focus = false;
    }
}
