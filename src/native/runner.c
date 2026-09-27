/*
 * PawnCraft AMX runner. The process has no Minecraft or host API access: every
 * external operation is a typed request over stdin/stdout to the Java/Lua
 * bridge. A crashed VM therefore cannot crash the Minecraft JVM.
 */
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "amx.h"
#include "stdlib.h"

#define MAX_AMX_BYTES (1024u * 1024u)
#define MAX_RESULTS 16
#define MAX_LINE (1024u * 1024u)
#define MAX_STRING (256u * 1024u)

typedef struct {
    char type;
    cell integer;
    float real;
    char *string;
} result_value;

static result_value results[MAX_RESULTS];
static int result_count;
static char last_error[512];
static unsigned long sequence;

static void clear_results(void) {
    for (int i = 0; i < result_count; i++) {
        free(results[i].string);
        results[i].string = NULL;
    }
    result_count = 0;
    last_error[0] = '\0';
}

static char *pawn_string(AMX *amx, cell parameter) {
    return ccpawn_read_string(amx, parameter);
}

static char hex_digit(unsigned value) {
    return (char)(value < 10 ? '0' + value : 'a' + value - 10);
}

static char *hex_encode(const char *input) {
    size_t length = strlen(input);
    if (length > MAX_STRING) return NULL;
    char *output = malloc(length * 2 + 2);
    if (output == NULL) return NULL;
    if (length == 0) {
        output[0] = '-';
        output[1] = '\0';
        return output;
    }
    for (size_t i = 0; i < length; i++) {
        unsigned value = (unsigned char)input[i];
        output[i * 2] = hex_digit(value >> 4);
        output[i * 2 + 1] = hex_digit(value & 15);
    }
    output[length * 2] = '\0';
    return output;
}

