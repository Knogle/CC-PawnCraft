/* Standalone checks against the actual private CC-native implementations.
 * Build with stdlib.c + upstream amx.c; see PAWN-STDLIB.md for sanitizer flags.
 */
#define main ccpawn_runner_entry_for_test
#include "../src/native/runner.c"
#undef main
#include <assert.h>

static cell ptr(const void *p) { return (cell)(uintptr_t)p; }
static AMX_NATIVE find_native(const char *name) {
    for (const AMX_NATIVE_INFO *entry = natives; entry->name; entry++)
        if (strcmp(name, entry->name) == 0) return entry->func;
    abort();
}

int main(void) {
    AMX_HEADER header = {0};
    unsigned char *memory = calloc(1, 1024);
    assert(memory);
    AMX vm = {0};
    vm.base = (unsigned char *)&header;
    vm.data = memory;
    vm.stp = 1024;
    cell *parameters = (cell *)(memory + 128);
    cell *last = (cell *)(memory + 1020);
    for (const AMX_NATIVE_INFO *entry = natives; entry->name; entry++) {
        *last = sizeof(cell);
        assert(entry->func(&vm, last) == -1 && vm.error == AMX_ERR_PARAMS);
        parameters[0] = INT32_MAX;
        assert(entry->func(&vm, parameters) == -1);
        parameters[0] = -4;
        assert(entry->func(&vm, parameters) == -1);
        parameters[0] = 3;
        assert(entry->func(&vm, parameters) == -1);
    }
    parameters[0] = 0;
    assert(find_native("cc_call")(&vm, parameters) == -1);
    assert(find_native("cc_peripheral_call")(&vm, parameters) == -1);
    assert(find_native("cc_result_string")(&vm, parameters) == -1);
    assert(find_native("cc_result_count")(&vm, parameters) == 0);

    assert(ccpawn_write_string(&vm, ptr(memory), 8, 0, "term") == 4);
    assert(ccpawn_write_string(&vm, ptr(memory + 32), 16, 0, "setCursorPos") == 12);
    assert(ccpawn_write_string(&vm, ptr(memory + 96), 4, 0, "i") == 1);
    parameters[0] = 4 * sizeof(cell);
    parameters[1] = ptr(memory);
    parameters[2] = ptr(memory + 32);
    parameters[3] = ptr(memory + 96);
    parameters[4] = ptr(memory + 1024);
    assert(find_native("cc_call")(&vm, parameters) == -1);
    assert(strstr(last_error, "invalid scalar argument"));
    assert(find_native("cc_peripheral_call")(&vm, parameters) == -1);
    assert(strstr(last_error, "invalid peripheral argument"));
    assert(sequence == 0); /* No partial protocol line was ever emitted. */
    clear_results();
    free(memory);
    return 0;
}
