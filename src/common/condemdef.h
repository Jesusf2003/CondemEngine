#pragma once

#define CONDEM_ENGINE_VERSION_MAJOR 0
#define CONDEM_ENGINE_VERSION_MINOR 1
#define CONDEM_ENGINE_VERSION_STRING "0.1.0"
#define CONDEM_ENGINE_NAME "Condem Engine"

#define MAX_OSPATH 256

typedef struct
{
    const char *basedir;
    const char *binarydir;
    int argc;
    char **argv;
} condemparms_t;
