/* Bounded PAWN standard-library subset for ComputerCraft.
 * API conventions follow the bundled CompuPhase PAWN 4.1 includes. Native
 * strings are byte strings, packed or unpacked, as used by the CC bridge.
 * All formatting occurs off-protocol; only a completely valid result is sent.
 */
#include "stdlib.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TEXT_MAX 16384u
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

typedef struct { char text[TEXT_MAX + 1]; size_t length; int packed; } text_value;

static int arity(const cell *p, size_t n) {
    return p[0] >= 0 && (size_t)p[0] / sizeof(cell) >= n && p[0] % sizeof(cell) == 0;
}

/* Do not use amx_StrLen before checking the whole scanned area. Its upstream
 * implementation assumes trusted, terminated input. stp is relative to data. */
static cell *cells(AMX *amx, cell address, size_t count) {
    AMX_HEADER *header = (AMX_HEADER *)amx->base;
    uintptr_t base = (uintptr_t)(amx->data ? amx->data : amx->base + header->dat);
    uintptr_t end = base + (size_t)amx->stp;
    uintptr_t ptr = (uintptr_t)amx_Address(amx, address);
    if (count > SIZE_MAX / sizeof(cell) || ptr < base || ptr > end ||
        (ptr - base) % sizeof(cell) != 0 || count * sizeof(cell) > end - ptr)
        return NULL;
    return (cell *)ptr;
}

static int read_text(AMX *amx, cell address, text_value *out) {
    cell *start = cells(amx, address, 1);
    if (!start) return 0;
    out->packed = (ucell)*start > UNPACKEDMAX;
    for (size_t i = 0; i <= TEXT_MAX; i++) {
        size_t pos = out->packed ? i / sizeof(cell) : i;
        if (!cells(amx, address, pos + 1)) return 0;
        ucell value = (ucell)start[pos];
        if (out->packed) value = (value >> (8 * (sizeof(cell) - 1 - i % sizeof(cell)))) & 255u;
        if (value > 255u) return 0;
        if (!value) { out->text[i] = '\0'; out->length = i; return 1; }
        if (i == TEXT_MAX) return 0;
        out->text[i] = (char)value;
    }
    return 0;
}

static cell write_text(AMX *amx, cell address, cell capacity, int packed, const char *text) {
    if (capacity <= 0 || (ucell)capacity > TEXT_MAX + 1u) return -1;
    cell *target = cells(amx, address, (size_t)capacity);
    if (!target) return -1;
    size_t maximum = (size_t)capacity * (packed ? sizeof(cell) : 1) - 1;
    size_t length = strlen(text);
    if (length > maximum) length = maximum;
    /* Only initialize cells we write, never the unused tail of the array. */
    if (packed) {
        size_t written = (length + 1 + sizeof(cell) - 1) / sizeof(cell);
        memset(target, 0, written * sizeof(cell));
        for (size_t i = 0; i < length; i++)
            target[i / sizeof(cell)] |= (cell)((ucell)(unsigned char)text[i] << (8 * (sizeof(cell) - 1 - i % sizeof(cell))));
    } else {
        for (size_t i = 0; i < length; i++) target[i] = (unsigned char)text[i];
        target[length] = 0;
    }
    return (cell)length;
}

int ccpawn_valid_cells(AMX *amx, cell address, size_t count) {
    return cells(amx, address, count) != NULL;
}
char *ccpawn_read_string(AMX *amx, cell address) {
    text_value value;
    if (!read_text(amx, address, &value)) return NULL;
    char *result = malloc(value.length + 1);
    if (result) memcpy(result, value.text, value.length + 1);
    return result;
}
cell ccpawn_write_string(AMX *amx, cell address, cell capacity, int packed, const char *text) {
    return write_text(amx, address, capacity, packed, text);
}

static int ascii_lower(int c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }
static int ascii_upper(int c) { return c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c; }

