// console.cpp -- salida de texto del motor

#include "engine/console.h"
#include "core/cmd.h"
#include "core/cvar.h"
#include "sys/sys.h"

#include <cstdarg>
#include <cstdio>

#define CON_MAX_LINES   1024

cvar_t  developer = {"developer", "0"};

static std::vector<std::string> con_lines;
static std::string              con_partial;    // línea en curso, aún sin '\n'

/*
================
Con_Clear_f
================
*/
static void Con_Clear_f(void)
{
    Con_Clear();
}

/*
================
Con_Init
================
*/
void Con_Init(void)
{
    Cvar_RegisterVariable(&developer);
    Cmd_AddCommand("clear", Con_Clear_f);
}

/*
================
Con_Print

Separa el texto en líneas y las añade al historial
================
*/
static void Con_Print(const char* text)
{
    Sys_Printf("%s", text);

    for (const char* p = text; *p; p++)
    {
        if (*p == '\n')
        {
            con_lines.push_back(con_partial);
            con_partial.clear();
            continue;
        }
        con_partial += *p;
    }

    if (con_lines.size() > CON_MAX_LINES)
        con_lines.erase(con_lines.begin(), con_lines.begin() + (con_lines.size() - CON_MAX_LINES));
}

/*
================
Con_Printf
================
*/
void Con_Printf(const char* fmt, ...)
{
    char    msg[4096];
    va_list argptr;

    va_start(argptr, fmt);
    vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    Con_Print(msg);
}

/*
================
Con_DPrintf

Solo imprime si el cvar "developer" está activo
================
*/
void Con_DPrintf(const char* fmt, ...)
{
    char    msg[4096];
    va_list argptr;

    if (!developer.value)
        return;

    va_start(argptr, fmt);
    vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    Con_Print(msg);
}

/*
================
Con_Clear
================
*/
void Con_Clear(void)
{
    con_lines.clear();
    con_partial.clear();
}

/*
================
Con_GetLines
================
*/
const std::vector<std::string>& Con_GetLines(void)
{
    return con_lines;
}
