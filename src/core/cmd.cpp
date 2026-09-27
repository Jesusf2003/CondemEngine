// cmd.cpp -- procesamiento de comandos de script
// Basado en cmd.c de Quake (docs/WinQuake/cmd.cpp).

#include "core/cmd.h"
#include "core/cvar.h"
#include "common/common.h"
#include "engine/console.h"

#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
    #define strcasecmp  _stricmp
    #define strncasecmp _strnicmp
#else
    #include <strings.h>
#endif

#define MAX_ALIAS_NAME  32
#define MAX_ARGS        80

struct cmdalias_t
{
    std::string name;
    std::string value;
};

static std::vector<cmdalias_t> cmd_alias;

static bool cmd_wait;

//=============================================================================

/*
============
Cmd_Wait_f

Retrasa la ejecución del resto del buffer de comandos hasta el siguiente
frame. Permite cosas como:
bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
============
*/
static void Cmd_Wait_f(void)
{
    cmd_wait = true;
}

/*
=============================================================================

                        BUFFER DE COMANDOS

=============================================================================
*/

#define CBUF_MAX_SIZE   (64 * 1024)

static std::string cmd_text;

/*
============
Cbuf_Init
============
*/
void Cbuf_Init(void)
{
    cmd_text.clear();
    cmd_text.reserve(8192);     // espacio para comandos y scripts
}

/*
============
Cbuf_AddText

Añade texto de comandos al final del buffer
============
*/
void Cbuf_AddText(const char* text)
{
    size_t l = strlen(text);

    if (cmd_text.size() + l >= CBUF_MAX_SIZE)
    {
        Con_Printf("Cbuf_AddText: overflow\n");
        return;
    }

    cmd_text.append(text, l);
}

/*
============
Cbuf_InsertText

Añade texto de comandos justo después del comando actual
============
*/
void Cbuf_InsertText(const char* text)
{
    size_t l = strlen(text);

    if (cmd_text.size() + l >= CBUF_MAX_SIZE)
    {
        Con_Printf("Cbuf_InsertText: overflow\n");
        return;
    }

    cmd_text.insert(0, text, l);
}

/*
============
Cbuf_Execute
============
*/
void Cbuf_Execute(void)
{
    std::string line;

    while (!cmd_text.empty())
    {
// buscar un salto de línea \n o ;
        size_t  i;
        int     quotes = 0;
        for (i = 0; i < cmd_text.size(); i++)
        {
            if (cmd_text[i] == '"')
                quotes++;
            if (!(quotes & 1) && cmd_text[i] == ';')
                break;  // no cortar dentro de una cadena entre comillas
            if (cmd_text[i] == '\n')
                break;
        }

        line.assign(cmd_text, 0, i);

// borrar la línea del buffer antes de ejecutarla, porque los comandos
// (exec, alias) pueden insertar texto al principio del buffer
        if (i == cmd_text.size())
            cmd_text.clear();
        else
            cmd_text.erase(0, i + 1);

// ejecutar la línea
        Cmd_ExecuteString(line.c_str(), src_command);

        if (cmd_wait)
        {   // dejar el resto del buffer para el siguiente frame
            cmd_wait = false;
            break;
        }
    }
}

/*
==============================================================================

                        COMANDOS DE SCRIPT

==============================================================================
*/

/*
===============
Cmd_StuffCmds_f

Añade los parámetros de la línea de comandos como comandos de script.
Los comandos empiezan con + y siguen hasta un - o el siguiente +
condem +exec autoexec.cfg -editor +developer 1
===============
*/
static void Cmd_StuffCmds_f(void)
{
    if (Cmd_Argc() != 1)
    {
        Con_Printf("stuffcmds : execute command line parameters\n");
        return;
    }

// construir la cadena combinada de parámetros
    std::string text;
    for (int i = 1; i < com_argc; i++)
    {
        if (!com_argv[i])
            continue;
        if (!text.empty())
            text += ' ';
        text += com_argv[i];
    }
    if (text.empty())
        return;

// extraer los comandos
    std::string build;
    for (size_t i = 0; i < text.size(); i++)
    {
        if (text[i] != '+')
            continue;

        i++;
        size_t j = i;
        while (j < text.size() && text[j] != '+' && text[j] != '-')
            j++;

        build.append(text, i, j - i);
        build += '\n';
        i = j - 1;
    }

    if (!build.empty())
        Cbuf_InsertText(build.c_str());
}

