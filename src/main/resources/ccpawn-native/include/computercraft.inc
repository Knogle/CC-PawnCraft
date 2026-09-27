#if defined _computercraft_included
  #endinput
#endif
#define _computercraft_included

#define CC_RESULT_NIL    0
#define CC_RESULT_INT    1
#define CC_RESULT_BOOL   2
#define CC_RESULT_FLOAT  3
#define CC_RESULT_STRING 4
#define CC_RESULT_JSON   5

native cc_call(const api[], const method[], const format[], {Float,_}:...);
native cc_result_count();
native cc_result_type(index);
native cc_result_int(index);
native bool:cc_result_bool(index);
native Float:cc_result_float(index);
native cc_result_string(index, destination[], size = sizeof destination);
native cc_last_error(destination[], size = sizeof destination);
native cc_sleep(milliseconds);
native cc_yield();
native print(const text[]);

stock cc_println(const text[])
{
    print(text);
    print("\n");
}

stock bool:cc_redstone_get_input(const side[])
{
    if (cc_call("redstone", "getInput", "s", side) < 1)
        return false;
    return cc_result_bool(0);
}

stock cc_redstone_set_output(const side[], bool:value)
{
    return cc_call("redstone", "setOutput", "sb", side, value);
}

stock cc_redstone_get_analog_input(const side[])
{
    if (cc_call("redstone", "getAnalogInput", "s", side) < 1)
        return 0;
    return cc_result_int(0);
}

stock cc_redstone_set_analog_output(const side[], value)
{
    return cc_call("redstone", "setAnalogOutput", "si", side, value);
}

stock bool:cc_fs_exists(const path[])
{
    if (cc_call("fs", "exists", "s", path) < 1)
        return false;
    return cc_result_bool(0);
}

stock cc_fs_read(const path[], destination[], size)
{
    if (cc_call("__cc", "fs_read", "s", path) < 1)
        return 0;
    return cc_result_string(0, destination, size);
}

stock bool:cc_fs_write(const path[], const contents[], bool:append = false)
{
    if (cc_call("__cc", "fs_write", "ssb", path, contents, append) < 1)
        return false;
    return cc_result_bool(0);
}

#include <ccstdlib>
#include <ccevents>
#include <ccperipheral>
#include <ccjson>
