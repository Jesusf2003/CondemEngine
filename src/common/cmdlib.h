#pragma once

#define MAX_NUM_ARGVS  50
#define NUM_SAFE_ARGVS 6

extern int   com_argc;
extern char *com_argv[MAX_NUM_ARGVS + NUM_SAFE_ARGVS + 1];

void InitArgv(int argc, char **argv);
void COM_AddParm(const char *parm);
int  COM_CheckParm(const char *parm);