/*
===============
Cmd_Exec_f
===============
*/
static void Cmd_Exec_f(void)
{
    if (Cmd_Argc() != 2)
    {
        Con_Printf("exec <filename> : execute a script file\n");
        return;
    }

    std::string f;
    if (!COM_LoadTextFile(Cmd_Argv(1), f))
    {
        Con_Printf("couldn't exec %s\n", Cmd_Argv(1));
        return;
    }
    Con_Printf("execing %s\n", Cmd_Argv(1));

    // asegura que la última línea del archivo no se una al siguiente comando
    if (f.empty() || f.back() != '\n')
        f += '\n';
    Cbuf_InsertText(f.c_str());
}

/*
===============
Cmd_Echo_f

Imprime el resto de la línea en la consola
===============
*/
static void Cmd_Echo_f(void)
{
    for (int i = 1; i < Cmd_Argc(); i++)
        Con_Printf("%s ", Cmd_Argv(i));
    Con_Printf("\n");
}

/*
===============
Cmd_Alias_f

Crea un nuevo comando que ejecuta una cadena de comandos (puede tener ;)
===============
*/
static void Cmd_Alias_f(void)
{
    if (Cmd_Argc() == 1)
    {
        Con_Printf("Current alias commands:\n");
        for (const cmdalias_t& a : cmd_alias)
            Con_Printf("%s : %s", a.name.c_str(), a.value.c_str());
        return;
    }

    const char* s = Cmd_Argv(1);
    if (strlen(s) >= MAX_ALIAS_NAME)
    {
        Con_Printf("Alias name is too long\n");
        return;
    }

// copiar el resto de la línea de comandos
    std::string cmd;
    int c = Cmd_Argc();
    for (int i = 2; i < c; i++)
    {
        cmd += Cmd_Argv(i);
        if (i != c - 1)
            cmd += ' ';
    }
    cmd += '\n';

// si el alias ya existe, se reutiliza
    for (cmdalias_t& a : cmd_alias)
    {
        if (!strcmp(s, a.name.c_str()))
        {
            a.value = cmd;
            return;
        }
    }

    cmd_alias.push_back({ s, cmd });
}

/*
=============================================================================

                        EJECUCIÓN DE COMANDOS

=============================================================================
*/

struct cmd_function_t
{
    const char* name;
    xcommand_t  function;
};

static int              cmd_argc;
static std::string      cmd_argv[MAX_ARGS];
static const char*      cmd_args = "";

cmd_source_t    cmd_source;

static std::vector<cmd_function_t> cmd_functions;   // comandos disponibles

/*
============
Cmd_List_f
============
*/
static void Cmd_List_f(void)
{
    const char* partial = Cmd_Argv(1);
    size_t      len = strlen(partial);
    int         count = 0;

    for (const cmd_function_t& cmd : cmd_functions)
    {
        if (len && strncasecmp(partial, cmd.name, len))
            continue;
        Con_Printf("  %s\n", cmd.name);
        count++;
    }
    Con_Printf("%i command(s)\n", count);
}

/*
============
Cmd_Init
============
*/
void Cmd_Init(void)
{
//
// registrar nuestros comandos
//
    Cmd_AddCommand("stuffcmds", Cmd_StuffCmds_f);
    Cmd_AddCommand("exec", Cmd_Exec_f);
    Cmd_AddCommand("echo", Cmd_Echo_f);
    Cmd_AddCommand("alias", Cmd_Alias_f);
    Cmd_AddCommand("wait", Cmd_Wait_f);
    Cmd_AddCommand("cmdlist", Cmd_List_f);
}

/*
============
Cmd_Argc
============
*/
int Cmd_Argc(void)
{
    return cmd_argc;
}

/*
============
Cmd_Argv
============
*/
const char* Cmd_Argv(int arg)
{
    if ((unsigned)arg >= (unsigned)cmd_argc)
        return "";
    return cmd_argv[arg].c_str();
}

