// sys.h -- interfaz no portable del sistema
// Implementada por sys_sdl3.cpp o sys_sdl2.cpp según CONDEM_PLATFORM.

#pragma once

#if defined(__GNUC__)
    #define SYS_PRINTF(fmt, args) __attribute__((format(printf, fmt, args)))
    #define SYS_NORETURN          __attribute__((noreturn))
#else
    #define SYS_PRINTF(fmt, args)
    #define SYS_NORETURN          [[noreturn]]
#endif

void Sys_Init(void);
// Inicializa la plataforma (SDL sin subsistemas de vídeo).

void Sys_Shutdown(void);

SYS_NORETURN void Sys_Error(const char* error, ...) SYS_PRINTF(1, 2);
// Error fatal: lo muestra en la salida y en un cuadro de mensaje y termina.

void Sys_Printf(const char* fmt, ...) SYS_PRINTF(1, 2);
// Salida de texto del sistema (stdout).

double Sys_FloatTime(void);
// Segundos desde Sys_Init.

void Sys_Sleep(int msec);

const char* Sys_PlatformName(void);
// "SDL3" o "SDL2".
