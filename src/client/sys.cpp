#include "sys.h"
#include "../common/condemdef.h"
#include "host.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>

double Sys_FloatTime()
{
    static Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 counter = SDL_GetPerformanceCounter();
    return (double) counter / (double) freq;
}

void Sys_Sleep(unsigned int ms)
{
    SDL_Delay(ms);
}

void Sys_Warn(const char* fmt, ...)
{
    char str[1024];
    va_list argptr;
    va_start(argptr, fmt);
    SDL_vsnprintf(str, sizeof(str), fmt, argptr);
    va_end(argptr);
    SDL_LogWarn(CONDEM_LOG_GENERAL, "%s", str);
}

void Sys_Printf(const char* fmt, ...)
{
    char str[1024];
    va_list argptr;
    va_start(argptr, fmt);
    SDL_vsnprintf(str, sizeof(str), fmt, argptr);
    va_end(argptr);
    SDL_LogInfo(CONDEM_LOG_GENERAL, "%s", str);
}

void Sys_DebugLog(const char* file, const char* fmt, ...)
{
    char msg[1024];
    va_list argptr;
    va_start(argptr, fmt);
    SDL_vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    SDL_IOStream *io = SDL_IOFromFile(file, "ab");
    if (!io) return;

    char line[1100];
    SDL_snprintf(line, sizeof(line), "[%.3f] %s\n", Sys_FloatTime(), msg);
    SDL_WriteIO(io, line, SDL_strlen(line));
    SDL_CloseIO(io);
}

[[noreturn]] void Sys_Error(const char* fmt, ...)
{
    char str[1024];
    va_list argptr;
    va_start(argptr, fmt);
    SDL_vsnprintf(str, sizeof(str), fmt, argptr);
    va_end(argptr);

    SDL_LogError(CONDEM_LOG_GENERAL, "FATAL: %s", str);
    Sys_DebugLog("engine_error.log", "FATAL: %s", str);

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, CONDEM_ENGINE_NAME " - Error fatal", str, nullptr);
    Quit();
    SDL_Quit();
    exit(1);
}

bool Sys_Mkdir(const char* path)
{
    if (SDL_CreateDirectory(path)) return true;
    SDL_PathInfo info;
    if (SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY) return true;
    Sys_Warn("Sys_Mkdir: no se pudo crear '%s': %s", path, SDL_GetError());
    return false;
}