static cell AMX_NATIVE_CALL n_strlen(AMX *amx, const cell *p) {
    text_value text;
    return arity(p, 1) && read_text(amx, p[1], &text) ? (cell)text.length : -1;
}
static cell AMX_NATIVE_CALL n_ispacked(AMX *amx, const cell *p) {
    text_value text;
    return arity(p, 1) && read_text(amx, p[1], &text) ? text.packed : -1;
}
static cell copy_string(AMX *amx, const cell *p, int packing) {
    text_value text;
    if (!arity(p, 3) || !read_text(amx, p[2], &text)) return -1;
    return write_text(amx, p[1], p[3], packing < 0 ? text.packed : packing, text.text);
}
static cell AMX_NATIVE_CALL n_strcopy(AMX *a, const cell *p) { return copy_string(a, p, -1); }
static cell AMX_NATIVE_CALL n_strpack(AMX *a, const cell *p) { return copy_string(a, p, 1); }
static cell AMX_NATIVE_CALL n_strunpack(AMX *a, const cell *p) { return copy_string(a, p, 0); }
static cell AMX_NATIVE_CALL n_strcat(AMX *amx, const cell *p) {
    text_value dest, source;
    if (!arity(p, 3) || !read_text(amx, p[1], &dest) || !read_text(amx, p[2], &source)) return -1;
    int packed = dest.length ? dest.packed : source.packed;
    if (p[3] <= 0 || (ucell)p[3] > TEXT_MAX + 1u) return -1;
    size_t cap = (size_t)p[3] * (packed ? sizeof(cell) : 1) - 1;
    if (dest.length > cap) return -1;
    size_t added = source.length;
    if (added > cap - dest.length) added = cap - dest.length;
    if (added > TEXT_MAX - dest.length) return -1;
    memcpy(dest.text + dest.length, source.text, added);
    dest.text[dest.length + added] = '\0';
    if (write_text(amx, p[1], p[3], packed, dest.text) < 0) return -1;
    return (cell)added;
}
static cell AMX_NATIVE_CALL n_strcmp(AMX *amx, const cell *p) {
    text_value a, b;
    if (!arity(p, 4) || p[4] < 0 || !read_text(amx, p[1], &a) || !read_text(amx, p[2], &b)) return -1;
    for (size_t i = 0; i < (size_t)p[4]; i++) {
        int x = i < a.length ? (unsigned char)a.text[i] : 0;
        int y = i < b.length ? (unsigned char)b.text[i] : 0;
        if (p[3]) { x = ascii_lower(x); y = ascii_lower(y); }
        if (x != y) return x < y ? -1 : 1;
        if (!x) return 0;
    }
    return 0;
}
static cell AMX_NATIVE_CALL n_strfind(AMX *amx, const cell *p) {
    text_value text, needle;
    if (!arity(p, 4) || p[4] < 0 || !read_text(amx, p[1], &text) || !read_text(amx, p[2], &needle)) return -1;
    if (needle.length > text.length || (size_t)p[4] > text.length) return -1;
    for (size_t i = (size_t)p[4]; i <= text.length - needle.length; i++) {
        size_t j = 0;
        while (j < needle.length) {
            int a = (unsigned char)text.text[i + j], b = (unsigned char)needle.text[j];
            if (p[3]) { a = ascii_lower(a); b = ascii_lower(b); }
            if (a != b) break;
            j++;
        }
        if (j == needle.length) return (cell)i;
    }
    return -1;
}
static cell AMX_NATIVE_CALL n_strval(AMX *amx, const cell *p) {
    text_value text;
    if (!arity(p, 2) || p[2] < 0 || !read_text(amx, p[1], &text) || (size_t)p[2] > text.length) return 0;
    errno = 0;
    long long value = strtoll(text.text + p[2], NULL, 10);
    if (errno == ERANGE || value < INT32_MIN || value > INT32_MAX) {
        amx_RaiseError(amx, AMX_ERR_DOMAIN); return 0;
    }
    return (cell)value;
}
static cell AMX_NATIVE_CALL n_valstr(AMX *amx, const cell *p) {
    if (!arity(p, 4)) return -1;
    char value[16];
    snprintf(value, sizeof value, "%d", (int)p[2]);
    return write_text(amx, p[1], p[4], p[3] != 0, value);
}