/*
============
Cmd_Args
============
*/
const char* Cmd_Args(void)
{
    return cmd_args;
}

/*
============
Cmd_TokenizeString

Separa la cadena en tokens de línea de comandos
============
*/
void Cmd_TokenizeString(const char* text)
{
    static std::string args;    // cmd_args apunta aquí

// limpiar los argumentos anteriores
    cmd_argc = 0;
    args.clear();
    cmd_args = "";

    while (true)
    {
// saltar espacios hasta un \n
        while (*text && (unsigned char)*text <= ' ' && *text != '\n')
            text++;

        if (*text == '\n')
            break;  // un salto de línea separa comandos en el buffer

        if (!*text)
            break;

        if (cmd_argc == 1)
        {
            const char* end = strchr(text, '\n');
            args.assign(text, end ? (size_t)(end - text) : strlen(text));
            cmd_args = args.c_str();
        }

        text = COM_Parse(text);
        if (!text)
            break;

        if (cmd_argc < MAX_ARGS)
        {
            cmd_argv[cmd_argc] = com_token;
            cmd_argc++;
        }
    }
}

/*
============
Cmd_AddCommand
============
*/
void Cmd_AddCommand(const char* cmd_name, xcommand_t function)
{
// fallar si el nombre ya es un cvar
    if (Cvar_FindVar(cmd_name))
    {
        Con_Printf("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
        return;
    }

// fallar si el comando ya existe
    if (Cmd_Exists(cmd_name))
    {
        Con_Printf("Cmd_AddCommand: %s already defined\n", cmd_name);
        return;
    }

    cmd_functions.push_back({ cmd_name, function });
}

/*
============
Cmd_Exists
============
*/
bool Cmd_Exists(const char* cmd_name)
{
    for (const cmd_function_t& cmd : cmd_functions)
    {
        if (!strcasecmp(cmd_name, cmd.name))
            return true;
    }
    return false;
}

/*
============
Cmd_CompleteCommand
============
*/
const char* Cmd_CompleteCommand(const char* partial)
{
    size_t len = strlen(partial);

    if (!len)
        return nullptr;

// buscar en las funciones
    for (const cmd_function_t& cmd : cmd_functions)
        if (!strncasecmp(partial, cmd.name, len))
            return cmd.name;

    return nullptr;
}

/*
============
Cmd_CompleteCommandList
============
*/
void Cmd_CompleteCommandList(const char* partial, std::vector<const char*>& matches)
{
    size_t len = strlen(partial);

    for (const cmd_function_t& cmd : cmd_functions)
        if (!strncasecmp(partial, cmd.name, len))
            matches.push_back(cmd.name);
}

/*
============
Cmd_ExecuteString

Una línea completa ha sido separada, así que se intenta ejecutar
============
*/
void Cmd_ExecuteString(const char* text, cmd_source_t src)
{
    cmd_source = src;
    Cmd_TokenizeString(text);

// ejecutar la línea
    if (!Cmd_Argc())
        return;     // sin tokens

// buscar en las funciones
    for (const cmd_function_t& cmd : cmd_functions)
    {
        if (!strcasecmp(cmd_argv[0].c_str(), cmd.name))
        {
            cmd.function();
            return;
        }
    }

// buscar en los alias
    for (const cmdalias_t& a : cmd_alias)
    {
        if (!strcasecmp(cmd_argv[0].c_str(), a.name.c_str()))
        {
            Cbuf_InsertText(a.value.c_str());
            return;
        }
    }

// buscar en los cvars
    if (!Cvar_Command())
        Con_Printf("Unknown command \"%s\"\n", Cmd_Argv(0));
}

/*
================
Cmd_CheckParm

Devuelve la posición (1 a argc-1) del parámetro en la lista de argumentos
del comando, o 0 si no está presente
================
*/
int Cmd_CheckParm(const char* parm)
{
    for (int i = 1; i < Cmd_Argc(); i++)
        if (!strcasecmp(parm, Cmd_Argv(i)))
            return i;

    return 0;
}
