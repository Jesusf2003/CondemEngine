#include <iostream>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_filesystem.h>

#include "common/cmdlib.h"
#include "common/condemdef.h"
#include "client/host.h"
#include "client/sys.h"

int main(int argc, char **argv)
{
    //SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    condemparms_t cdparms;

    InitArgv(argc, argv);

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        Sys_Error("Couldn't initialize SDL: %s", SDL_GetError());
    }

    char title[128];
    SDL_snprintf(title, sizeof(title), "%s - %s", CONDEM_ENGINE_NAME, CONDEM_ENGINE_VERSION_STRING);

    SDL_Window *window = SDL_CreateWindow(title, 800, 600, SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        Sys_Error("Couldn't create window: %s", SDL_GetError());
    }

    cdparms.basedir = SDL_GetCurrentDirectory();
    cdparms.binarydir = SDL_GetBasePath();
    cdparms.argc = com_argc;
    cdparms.argv = com_argv;

    if (cdparms.binarydir)
        Sys_Printf("Binary is located at %s", cdparms.binarydir);

    Init(&cdparms);

    int running = 1;
    SDL_Event event;
    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
                running = 0;
        }
    }

    Quit();

    SDL_free((void *)cdparms.basedir);

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}