static int append(char *out, size_t *length, const char *value, size_t count) {
    if (count > TEXT_MAX - *length) return 0;
    memcpy(out + *length, value, count);
    *length += count;
    out[*length] = '\0';
    return 1;
}

/* Native varargs are references, not direct cell values. Restrict conversion
 * syntax rather than forwarding arbitrary user formats to C's printf. */
static int format_text(AMX *amx, const cell *p, size_t format_index, char out[TEXT_MAX + 1]) {
    text_value format;
    if (!arity(p, format_index) || !read_text(amx, p[format_index], &format)) return -1;
    size_t argument = format_index + 1, count = (size_t)p[0] / sizeof(cell), length = 0;
    out[0] = '\0';
    for (size_t i = 0; i < format.length; i++) {
        if (format.text[i] != '%') { if (!append(out, &length, format.text + i, 1)) return -1; continue; }
        if (++i >= format.length) return -1;
        if (format.text[i] == '%') { if (!append(out, &length, "%", 1)) return -1; continue; }
        char spec[32] = "%"; size_t spec_length = 1;
        while (i < format.length && strchr("-+ 0#", format.text[i])) {
            if (spec_length >= 8) return -1;
            spec[spec_length++] = format.text[i++];
        }
        unsigned width = 0;
        while (i < format.length && format.text[i] >= '0' && format.text[i] <= '9') {
            width = width * 10 + (unsigned)(format.text[i] - '0');
            if (width > 1024 || spec_length >= sizeof spec - 3) return -1;
            spec[spec_length++] = format.text[i++];
        }
        if (i < format.length && format.text[i] == '.') {
            spec[spec_length++] = format.text[i++]; unsigned precision = 0, digits = 0;
            while (i < format.length && format.text[i] >= '0' && format.text[i] <= '9') {
                precision = precision * 10 + (unsigned)(format.text[i] - '0'); digits++;
                if (precision > 1024 || spec_length >= sizeof spec - 3) return -1;
                spec[spec_length++] = format.text[i++];
            }
            if (!digits) return -1;
        }
        if (i >= format.length || argument > count) return -1;
        char conversion = format.text[i];
        if (!strchr("diuxXobcsfFeEgG", conversion)) return -1;
        spec[spec_length++] = conversion; spec[spec_length] = '\0';
        cell parameter = p[argument++];
        char piece[TEXT_MAX + 1]; int written = 0;
        if (conversion == 's') {
            text_value value;
            if (!read_text(amx, parameter, &value)) return -1;
            written = snprintf(piece, sizeof piece, spec, value.text);
        } else {
            cell *ref = cells(amx, parameter, 1);
            if (!ref) return -1;
            cell value = *ref;
            if (strchr("fFeEgG", conversion)) {
                float real; memcpy(&real, &value, sizeof real);
                written = snprintf(piece, sizeof piece, spec, (double)real);
            } else if (conversion == 'b') {
                /* Binary is a PAWN convenience; flag/precision forms are
                 * deliberately unsupported rather than interpreted by libc. */
                if (spec_length != 2) return -1;
                ucell bits = (ucell)value; char reverse[32]; size_t n = 0;
                do { reverse[n++] = (char)('0' + (bits & 1)); bits >>= 1; } while (bits);
                for (size_t j = 0; j < n; j++) piece[j] = reverse[n - j - 1];
                piece[n] = '\0'; written = (int)n;
            } else if (conversion == 'c') {
                if (value <= 0 || value > 255) return -1;
                written = snprintf(piece, sizeof piece, spec, (int)value);
            } else if (conversion == 'd' || conversion == 'i')
                written = snprintf(piece, sizeof piece, spec, (int)value);
            else written = snprintf(piece, sizeof piece, spec, (unsigned)(ucell)value);
        }
        if (written < 0 || (size_t)written >= sizeof piece || !append(out, &length, piece, (size_t)written)) return -1;
    }
    return (int)length;
}
static cell AMX_NATIVE_CALL n_printf(AMX *amx, const cell *p) {
    char out[TEXT_MAX + 1];
    int length = format_text(amx, p, 1, out);
    if (length < 0) return -1;
    return ccpawn_terminal_write(out) < 0 ? -1 : length;
}
static cell AMX_NATIVE_CALL n_strformat(AMX *amx, const cell *p) {
    char out[TEXT_MAX + 1];
    if (format_text(amx, p, 4, out) < 0) return -1;
    /* PAWN's strformat returns success, not a C snprintf required length. */
    return write_text(amx, p[1], p[2], p[3] != 0, out) < 0 ? -1 : 1;
}

