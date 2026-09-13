#pragma once

#include <cstdarg>

enum
{
    CONDEM_LOG_GENERAL = 0x10,
    CONDEM_LOG_FS,
    CONDEM_LOG_SCRIPT,
    CONDEM_LOG_RENDER,
};

// -- timing
double Sys_FloatTime();
void Sys_Sleep(unsigned int ms);

// -- loggin
[[noreturn]] void Sys_Error(const char* fmt, ...);

void Sys_Warn(const char* fmt, ...);
void Sys_Printf(const char* fmt, ...);
void Sys_DebugLog(const char* file, const char* fmt, ...);

[[noreturn]] void Sys_Quit();

bool Sys_Mkdir(const char* path);