static int hex_value(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

static char *hex_decode(const char *input) {
    if (strcmp(input, "-") == 0) return strdup("");
    size_t length = strlen(input);
    if ((length & 1u) != 0 || length / 2 > MAX_STRING) return NULL;
    char *output = malloc(length / 2 + 1);
    if (output == NULL) return NULL;
    for (size_t i = 0; i < length; i += 2) {
        int high = hex_value(input[i]);
        int low = hex_value(input[i + 1]);
        if (high < 0 || low < 0) {
            free(output);
            return NULL;
        }
        output[i / 2] = (char)((high << 4) | low);
    }
    output[length / 2] = '\0';
    return output;
}

static int parse_reply(unsigned long expected_sequence) {
    char *line = malloc(MAX_LINE);
    if (line == NULL) return 0;
    if (fgets(line, MAX_LINE, stdin) == NULL) {
        snprintf(last_error, sizeof last_error, "bridge closed its input");
        free(line);
        return 0;
    }
    size_t length = strlen(line);
    if (length == 0 || line[length - 1] != '\n') {
        snprintf(last_error, sizeof last_error, "oversized bridge response");
        free(line);
        return 0;
    }
    line[length - 1] = '\0';

    char *state = NULL;
    char *kind = strtok_r(line, "\t", &state);
    char *sequence_text = strtok_r(NULL, "\t", &state);
    if (kind == NULL || sequence_text == NULL || strtoul(sequence_text, NULL, 10) != expected_sequence) {
        snprintf(last_error, sizeof last_error, "invalid bridge response");
        free(line);
        return 0;
    }
    if (strcmp(kind, "ERROR") == 0) {
        char *encoded = strtok_r(NULL, "\t", &state);
        char *message = encoded == NULL ? NULL : hex_decode(encoded);
        snprintf(last_error, sizeof last_error, "%s", message == NULL ? "CC API call failed" : message);
        free(message);
        free(line);
        return 0;
    }
    if (strcmp(kind, "RETURN") != 0) {
        snprintf(last_error, sizeof last_error, "unknown bridge response");
        free(line);
        return 0;
    }

    char *count_text = strtok_r(NULL, "\t", &state);
    long count = count_text == NULL ? -1 : strtol(count_text, NULL, 10);
    if (count < 0 || count > MAX_RESULTS) {
        snprintf(last_error, sizeof last_error, "invalid result count");
        free(line);
        return 0;
    }
    for (int i = 0; i < count; i++) {
        char *type = strtok_r(NULL, "\t", &state);
        char *value = strtok_r(NULL, "\t", &state);
        if (type == NULL || value == NULL || strlen(type) != 1) {
            snprintf(last_error, sizeof last_error, "truncated bridge response");
            clear_results();
            free(line);
            return 0;
        }
        result_value *result = &results[result_count++];
        result->type = type[0];
        switch (result->type) {
            case 'I': case 'B': result->integer = (cell)strtol(value, NULL, 10); break;
            case 'F': {
                char *end = NULL;
                errno = 0;
                result->real = strtof(value, &end);
                /* strtof may set ERANGE for representable subnormals too.
                 * Keep finite nonzero values; reject genuine underflow to 0. */
                if (end == value || *end != '\0' || !isfinite(result->real) ||
                    (errno == ERANGE && result->real == 0.0f)) {
                    clear_results();
                    snprintf(last_error, sizeof last_error, "invalid/nonfinite Float result");
                    free(line);
                    return 0;
                }
                break;
            }
            case 'S': case 'J':
                result->string = hex_decode(value);
                if (result->string == NULL) {
                    snprintf(last_error, sizeof last_error, "invalid string result");
                    clear_results();
                    free(line);
                    return 0;
                }
                break;
            case 'N': break;
            default:
                snprintf(last_error, sizeof last_error, "unknown result type");
                clear_results();
                free(line);
                return 0;
        }
    }
    free(line);
    return 1;
}

static int emit_call(AMX *amx, const char *api, const char *method,
                     const char *format, const cell *params, int first_argument) {
    clear_results();
    size_t count = strlen(format);
    if (count > 16) {
        snprintf(last_error, sizeof last_error, "at most 16 arguments are supported");
        return -1;
    }
    char *api_hex = hex_encode(api);
    char *method_hex = hex_encode(method);
    if (api_hex == NULL || method_hex == NULL) {
        free(api_hex); free(method_hex);
        snprintf(last_error, sizeof last_error, "out of memory");
        return -1;
    }
    /* Validate and encode the whole request before exposing anything on stdout.
     * A rejected format must not leave a partial protocol line behind. */
    char *values[16] = { 0 };
    char types[16];
    size_t total = strlen(api_hex) + strlen(method_hex) + 80;
    int valid = 1;
    for (size_t i = 0; i < count; i++) {
        cell value = params[first_argument + (int)i];
        char scalar[64];
        switch (format[i]) {
            case 'i': types[i] = 'I'; snprintf(scalar, sizeof scalar, "%d", (int)value); values[i] = strdup(scalar); break;
            case 'b': types[i] = 'B'; values[i] = strdup(value ? "1" : "0"); break;
            case 'f': {
                float real; memcpy(&real, &value, sizeof real);
                if (!isfinite(real)) { valid = 0; break; }
                types[i] = 'F'; snprintf(scalar, sizeof scalar, "%.9g", (double)real);
                values[i] = strdup(scalar);
                break;
            }
            case 's': case 'j': {
                char *text = pawn_string(amx, value);
                types[i] = format[i] == 'j' ? 'J' : 'S';
                values[i] = text ? hex_encode(text) : NULL;
                free(text);
                break;
            }
            default: valid = 0; break;
        }
        if (!valid || !values[i]) { valid = 0; break; }
        total += strlen(values[i]) + 3;
        if (total >= MAX_LINE) { valid = 0; break; }
    }
    if (!valid) {
        for (size_t i = 0; i < count; i++) free(values[i]);
        free(api_hex); free(method_hex);
        snprintf(last_error, sizeof last_error, "invalid/oversized CC argument or format");
        return -1;
    }
    unsigned long current = ++sequence;
    printf("CALL\t%lu\t%s\t%s\t%zu", current, api_hex, method_hex, count);
    free(api_hex); free(method_hex);
    for (size_t i = 0; i < count; i++) {
        printf("\t%c\t%s", types[i], values[i]);
        free(values[i]);
    }
    printf("\n");
    fflush(stdout);
    return parse_reply(current) ? result_count : -1;
}

static cell AMX_NATIVE_CALL native_cc_call(AMX *amx, const cell *params) {
    char *api = pawn_string(amx, params[1]);
    char *method = pawn_string(amx, params[2]);
    char *format = pawn_string(amx, params[3]);
    if (api == NULL || method == NULL || format == NULL) {
        free(api); free(method); free(format);
        snprintf(last_error, sizeof last_error, "invalid cc_call string");
        return -1;
    }
    int declared = params[0] / (cell)sizeof(cell);
    if ((int)strlen(format) > declared - 3) {
        snprintf(last_error, sizeof last_error, "format requires more arguments than supplied");
        free(api); free(method); free(format);
        return -1;
    }
    /* PAWN passes variadic scalar arguments by reference, unlike the fixed
     * arguments of cc_sleep. Strings already are addresses; keep them intact. */
    cell arguments[16];
    size_t count = strlen(format);
    int result = -1;
    if (count > 16) {
        snprintf(last_error, sizeof last_error, "at most 16 arguments are supported");
    } else {
        size_t index;
        for (index = 0; index < count; index++) {
            cell value = params[4 + index];
            if (format[index] == 'i' || format[index] == 'b' || format[index] == 'f') {
                cell *address = amx_Address(amx, value);
                if (!ccpawn_valid_cells(amx, value, 1)) {
                    snprintf(last_error, sizeof last_error, "invalid scalar argument");
                    break;
                }
                value = *address;
            }
            arguments[index] = value;
        }
        if (index == count) result = emit_call(amx, api, method, format, arguments, 0);
    }
    free(api); free(method); free(format);
    return (cell)result;
}

static cell AMX_NATIVE_CALL native_result_count(AMX *amx, const cell *params) {
    (void)amx; (void)params;
    return result_count;
}

static result_value *get_result(cell index) {
    return index >= 0 && index < result_count ? &results[index] : NULL;
}

static cell AMX_NATIVE_CALL native_result_type(AMX *amx, const cell *params) {
    (void)amx;
    result_value *value = get_result(params[1]);
    if (value == NULL || value->type == 'N') return 0;
    if (value->type == 'I') return 1;
    if (value->type == 'B') return 2;
    if (value->type == 'F') return 3;
    if (value->type == 'S') return 4;
    if (value->type == 'J') return 5;
    return 0;
}

static cell AMX_NATIVE_CALL native_result_int(AMX *amx, const cell *params) {
    (void)amx;
    result_value *value = get_result(params[1]);
    return value == NULL ? 0 : value->integer;
}

static cell AMX_NATIVE_CALL native_result_bool(AMX *amx, const cell *params) {
    return native_result_int(amx, params) != 0;
}

static cell AMX_NATIVE_CALL native_result_float(AMX *amx, const cell *params) {
    (void)amx;
    result_value *value = get_result(params[1]);
    float result = value == NULL ? 0.0f :
        (value->type == 'I' || value->type == 'B') ? (float)value->integer : value->real;
    cell encoded;
    memcpy(&encoded, &result, sizeof encoded);
    return encoded;
}

static cell copy_string_result(AMX *amx, const cell *params, const char *source) {
    if (ccpawn_write_string(amx, params[1], params[2], 0, source) < 0) return -1;
    /* CC getters historically return the full source length so callers can
     * detect truncation. Stdlib copying functions instead return bytes copied. */
    return (cell)strlen(source);
}

static cell AMX_NATIVE_CALL native_result_string(AMX *amx, const cell *params) {
    result_value *value = get_result(params[1]);
    const char *text = value != NULL && value->string != NULL ? value->string : "";
    cell shifted[3] = { params[0], params[2], params[3] };
    return copy_string_result(amx, shifted, text);
}

static cell AMX_NATIVE_CALL native_last_error(AMX *amx, const cell *params) {
    return copy_string_result(amx, params, last_error);
}

static cell AMX_NATIVE_CALL native_print(AMX *amx, const cell *params) {
    char *text = pawn_string(amx, params[1]);
    if (text == NULL) return -1;
    int result = ccpawn_terminal_write(text);
    free(text);
    return result < 0 ? -1 : 0;
}

static cell AMX_NATIVE_CALL native_sleep(AMX *amx, const cell *params) {
    cell fake[2] = { 0, params[1] };
    return emit_call(amx, "__cc", "sleep", "i", fake, 1) < 0 ? -1 : 0;
}

static cell AMX_NATIVE_CALL native_yield(AMX *amx, const cell *params) {
    (void)params;
    cell fake[1] = { 0 };
    return emit_call(amx, "__cc", "yield", "", fake, 1) < 0 ? -1 : 0;
}

#include "transport_ext.h"

#define CC_GUARDED(name, minimum) \
    static cell AMX_NATIVE_CALL safe_##name(AMX *amx, const cell *parameters) { \
        if (!ccpawn_valid_parameters(amx, parameters, minimum)) { \
            clear_results(); \
            snprintf(last_error, sizeof last_error, "invalid CC native parameter vector"); \
            amx_RaiseError(amx, AMX_ERR_PARAMS); \
            return -1; \
        } \
        return name(amx, parameters); \
    }
