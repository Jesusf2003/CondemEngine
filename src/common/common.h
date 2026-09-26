// common.h -- utilidades generales compartidas por todo el motor

#pragma once

#include <string>

#include "condemdef.h"

//============================================================================
// Línea de comandos

extern int          com_argc;
extern char**       com_argv;

void COM_InitArgv(int argc, char** argv);

int COM_CheckParm(const char* parm);
// Devuelve la posición (1 a argc-1) del parámetro en la línea de comandos,
// o 0 si no está presente.

//============================================================================
// Parser

extern char com_token[1024];

const char* COM_Parse(const char* data);
// Lee el siguiente token de data y lo deja en com_token.
// Devuelve el puntero justo después del token, o nullptr al llegar al final.

//============================================================================
// Archivos

bool COM_LoadTextFile(const char* path, std::string& out);
// Lee un archivo de texto completo. Devuelve false si no se puede abrir.
