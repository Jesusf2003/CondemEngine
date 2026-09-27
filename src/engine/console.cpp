// console.cpp -- salida de texto del motor
// Basado en console.c de Quake (docs/WinQuake/console.cpp).

#include "engine/console.h"
#include "common/common.h"
#include "core/cmd.h"
#include "core/cvar.h"
#include "sys/sys.h"

#include <cstdarg>
#include <cstdio>

#define CON_MAX_LINES   1024
#define CON_LOGFILE     "qconsole.log"

cvar_t  developer = {"developer", "0"};

static std::vector<std::string> con_lines;
static std::string              con_partial;    // línea en curso, aún sin '\n'
static bool                     con_cr;         // '\r': la próxima línea sobrescribe la actual

static FILE*                    con_debuglog;   // -condebug

/*
================
Con_StripText

Texto plano para archivos: sin la marca de resaltado ni códigos ANSI, las
barras \35\36\37 como '=' y '\r' como salto de línea
================
*/
static std::string Con_StripText(const std::string& text)
{
    std::string out;

    for (size_t i = 0; i < text.size(); i++)
    {
        const char c = text[i];
        if (c == 1 || c == CON_HIGHLIGHT)
            continue;
        if (c == '\35' || c == '\36' || c == '\37')
        {
            out += '=';
            continue;
        }
        if (c == '\r')
        {
            out += '\n';
            continue;
        }
        if (c == '\033' && i + 1 < text.size() && text[i + 1] == '[')
        {
            // ESC [ ... m
            size_t end = text.find('m', i + 2);
            if (end != std::string::npos)
            {
                i = end;
                continue;
            }
        }
        out += c;
    }
    return out;
}

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
Con_Dump_f

condump [archivo]: guarda el contenido de la consola en un archivo de texto
================
*/
static void Con_Dump_f(void)
{
    std::string name = Cmd_Argc() > 1 ? Cmd_Argv(1) : "condump";
    if (name.find('.') == std::string::npos)
        name += ".txt";

    FILE* f = fopen(name.c_str(), "w");
    if (!f)
    {
        Con_Printf("ERROR: no se pudo crear %s\n", name.c_str());
        return;
    }

    for (const std::string& line : con_lines)
        fprintf(f, "%s\n", Con_StripText(line).c_str());
    if (!con_partial.empty())
        fprintf(f, "%s\n", Con_StripText(con_partial).c_str());
    fclose(f);

    Con_Printf("Consola guardada en %s\n", name.c_str());
}

/*
================
Con_Init
================
*/
void Con_Init(void)
{
    // -condebug: todo lo que se imprime se guarda también en qconsole.log
    if (COM_CheckParm("-condebug"))
    {
        con_debuglog = fopen(CON_LOGFILE, "w");     // como Quake, empieza vacío
        if (!con_debuglog)
            Sys_Printf("Con_Init: no se pudo crear " CON_LOGFILE "\n");
    }

    Cvar_RegisterVariable(&developer);
    Cmd_AddCommand("clear", Con_Clear_f);
    Cmd_AddCommand("condump", Con_Dump_f);
}

/*
================
Con_Print

Separa el texto en líneas y las añade al historial. Todo lo que se imprime
pasa por aquí para que quede en el log.

Como en Quake, si el texto empieza por '\1' o '\2' sus líneas se muestran
resaltadas (en Quake, con la otra mitad de la fuente). '\r' vuelve al
principio de la línea: la siguiente escritura la sobrescribe.
================
*/
static void Con_Print(const char* text)
{
    bool highlight = false;

    if (text[0] == 1 || text[0] == 2)
    {
        highlight = true;
        text++;
    }

    Sys_Printf("%s", text);

    if (con_debuglog)
    {
        fputs(Con_StripText(text).c_str(), con_debuglog);
        fflush(con_debuglog);
    }

    for (const char* p = text; *p; p++)
    {
        if (con_cr)
        {
            con_partial.clear();
            con_cr = false;
        }

        if (*p == '\r')
        {
            con_cr = true;
            continue;
        }

        if (con_partial.empty() && highlight)
            con_partial += CON_HIGHLIGHT;

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
    con_cr = false;
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
