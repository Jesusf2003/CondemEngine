#include <SDL3/SDL.h>

#include "condemfs.h"
#include "cmdlib.h"
#include "../client/sys.h"

#define DEFAULT_GAMEDIR "cdbase"

char fs_basedir[MAX_OSPATH];
char fs_gamedir[MAX_OSPATH];
searchpath_t *fs_searchpaths = nullptr;

static bool FS_PathExists(const char *path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info);
}

static bool FS_DirExists(const char* path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

static void FS_AddSearchPath(const char *path)
{
    searchpath_t *sp = (searchpath_t *)SDL_malloc(sizeof(searchpath_t));
    SDL_strlcpy(sp->filename, path, sizeof(sp->filename));
    sp->next = fs_searchpaths;
    fs_searchpaths = sp;

    Sys_Printf("FS: agregado directorio de busqueda '%s'", sp->filename);
}

void FS_Init(condemparms_t* parms)
{
    if (parms->basedir)
    {
        SDL_strlcpy(fs_basedir, parms->basedir, sizeof(fs_basedir));
    } else {
        SDL_strlcpy(fs_basedir, "./", sizeof(fs_basedir));
    }
    
    const char *gamedir_name = DEFAULT_GAMEDIR;
    int p = COM_CheckParm("-game");
    if (p && p + 1 < com_argc)
    {
        gamedir_name = com_argv[p + 1];
    }
    
    SDL_snprintf(fs_gamedir, sizeof(fs_gamedir), "%s%s", fs_basedir, gamedir_name);
    
    if (!FS_DirExists(fs_gamedir))
    {
        Sys_Error("FS_Init(): no se encontró el directorio base del juego '%s'", fs_gamedir);
    }
    
    FS_AddSearchPath(fs_gamedir);
    Sys_Printf("FS: basedir = '%s'", fs_basedir);
    Sys_Printf("FS: gamedir = '%s'", fs_gamedir);
}

void FS_Shutdown()
{
    searchpath_t *sp = fs_searchpaths;
    while(sp)
    {
        searchpath_t *next = sp->next;
        SDL_free(sp);
        sp = next;
    }
    fs_searchpaths = nullptr;
}

bool FS_FullPath(const char *relpath, char *out, size_t outsize)
{
    for (searchpath_t* sp = fs_searchpaths; sp; sp = sp->next)
    {
        SDL_snprintf(out, outsize, "%s/%s", sp->filename, relpath);
        if (FS_PathExists(out)) return true;
    }
    out[0] = '\0';
    return false;
}

bool FS_FileExists(const char *relpath)
{
    char full[MAX_OSPATH * 2];
    return FS_FullPath(relpath, full, sizeof(full));
}