CC_GUARDED(native_cc_call, 3)
CC_GUARDED(native_peripheral_call, 3)
CC_GUARDED(native_result_count, 0)
CC_GUARDED(native_result_type, 1)
CC_GUARDED(native_result_int, 1)
CC_GUARDED(native_result_bool, 1)
CC_GUARDED(native_result_float, 1)
CC_GUARDED(native_result_string, 3)
CC_GUARDED(native_last_error, 2)
CC_GUARDED(native_sleep, 1)
CC_GUARDED(native_yield, 0)
CC_GUARDED(native_print, 1)

static const AMX_NATIVE_INFO natives[] = {
    { "cc_call", safe_native_cc_call },
    { "cc_peripheral_call", safe_native_peripheral_call },
    { "cc_result_count", safe_native_result_count },
    { "cc_result_type", safe_native_result_type },
    { "cc_result_int", safe_native_result_int },
    { "cc_result_bool", safe_native_result_bool },
    { "cc_result_float", safe_native_result_float },
    { "cc_result_string", safe_native_result_string },
    { "cc_last_error", safe_native_last_error },
    { "cc_sleep", safe_native_sleep },
    { "cc_yield", safe_native_yield },
    { "print", safe_native_print },
    { NULL, NULL }
};

static const char *error_name(int error) {
    static const char *names[] = {
        "none", "forced exit", "assertion failed", "stack/heap collision",
        "array bounds", "invalid memory access", "invalid instruction",
        "stack underflow", "heap underflow", "invalid native callback",
        "native failed", "divide by zero", "sleep", "invalid state"
    };
    if (error >= 0 && (size_t)error < sizeof names / sizeof names[0]) return names[error];
    return "AMX error";
}