static cell AMX_NATIVE_CALL n_min(AMX *a, const cell *p) { (void)a; return arity(p, 2) ? (p[1] < p[2] ? p[1] : p[2]) : 0; }
static cell AMX_NATIVE_CALL n_max(AMX *a, const cell *p) { (void)a; return arity(p, 2) ? (p[1] > p[2] ? p[1] : p[2]) : 0; }
static cell AMX_NATIVE_CALL n_clamp(AMX *a, const cell *p) {
    if (!arity(p, 3) || p[2] > p[3]) { amx_RaiseError(a, AMX_ERR_DOMAIN); return 0; }
    return p[1] < p[2] ? p[2] : p[1] > p[3] ? p[3] : p[1];
}
static cell AMX_NATIVE_CALL n_tolower(AMX *a, const cell *p) { (void)a; return arity(p, 1) ? ascii_lower(p[1]) : 0; }
static cell AMX_NATIVE_CALL n_toupper(AMX *a, const cell *p) { (void)a; return arity(p, 1) ? ascii_upper(p[1]) : 0; }
static uint32_t random_state;
static cell AMX_NATIVE_CALL n_random(AMX *a, const cell *p) {
    if (!arity(p, 1) || p[1] <= 0) { amx_RaiseError(a, AMX_ERR_DOMAIN); return 0; }
    if (!random_state) {
        struct timespec stamp; timespec_get(&stamp, TIME_UTC);
        random_state = (uint32_t)stamp.tv_sec ^ (uint32_t)stamp.tv_nsec ^ (uint32_t)(uintptr_t)a;
        if (!random_state) random_state = 0xa341316c;
    }
    /* Unbiased rejection sampling, but intentionally not cryptographic. */
    uint32_t maximum = (uint32_t)p[1], threshold = (0u - maximum) % maximum, value;
    do {
        random_state ^= random_state << 13; random_state ^= random_state >> 17; random_state ^= random_state << 5;
        value = random_state;
    } while (value < threshold);
    return (cell)(value % maximum);
}

static float real(cell c) { float value; memcpy(&value, &c, sizeof value); return value; }
static cell number(AMX *amx, double value) {
    float narrowed = (float)value; cell bits;
    if (!isfinite(narrowed)) { amx_RaiseError(amx, AMX_ERR_DOMAIN); return 0; }
    memcpy(&bits, &narrowed, sizeof bits); return bits;
}
#define FLOAT_UNARY(name, expression) \
    static cell AMX_NATIVE_CALL name(AMX *a, const cell *p) { \
        if (!arity(p, 1)) { amx_RaiseError(a, AMX_ERR_PARAMS); return 0; } \
        double x = real(p[1]); return number(a, (expression)); }
