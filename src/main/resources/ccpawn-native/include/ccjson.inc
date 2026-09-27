#if defined _ccjson_included
  #endinput
#endif
#define _ccjson_included
#include <computercraft>

#define CC_JSON_NULL   0
#define CC_JSON_BOOL   1
#define CC_JSON_NUMBER 2
#define CC_JSON_STRING 3
#define CC_JSON_ARRAY  4
#define CC_JSON_OBJECT 5

// Handles belong to this program run; 0 means allocation/parse failure.
stock cc_json_parse(const text[])
{
    if (cc_call("__cc", "json_parse", "s", text) < 1) return 0;
    return cc_result_int(0);
}

stock cc_json_object()
{
    if (cc_call("__cc", "json_create", "s", "object") < 1) return 0;
    return cc_result_int(0);
}

stock cc_json_array()
{
    if (cc_call("__cc", "json_create", "s", "array") < 1) return 0;
    return cc_result_int(0);
}

// Fails before copying if size is insufficient: no silently truncated JSON.
stock cc_json_stringify(handle, destination[], size = sizeof destination)
{
    if (cc_call("__cc", "json_stringify", "ii", handle, size) < 1) return -1;
    return cc_result_string(0, destination, size);
}

// Return count (1) or -1. Read the value through cc_result_* immediately.
// JSON null has CC_RESULT_NIL. Missing paths are errors, not JSON null.
// Objects/arrays are returned as JSON text (CC_RESULT_STRING).
stock cc_json_get(handle, const pointer[] = ``'')
{
    return cc_call("__cc", "json_get", "is", handle, pointer);
}

stock cc_json_type(handle, const pointer[] = ``'')
{
    if (cc_call("__cc", "json_type", "is", handle, pointer) < 1) return -1;
    return cc_result_int(0);
}

stock bool:cc_json_set_string(handle, const pointer[], const value[])
{
    if (cc_call("__cc", "json_set", "isss", handle, pointer, "string", value) < 1) return false;
    return cc_result_bool(0);
}

stock bool:cc_json_set_int(handle, const pointer[], value)
{
    if (cc_call("__cc", "json_set", "issi", handle, pointer, "number", value) < 1) return false;
    return cc_result_bool(0);
}

stock bool:cc_json_set_float(handle, const pointer[], Float:value)
{
    if (cc_call("__cc", "json_set", "issf", handle, pointer, "number", value) < 1) return false;
    return cc_result_bool(0);
}

stock bool:cc_json_set_bool(handle, const pointer[], bool:value)
{
    if (cc_call("__cc", "json_set", "issb", handle, pointer, "boolean", value) < 1) return false;
    return cc_result_bool(0);
}

stock bool:cc_json_set_null(handle, const pointer[])
{
    if (cc_call("__cc", "json_set", "iss", handle, pointer, "null") < 1) return false;
    return cc_result_bool(0);
}

stock bool:cc_json_set_json(handle, const pointer[], const text[])
{
    if (cc_call("__cc", "json_set", "isss", handle, pointer, "json", text) < 1) return false;
    return cc_result_bool(0);
}

stock bool:cc_json_free(handle)
{
    if (cc_call("__cc", "json_free", "i", handle) < 1) return false;
    return cc_result_bool(0);
}
