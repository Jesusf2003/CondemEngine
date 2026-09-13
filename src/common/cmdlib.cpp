#include "cmdlib.h"
#include <SDL3/SDL.h>

int com_argc;
char *com_argv[MAX_NUM_ARGVS + NUM_SAFE_ARGVS + 1];

void InitArgv(int argc, char **argv)
{
    if (argc > MAX_NUM_ARGVS)
        argc = MAX_NUM_ARGVS;

    com_argc = argc;
    for (int i = 0; i < argc; i++)
        com_argv[i] = argv[i];
    com_argv[com_argc] = nullptr;
}

// Devuelve el indice en com_argv donde aparece 'parm', o 0 si no se encontro.
// El indice es util para leer el argumento SIGUIENTE, ej:
//   int i = COM_CheckParm("-game");
//   if (i && i + 1 < com_argc) FS_SetGameMod(com_argv[i + 1]);
int COM_CheckParm(const char *parm)
{
    for (int i = 1; i < com_argc; i++)
    {
        if (!com_argv[i]) continue;
        if (!SDL_strcmp(parm, com_argv[i]))
            return i;
    }
    return 0;
}

void COM_AddParm(const char *parm)
{
    if (com_argc >= MAX_NUM_ARGVS + NUM_SAFE_ARGVS)
    {
        SDL_Log("COM_AddParm: limite de parametros excedido, se ignora '%s'", parm);
        return;
    }
    com_argv[com_argc++] = (char *)parm;
}