#define FLOAT_BINARY(name, expression) \
    static cell AMX_NATIVE_CALL name(AMX *a, const cell *p) { \
        if (!arity(p, 2)) { amx_RaiseError(a, AMX_ERR_PARAMS); return 0; } \
        double x = real(p[1]), y = real(p[2]); return number(a, (expression)); }
FLOAT_BINARY(n_floatadd, x + y)
FLOAT_BINARY(n_floatsub, x - y)
FLOAT_BINARY(n_floatmul, x * y)
FLOAT_BINARY(n_floatdiv, x / y)
FLOAT_BINARY(n_floatpower, pow(x, y))
FLOAT_BINARY(n_floatlog, x > 0 && y > 0 && y != 1 ? log(x) / log(y) : NAN)
FLOAT_UNARY(n_floatfract, x - floor(x))
FLOAT_UNARY(n_floatsqroot, sqrt(x))
FLOAT_UNARY(n_floatabs, fabs(x))
static cell AMX_NATIVE_CALL n_float(AMX *a, const cell *p) {
    return arity(p, 1) ? number(a, p[1]) : 0;
}
static cell AMX_NATIVE_CALL n_strfloat(AMX *a, const cell *p) {
    text_value value;
    if (!arity(p, 1) || !read_text(a, p[1], &value)) { amx_RaiseError(a, AMX_ERR_PARAMS); return 0; }
    return number(a, strtod(value.text, NULL));
}
static cell AMX_NATIVE_CALL n_floatcmp(AMX *a, const cell *p) {
    if (!arity(p, 2)) { amx_RaiseError(a, AMX_ERR_PARAMS); return 0; }
    float x = real(p[1]), y = real(p[2]);
    if (!isfinite(x) || !isfinite(y)) { amx_RaiseError(a, AMX_ERR_DOMAIN); return 0; }
    return x < y ? -1 : x > y ? 1 : 0;
}
static cell rounded(AMX *a, double x, cell method) {
    double value;
    switch (method) {
        case 0: value = floor(x + 0.5); break; /* upstream PAWN convention */
        case 1: value = floor(x); break;
        case 2: value = ceil(x); break;
        case 3: value = trunc(x); break;
        case 4: { double lo = floor(x), delta = x - lo; value = delta < 0.5 ? lo : delta > 0.5 ? lo + 1 : fmod(lo, 2) == 0 ? lo : lo + 1; break; }
        default: amx_RaiseError(a, AMX_ERR_PARAMS); return 0;
    }
    if (!isfinite(value) || value < INT32_MIN || value > INT32_MAX) { amx_RaiseError(a, AMX_ERR_DOMAIN); return 0; }
    return (cell)value;
}
static cell AMX_NATIVE_CALL n_floatround(AMX *a, const cell *p) {
    if (!arity(p, 2)) { amx_RaiseError(a, AMX_ERR_PARAMS); return 0; }
    return rounded(a, real(p[1]), p[2]);
}
static cell AMX_NATIVE_CALL n_floatint(AMX *a, const cell *p) {
    if (!arity(p, 1)) { amx_RaiseError(a, AMX_ERR_PARAMS); return 0; }
    return rounded(a, real(p[1]), 3);
}
static cell trig(AMX *a, const cell *p, int function) {
    if (!arity(p, 2) || p[2] < 0 || p[2] > 2) { amx_RaiseError(a, AMX_ERR_PARAMS); return 0; }
    double value = real(p[1]);
    if (p[2] == 1) value *= 3.14159265358979323846 / 180;
    if (p[2] == 2) value *= 3.14159265358979323846 / 200;
    return number(a, function == 0 ? sin(value) : function == 1 ? cos(value) : tan(value));
}
static cell AMX_NATIVE_CALL n_floatsin(AMX *a, const cell *p) { return trig(a, p, 0); }
static cell AMX_NATIVE_CALL n_floatcos(AMX *a, const cell *p) { return trig(a, p, 1); }
static cell AMX_NATIVE_CALL n_floattan(AMX *a, const cell *p) { return trig(a, p, 2); }

