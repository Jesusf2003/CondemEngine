// cvar.h -- variables de consola
// Basado en cvar.h de Quake (docs/WinQuake/cvar.h).

#pragma once

#include <cstdio>
#include <string>

/*

Las variables cvar_t guardan valores numéricos o de texto que se pueden
cambiar o mostrar desde la consola o desde el código de CondemC, y también
se pueden acceder directamente desde C++.

Basta con inicializar un cvar_t con los dos primeros campos, o añadir true
para las variables que se deben guardar en el archivo de configuración:

cvar_t  r_draworder = {"r_draworder", "1"};
cvar_t  scr_screensize = {"screensize", "1", true};

Los cvars se deben registrar antes de usarse. Normalmente todos se registran
en la función de inicialización de su sistema, antes de ejecutar comandos:
Cvar_RegisterVariable(&host_framerate);

El código C++ normalmente consulta el cvar directamente:
if (r_draworder.value)

o puede buscarlo por nombre:
if (Cvar_VariableValue("r_draworder"))

Desde la consola se accede de dos formas:
r_draworder         muestra el valor actual
r_draworder 0       cambia el valor a 0

Los cvars no pueden tener el mismo nombre que un comando.
*/

struct cvar_t
{
    const char*     name;
    std::string     string;
    bool            archive = false;    // true para guardarlo en config.cfg
    bool            server = false;     // notifica a los jugadores cuando cambia
    float           value = 0.0f;
    cvar_t*         next = nullptr;
};

void Cvar_Init(void);
// Registra los comandos del sistema de cvars (set, toggle, cvarlist).

void Cvar_RegisterVariable(cvar_t* variable);
// Registra un cvar que ya tiene nombre, valor y, opcionalmente, archive.

void Cvar_Set(const char* var_name, const char* value);
// Equivale a escribir "<name> <value>" en la consola.

void Cvar_SetValue(const char* var_name, float value);
// Convierte el valor a texto y llama a Cvar_Set.

float Cvar_VariableValue(const char* var_name);
// Devuelve 0 si no existe o no es numérico.

const char* Cvar_VariableString(const char* var_name);
// Devuelve una cadena vacía si no existe.

const char* Cvar_CompleteVariable(const char* partial);
// Intenta completar un nombre de variable parcial (autocompletado).
// Devuelve nullptr si no hay coincidencias.

bool Cvar_Command(void);
// La llama Cmd_ExecuteString cuando Cmd_Argv(0) no es un comando conocido.
// Devuelve true si era una variable y se ha procesado (mostrar o cambiar).

void Cvar_WriteVariables(FILE* f);
// Escribe líneas "variable valor" para todos los cvars con archive = true.

cvar_t* Cvar_FindVar(const char* var_name);

extern cvar_t* cvar_vars;
