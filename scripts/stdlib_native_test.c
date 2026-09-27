/* Standalone native-boundary test. Deliberately forged parameter vectors cannot
 * be constructed with an ordinary well-formed PAWN call. Run with sanitizers:
 * cc -std=gnu11 -fsanitize=address,undefined -fsanitize-undefined-trap-on-error \
 *   -DPAWN_CELL_SIZE=32 -DHAVE_STDINT_H -DHAVE_INTTYPES_H \
 *   -Ithird_party/pawn/amx -Ithird_party/pawn/linux \
 *   scripts/stdlib_native_test.c src/native/stdlib.c -lm -o /tmp/stdlib-native-test
 */
#include "../src/native/stdlib.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const AMX_NATIVE_INFO *registered;
static unsigned writes;

int AMXAPI amx_Register(AMX *amx, const AMX_NATIVE_INFO *natives, int count) {
    (void)amx; (void)count; registered = natives; return AMX_ERR_NONE;
}
int AMXAPI amx_RaiseError(AMX *amx, int error) {
    amx->error = error; return error;
}
int ccpawn_terminal_write(const char *text) { (void)text; writes++; return 0; }

static cell address(const void *value) { return (cell)(uintptr_t)value; }
static AMX_NATIVE lookup(const char *name) {
    for (const AMX_NATIVE_INFO *entry = registered; entry->name; entry++)
        if (strcmp(name, entry->name) == 0) return entry->func;
    abort();
}

int main(void) {
    AMX_HEADER header = {0};
    unsigned char *memory = calloc(1, 1024);
    assert(memory != NULL);
    AMX vm = {0};
    vm.base = (unsigned char *)&header;
    vm.data = memory;
    vm.stp = 1024;
    assert(ccpawn_stdlib_register(&vm) == AMX_ERR_NONE);

    cell *arguments = (cell *)(memory + 128);
    cell *last = (cell *)(memory + 1020);
    *last = 'A';
    assert(ccpawn_read_string(&vm, address(last)) == NULL);
    *last = (cell)0x41414141u;
    assert(ccpawn_read_string(&vm, address(last)) == NULL);
    assert(ccpawn_read_string(&vm, address(memory + 1)) == NULL);
    assert(!ccpawn_valid_cells(&vm, address(last), 2));
    assert(!ccpawn_valid_cells(&vm, address(memory), SIZE_MAX));
    assert(ccpawn_write_string(&vm, address(last), 2, 1, "abc") == -1);
    assert(ccpawn_write_string(&vm, address(last), 1, 1, "abc") == 3);
    char *value = ccpawn_read_string(&vm, address(last));
    assert(value && strcmp(value, "abc") == 0);
    free(value);

    /* Even a fully allocated last cell cannot advertise arguments past stp. */
    *last = sizeof(cell);
    assert(lookup("strlen")(&vm, last) == -1 && vm.error == AMX_ERR_PARAMS);
    arguments[0] = INT32_MAX;
    assert(lookup("printf")(&vm, arguments) == -1 && vm.error == AMX_ERR_PARAMS);
    arguments[0] = -4;
    assert(lookup("printf")(&vm, arguments) == -1);
    arguments[0] = 3;
    assert(lookup("printf")(&vm, arguments) == -1);
    arguments[0] = 129 * sizeof(cell);
    assert(lookup("printf")(&vm, arguments) == -1);

    /* The format is valid; the reference passed as its value is not. */
    ((cell *)memory)[0] = '%';
    ((cell *)memory)[1] = 'd';
    ((cell *)memory)[2] = 0;
    arguments[0] = 2 * sizeof(cell);
    arguments[1] = address(memory);
    arguments[2] = address(memory + 1024);
    assert(lookup("printf")(&vm, arguments) == -1);
    assert(writes == 0);
    free(memory);
    return 0;
}
