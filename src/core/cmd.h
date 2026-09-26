// cmd.h -- buffer de comandos y ejecución de comandos
// Basado en cmd.h de Quake (docs/WinQuake/cmd.h).

#pragma once

//===========================================================================

/*

Se pueden añadir cualquier cantidad de comandos en un frame, desde distintas
fuentes. La mayoría vienen de la consola o de los bindings de teclas, pero
también se pueden ejecutar archivos de texto completos con "exec".

Las opciones de la línea de comandos que empiezan con + también se añaden
al buffer de comandos (ver "stuffcmds").

*/

void Cbuf_Init(void);
// Inicializa el buffer de texto de comandos.

void Cbuf_AddText(const char* text);
// A medida que se generan comandos desde la consola o los bindings,
// el texto se añade al final del buffer.

void Cbuf_InsertText(const char* text);
// Cuando un comando quiere ejecutar otros comandos inmediatamente, el texto
// se inserta al principio del buffer, antes del resto de comandos pendientes.

void Cbuf_Execute(void);
// Extrae líneas terminadas en \n o ; del buffer y las ejecuta con
// Cmd_ExecuteString. Se detiene cuando el buffer está vacío o con "wait".
// Normalmente se llama una vez por frame.
// ¡No llamar dentro de una función de comando!

//===========================================================================

/*

La ejecución de comandos toma una cadena terminada en null, la separa en
tokens y busca un comando, alias o variable que coincida con el primer token.

*/

typedef void (*xcommand_t)(void);

typedef enum
{
    src_client,     // llegó por red (reservado para cuando exista el servidor)
    src_command     // desde el buffer de comandos
} cmd_source_t;

extern cmd_source_t cmd_source;

void Cmd_Init(void);

void Cmd_AddCommand(const char* cmd_name, xcommand_t function);
// La llaman las funciones de inicialización de otros sistemas para registrar
// comandos. cmd_name se guarda por referencia, no debe ser memoria temporal.

bool Cmd_Exists(const char* cmd_name);
// La usa cvar para evitar que un cvar y un comando tengan el mismo nombre.

const char* Cmd_CompleteCommand(const char* partial);
// Intenta completar un comando parcial (autocompletado de la consola).
// Devuelve nullptr si no hay coincidencias.

int         Cmd_Argc(void);
const char* Cmd_Argv(int arg);
const char* Cmd_Args(void);
// Las funciones de comando obtienen sus parámetros con estas funciones.
// Cmd_Argv() devuelve una cadena vacía, nunca nullptr, si arg >= argc.

int Cmd_CheckParm(const char* parm);
// Devuelve la posición (1 a argc-1) del parámetro en la lista de argumentos
// del comando actual, o 0 si no está presente.

void Cmd_TokenizeString(const char* text);
// Toma una cadena terminada en null (no necesita \n) y la separa en tokens.

void Cmd_ExecuteString(const char* text, cmd_source_t src);
// Separa una sola línea en argumentos e intenta ejecutarla.
