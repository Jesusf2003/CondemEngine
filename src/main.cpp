// main.cpp -- punto de entrada de CondemEngine
//
//   CondemEngine              muestra un mensaje y termina
//   CondemEngine -editor      inicia el editor (ImGui + SDL + Vulkan)
//
// Los parámetros que empiezan con + se ejecutan como comandos de consola:
//   CondemEngine -editor +developer 1 +exec editor.cfg

#include "common/common.h"
#include "core/cmd.h"
#include "core/cvar.h"
#include "engine/console.h"
#include "sys/sys.h"
#include "tools/editor/editor.h"

int main(int argc, char* argv[])
{
    COM_InitArgv(argc, argv);
    Sys_Init();

    Cbuf_Init();
    Cmd_Init();
    Cvar_Init();
    Con_Init();

    // ejecutar los comandos +xxx de la línea de comandos
    Cbuf_AddText("stuffcmds\n");
    Cbuf_Execute();

    int ret = 0;
    // editor mode
    if (COM_CheckParm("-editor"))
    {
        ret = Editor_Main();
    }
    // engine mode
    else
    {
        Con_Printf("%s %s (%s, %d bits)\n", ENGINE_NAME, ENGINE_VERSION, Sys_PlatformName(), (int)(sizeof(void*) * 8));
        Con_Printf("Hola mundo! Usa -editor para iniciar el editor.\n");
    }

    Sys_Shutdown();
    return ret;
}
