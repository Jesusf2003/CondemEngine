// common.cpp -- utilidades generales compartidas por todo el motor

#include "common/common.h"

#include <cstring>
#include <fstream>
#include <sstream>

#ifdef _WIN32
    #define strcasecmp _stricmp
#else
    #include <strings.h>
#endif

int     com_argc;
char**  com_argv;

char    com_token[1024];

/*
================
COM_InitArgv
================
*/
void COM_InitArgv(int argc, char** argv)
{
    com_argc = argc;
    com_argv = argv;
}

/*
================
COM_CheckParm

Devuelve la posición (1 a argc-1) del parámetro en la línea de comandos,
o 0 si no está presente
================
*/
int COM_CheckParm(const char* parm)
{
    for (int i = 1; i < com_argc; i++)
    {
        if (!com_argv[i])
            continue;
        if (!strcasecmp(parm, com_argv[i]))
            return i;
    }
    return 0;
}

/*
==============
COM_Parse

Lee el siguiente token de data y lo deja en com_token
==============
*/
const char* COM_Parse(const char* data)
{
    int c;
    int len = 0;

    com_token[0] = 0;

    if (!data)
        return nullptr;

// saltar espacios en blanco
skipwhite:
    while ((c = (unsigned char)*data) <= ' ')
    {
        if (c == 0)
            return nullptr;     // fin del texto
        data++;
    }

// saltar comentarios //
    if (c == '/' && data[1] == '/')
    {
        while (*data && *data != '\n')
            data++;
        goto skipwhite;
    }

// cadenas entre comillas
    if (c == '\"')
    {
        data++;
        while (true)
        {
            c = (unsigned char)*data;
            if (c)
                data++;
            if (c == '\"' || !c)
            {
                com_token[len] = 0;
                return data;
            }
            if (len < (int)sizeof(com_token) - 1)
                com_token[len++] = (char)c;
        }
    }

// caracteres especiales que forman un token por sí solos
    if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':')
    {
        com_token[len++] = (char)c;
        com_token[len] = 0;
        return data + 1;
    }

// palabra normal
    do
    {
        if (len < (int)sizeof(com_token) - 1)
            com_token[len++] = (char)c;
        data++;
        c = (unsigned char)*data;
        if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':')
            break;
    } while (c > 32);

    com_token[len] = 0;
    return data;
}

/*
================
COM_LoadTextFile
================
*/
bool COM_LoadTextFile(const char* path, std::string& out)
{
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f)
        return false;

    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}
