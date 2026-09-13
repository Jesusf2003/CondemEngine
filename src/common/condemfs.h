#pragma once

#include <cstddef>

#include "condemdef.h"

typedef struct searchpath_s
{
    char filename[MAX_OSPATH];
    struct searchpath_s *next;
} searchpath_t;

extern char fs_basedir[MAX_OSPATH];
extern char fs_gamedir[MAX_OSPATH];
extern searchpath_t *fs_searchpaths;

void FS_Init(condemparms_t* parms);
void FS_Shutdown();

bool FS_FullPath(const char *relpath, char *out, size_t outsize);
bool FS_FileExists(const char* relpath);