static int load_program(const char *path, AMX *amx, unsigned char **allocation) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) return AMX_ERR_NOTFOUND;
    AMX_HEADER header;
    if (fread(&header, sizeof header, 1, file) != 1) {
        fclose(file);
        return AMX_ERR_FORMAT;
    }
    amx_Align16(&header.magic);
    amx_Align16((uint16_t *)&header.flags);
    amx_Align32((uint32_t *)&header.size);
    amx_Align32((uint32_t *)&header.stp);
    if (header.magic != AMX_MAGIC || header.size <= 0 || header.stp < header.size ||
        (unsigned)header.size > MAX_AMX_BYTES || (unsigned)header.stp > MAX_AMX_BYTES) {
        fclose(file);
        return AMX_ERR_FORMAT;
    }
    unsigned char *memory = calloc(1, (size_t)header.stp);
    if (memory == NULL) {
        fclose(file);
        return AMX_ERR_MEMORY;
    }
    rewind(file);
    if (fread(memory, 1, (size_t)header.size, file) != (size_t)header.size) {
        free(memory);
        fclose(file);
        return AMX_ERR_FORMAT;
    }
    fclose(file);
    memset(amx, 0, sizeof *amx);
    int error = amx_Init(amx, memory);
    if (error != AMX_ERR_NONE) {
        free(memory);
        return error;
    }
    *allocation = memory;
    return AMX_ERR_NONE;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc != 2) {
        fprintf(stderr, "usage: ccpawn-runner PROGRAM.amx\n");
        return 64;
    }
    AMX amx;
    unsigned char *program = NULL;
    int error = load_program(argv[1], &amx, &program);
    if (error != AMX_ERR_NONE) {
        printf("ERROR\t%d\tload failed\n", error);
        return 65;
    }
    error = ccpawn_stdlib_register(&amx);
    if (error == AMX_ERR_NONE || error == AMX_ERR_NOTFOUND)
        error = amx_Register(&amx, natives, -1);
    if (error != AMX_ERR_NONE) {
        printf("ERROR\t%d\tunresolved native\n", error);
        amx_Cleanup(&amx); free(program);
        return 66;
    }
    printf("READY\n");
    cell return_value = 0;
    error = amx_Exec(&amx, &return_value, AMX_EXEC_MAIN);
    if (error == AMX_ERR_NONE) printf("DONE\t%d\n", (int)return_value);
    else printf("ERROR\t%d\t%s\n", error, error_name(error));
    clear_results();
    amx_Cleanup(&amx);
    free(program);
    return error == AMX_ERR_NONE ? 0 : 67;
}
