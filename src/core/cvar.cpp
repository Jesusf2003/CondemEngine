// cvar.cpp -- seguimiento de variables dinámicas
// Basado en cvar.c de Quake (docs/WinQuake/cvar.cpp).

#include "core/cvar.h"
#include "core/cmd.h"
#include "engine/console.h"

#include <cstdlib>
#include <cstring>

#ifdef _WIN32
    #define strcasecmp  _stricmp
    #define strncasecmp _strnicmp
#else
    #include <strings.h>
#endif

cvar_t* cvar_vars;

/*
============
Cvar_FindVar
============
*/
cvar_t* Cvar_FindVar(const char* var_name)
{
    for (cvar_t* var = cvar_vars; var; var = var->next)
        if (!strcasecmp(var_name, var->name))
            return var;

    return nullptr;
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue(const char* var_name)
{
    cvar_t* var = Cvar_FindVar(var_name);
    if (!var)
        return 0;
    return (float)atof(var->string.c_str());
}

/*
============
Cvar_VariableString
============
*/
const char* Cvar_VariableString(const char* var_name)
{
    cvar_t* var = Cvar_FindVar(var_name);
    if (!var)
        return "";
    return var->string.c_str();
}

/*
============
Cvar_CompleteVariable
============
*/
const char* Cvar_CompleteVariable(const char* partial)
{
    size_t len = strlen(partial);

    if (!len)
        return nullptr;

// buscar en las variables
    for (cvar_t* cvar = cvar_vars; cvar; cvar = cvar->next)
        if (!strncasecmp(partial, cvar->name, len))
            return cvar->name;

    return nullptr;
}

/*
============
Cvar_Set
============
*/
void Cvar_Set(const char* var_name, const char* value)
{
    cvar_t* var = Cvar_FindVar(var_name);
    if (!var)
    {   // si pasa esto hay un error en el código C++
        Con_Printf("Cvar_Set: variable %s not found\n", var_name);
        return;
    }

    bool changed = var->string != value;

    var->string = value;
    var->value = (float)atof(var->string.c_str());

    if (var->server && changed)
    {
        // TODO: SV_BroadcastPrintf cuando exista el servidor
        Con_Printf("\"%s\" changed to \"%s\"\n", var->name, var->string.c_str());
    }
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue(const char* var_name, float value)
{
    char val[32];

    // "%g" evita los ceros sobrantes de "%f" (1 en lugar de 1.000000)
    snprintf(val, sizeof(val), "%g", value);
    Cvar_Set(var_name, val);
}

/*
============
Cvar_RegisterVariable

Añade una variable a la lista
============
*/
void Cvar_RegisterVariable(cvar_t* variable)
{
// comprobar si ya estaba definida
    if (Cvar_FindVar(variable->name))
    {
        Con_Printf("Can't register variable %s, allready defined\n", variable->name);
        return;
    }

// comprobar que no coincida con un comando
    if (Cmd_Exists(variable->name))
    {
        Con_Printf("Cvar_RegisterVariable: %s is a command\n", variable->name);
        return;
    }

    variable->value = (float)atof(variable->string.c_str());

// enlazar la variable
    variable->next = cvar_vars;
    cvar_vars = variable;
}

/*
============
Cvar_Command

Muestra o cambia variables desde la consola
============
*/
bool Cvar_Command(void)
{
// buscar en las variables
    cvar_t* v = Cvar_FindVar(Cmd_Argv(0));
    if (!v)
        return false;

// mostrar o cambiar la variable
    if (Cmd_Argc() == 1)
    {
        Con_Printf("\"%s\" is \"%s\"\n", v->name, v->string.c_str());
        return true;
    }

    Cvar_Set(v->name, Cmd_Argv(1));
    return true;
}

/*
============
Cvar_WriteVariables

Escribe líneas "variable valor" para todos los cvars con archive = true
============
*/
void Cvar_WriteVariables(FILE* f)
{
    for (cvar_t* var = cvar_vars; var; var = var->next)
        if (var->archive)
            fprintf(f, "%s \"%s\"\n", var->name, var->string.c_str());
}

/*
=============================================================================

                        COMANDOS DE CVAR

=============================================================================
*/

/*
============
Cvar_Set_f

set <variable> <valor>
============
*/
static void Cvar_Set_f(void)
{
    if (Cmd_Argc() != 3)
    {
        Con_Printf("set <variable> <value> : set a console variable\n");
        return;
    }
    Cvar_Set(Cmd_Argv(1), Cmd_Argv(2));
}

/*
============
Cvar_Toggle_f

toggle <variable> : alterna entre 0 y 1
============
*/
static void Cvar_Toggle_f(void)
{
    if (Cmd_Argc() != 2)
    {
        Con_Printf("toggle <variable> : toggle a console variable between 0 and 1\n");
        return;
    }

    cvar_t* v = Cvar_FindVar(Cmd_Argv(1));
    if (!v)
    {
        Con_Printf("toggle: variable %s not found\n", Cmd_Argv(1));
        return;
    }
    Cvar_Set(v->name, v->value ? "0" : "1");
}

/*
============
Cvar_List_f

cvarlist [parcial]
============
*/
static void Cvar_List_f(void)
{
    const char* partial = Cmd_Argv(1);
    size_t      len = strlen(partial);
    int         count = 0;

    for (cvar_t* var = cvar_vars; var; var = var->next)
    {
        if (len && strncasecmp(partial, var->name, len))
            continue;
        Con_Printf("%c%c %s \"%s\"\n",
            var->archive ? '*' : ' ',
            var->server ? 's' : ' ',
            var->name, var->string.c_str());
        count++;
    }
    Con_Printf("%i cvar(s)\n", count);
}

/*
============
Cvar_Init
============
*/
void Cvar_Init(void)
{
    Cmd_AddCommand("set", Cvar_Set_f);
    Cmd_AddCommand("toggle", Cvar_Toggle_f);
    Cmd_AddCommand("cvarlist", Cvar_List_f);
}
