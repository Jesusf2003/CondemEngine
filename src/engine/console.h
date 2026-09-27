// console.h -- salida de texto del motor

#pragma once

#include <string>
#include <vector>

#if defined(__GNUC__)
    #define CONDEM_PRINTF(fmt, args) __attribute__((format(printf, fmt, args)))
#else
    #define CONDEM_PRINTF(fmt, args)
#endif

// Marca al comienzo de una línea del historial: la línea está resaltada.
// Se pone sola cuando el texto de Con_Printf empieza por '\1' o '\2' (como en
// Quake, donde se dibujaba con la mitad dorada de la fuente).
#define CON_HIGHLIGHT   '\2'

void Con_Init(void);
// -condebug guarda todo lo que se imprime en qconsole.log.
// Registra "clear" y "condump [archivo]".

void Con_Printf(const char* fmt, ...) CONDEM_PRINTF(1, 2);
// Imprime en la salida del sistema y lo guarda en el historial de la consola.
// Admite códigos de color ANSI ("\033[31m", ver IMGUIQuakeConsole.h en
// docs/VirtuosoConsole) y '\1'/'\2' al principio para resaltar el texto.

void Con_DPrintf(const char* fmt, ...) CONDEM_PRINTF(1, 2);
// Igual que Con_Printf, solo si el cvar "developer" está activo.

void Con_Clear(void);

const std::vector<std::string>& Con_GetLines(void);
// Historial de líneas, para mostrarlo en la consola del editor. Conservan los
// códigos ANSI y la marca CON_HIGHLIGHT.
