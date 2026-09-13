#include "host.h"
#include "../common/cmdlib.h"
#include "../common/condemfs.h"
#include "../client/sys.h"   // <- agregar
#include <SDL3/SDL.h>

bool host_init = false;

void Init(condemparms_t *parms)
{
    FS_Init(parms);

    host_init = true;
    Sys_Printf("%s %s inicializado.\n", CONDEM_ENGINE_NAME, CONDEM_ENGINE_VERSION_STRING);
}

void Quit(void)
{
    if (!host_init) return;
    FS_Shutdown();
    host_init = false;
}