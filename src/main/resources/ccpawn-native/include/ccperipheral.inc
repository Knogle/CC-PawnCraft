#if defined _ccperipheral_included
    #endinput
#endif
#define _ccperipheral_included
#include <computercraft>

// At most 14 method arguments. Uses the same i/b/f/s/j format as cc_call.
native cc_peripheral_call(const name[], const method[], const format[], {Float,_}:...);

stock bool:cc_peripheral_is_present(const name[])
{
    if (cc_call("peripheral", "isPresent", "s", name) < 1) return false;
    return cc_result_bool(0);
}

stock bool:cc_peripheral_has_type(const name[], const type[])
{
    if (cc_call("peripheral", "hasType", "ss", name, type) < 1) return false;
    return cc_result_bool(0);
}

stock cc_peripheral_get_type(const name[], destination[], size = sizeof destination)
{
    if (cc_call("peripheral", "getType", "s", name) < 1) return -1;
    return cc_result_string(0, destination, size);
}

// Finds a name, not a Lua table of closures. Empty string / 0 means not found.
stock cc_peripheral_find(const type[], destination[], size = sizeof destination)
{
    if (cc_call("__cc", "peripheral_find", "s", type) < 1) return -1;
    return cc_result_string(0, destination, size);
}

// List of names is returned as JSON through cc_result_string(0,...).
stock cc_peripheral_get_names()
    return cc_call("peripheral", "getNames", "");

stock cc_peripheral_get_methods(const name[])
    return cc_call("peripheral", "getMethods", "s", name);

stock bool:cc_redstone_get_output(const side[])
{
    if (cc_call("redstone", "getOutput", "s", side) < 1) return false;
    return cc_result_bool(0);
}

stock cc_redstone_get_analog_output(const side[])
{
    if (cc_call("redstone", "getAnalogOutput", "s", side) < 1) return -1;
    return cc_result_int(0);
}
