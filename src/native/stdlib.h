#ifndef CCPAWN_STDLIB_H
#define CCPAWN_STDLIB_H
#include "amx.h"
#include <stddef.h>

/* Register alongside the CC natives. AMX_ERR_NOTFOUND is allowed until all
 * tables have been registered. No host console/file/process APIs are exposed. */
int ccpawn_stdlib_register(AMX *amx);
int ccpawn_terminal_write(const char *text);
char *ccpawn_read_string(AMX *amx, cell address);
int ccpawn_valid_cells(AMX *amx, cell address, size_t count);
int ccpawn_valid_parameters(AMX *amx, const cell *parameters, size_t minimum);
cell ccpawn_write_string(AMX *amx, cell address, cell capacity, int packed, const char *text);
#endif
