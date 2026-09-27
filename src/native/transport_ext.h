/* Included after the runner transport helpers; not a standalone source file. */
int ccpawn_terminal_write(const char *text) {
    char *encoded = hex_encode(text);
    if (!encoded) {
        snprintf(last_error, sizeof last_error, "invalid/oversized terminal text");
        return -1;
    }
    clear_results();
    unsigned long current = ++sequence;
    printf("CALL\t%lu\t5f5f6363\t7772697465\t1\tS\t%s\n", current, encoded);
    free(encoded);
    fflush(stdout);
    return parse_reply(current) ? 0 : -1;
}

static cell AMX_NATIVE_CALL native_peripheral_call(AMX *amx, const cell *params) {
    if (params[0] < 3 * (cell)sizeof(cell)) return -1;
    char *format = pawn_string(amx, params[3]);
    if (!format) return -1;
    size_t count = strlen(format);
    if (count > 14 || count > (size_t)(params[0] / (cell)sizeof(cell) - 3)) {
        free(format);
        snprintf(last_error, sizeof last_error, "peripheral call supports at most 14 arguments");
        return -1;
    }
    char combined[17] = "ss";
    memcpy(combined + 2, format, count + 1);
    cell arguments[16] = { params[1], params[2] };
    for (size_t index = 0; index < count; index++) {
        cell value = params[4 + index];
        if (format[index] == 'i' || format[index] == 'b' || format[index] == 'f') {
            if (!ccpawn_valid_cells(amx, value, 1)) {
                free(format);
                snprintf(last_error, sizeof last_error, "invalid peripheral argument address");
                return -1;
            }
            value = *amx_Address(amx, value);
        }
        arguments[index + 2] = value;
    }
    free(format);
    return emit_call(amx, "peripheral", "call", combined, arguments, 0);
}
