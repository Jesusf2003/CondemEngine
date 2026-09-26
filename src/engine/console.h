// console.h -- salida de texto del motor

#pragma once

#include <string>
#include <vector>

#if defined(__GNUC__)
    #define CONDEM_PRINTF(fmt, args) __attribute__((format(printf, fmt, args)))
#else
    #define CONDEM_PRINTF(fmt, args)
#endif

void Con_Init(void);

void Con_Printf(const char* fmt, ...) CONDEM_PRINTF(1, 2);
// Imprime en la salida del sistema y lo guarda en el historial de la consola.

void Con_DPrintf(const char* fmt, ...) CONDEM_PRINTF(1, 2);
// Igual que Con_Printf, solo si el cvar "developer" está activo.

void Con_Clear(void);

const std::vector<std::string>& Con_GetLines(void);
// Historial de líneas, para mostrarlo en la consola del editor.