/* A forged AMX bytecode file may lie about its native argument byte count.
 * Check the complete parameter vector before any wrapper indexes it. */
int ccpawn_valid_parameters(AMX *amx, const cell *p, size_t minimum) {
    cell address = (cell)(uintptr_t)p;
    if (cells(amx, address, 1) != p || p[0] < 0 ||
        p[0] % sizeof(cell) != 0 || (ucell)p[0] > 128u * sizeof(cell)) return 0;
    if ((size_t)p[0] / sizeof(cell) < minimum) return 0;
    return cells(amx, address, 1 + (size_t)p[0] / sizeof(cell)) == p;
}
#define GUARDED(name) \
    static cell AMX_NATIVE_CALL safe_##name(AMX *a, const cell *p) { \
        if (!ccpawn_valid_parameters(a, p, 0)) { amx_RaiseError(a, AMX_ERR_PARAMS); return -1; } \
        return name(a, p); \
    }
GUARDED(n_printf)
GUARDED(n_strformat)
GUARDED(n_strlen)
GUARDED(n_strcmp)
GUARDED(n_strcopy)
GUARDED(n_strcat)
GUARDED(n_strfind)
GUARDED(n_strval)
GUARDED(n_valstr)
GUARDED(n_ispacked)
GUARDED(n_strpack)
GUARDED(n_strunpack)
GUARDED(n_min)
GUARDED(n_max)
GUARDED(n_clamp)
GUARDED(n_random)
GUARDED(n_tolower)
GUARDED(n_toupper)
GUARDED(n_float)
GUARDED(n_strfloat)
GUARDED(n_floatmul)
GUARDED(n_floatdiv)
GUARDED(n_floatadd)
GUARDED(n_floatsub)
GUARDED(n_floatfract)
GUARDED(n_floatround)
GUARDED(n_floatcmp)
GUARDED(n_floatsqroot)
GUARDED(n_floatpower)
GUARDED(n_floatlog)
GUARDED(n_floatsin)
GUARDED(n_floatcos)
GUARDED(n_floattan)
GUARDED(n_floatabs)
GUARDED(n_floatint)
#define NATIVE(name, function) {name, safe_##function}
static const AMX_NATIVE_INFO standard_natives[] = {
    NATIVE("printf", n_printf), NATIVE("strformat", n_strformat),
    NATIVE("strlen", n_strlen), NATIVE("strcmp", n_strcmp), NATIVE("strcopy", n_strcopy),
    NATIVE("strcat", n_strcat), NATIVE("strfind", n_strfind), NATIVE("strval", n_strval), NATIVE("valstr", n_valstr),
    NATIVE("ispacked", n_ispacked), NATIVE("strpack", n_strpack), NATIVE("strunpack", n_strunpack),
    NATIVE("min", n_min), NATIVE("max", n_max), NATIVE("clamp", n_clamp), NATIVE("random", n_random),
    NATIVE("tolower", n_tolower), NATIVE("toupper", n_toupper),
    NATIVE("float", n_float), NATIVE("strfloat", n_strfloat), NATIVE("floatmul", n_floatmul),
    NATIVE("floatdiv", n_floatdiv), NATIVE("floatadd", n_floatadd), NATIVE("floatsub", n_floatsub),
    NATIVE("floatfract", n_floatfract), NATIVE("floatround", n_floatround), NATIVE("floatcmp", n_floatcmp),
    NATIVE("floatsqroot", n_floatsqroot), NATIVE("floatpower", n_floatpower), NATIVE("floatlog", n_floatlog),
    NATIVE("floatsin", n_floatsin), NATIVE("floatcos", n_floatcos), NATIVE("floattan", n_floattan),
    NATIVE("floatabs", n_floatabs), NATIVE("floatint", n_floatint), {NULL, NULL}
};
int ccpawn_stdlib_register(AMX *amx) { return amx_Register(amx, standard_natives, -1); }
