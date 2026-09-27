#if defined _ccevents_included
    #endinput
#endif
#define _ccevents_included
#include <computercraft>

// Event indices: 0 = name, 1.. = arguments. Snapshot survives other CC calls.
stock cc_pull_event(const filter[] = ``'')
    return cc_call("__cc", "pull_event", "s", filter);

stock cc_event_count()
{
    if (cc_call("__cc", "event_count", "") < 1) return -1;
    return cc_result_int(0);
}

stock cc_event_type(index)
{
    if (cc_call("__cc", "event_value", "i", index) < 1) return -1;
    return cc_result_type(0);
}

stock cc_event_string(index, destination[], size = sizeof destination)
{
    if (cc_call("__cc", "event_value", "i", index) < 1) return -1;
    return cc_result_string(0, destination, size);
}

stock cc_event_name(destination[], size = sizeof destination)
    return cc_event_string(0, destination, size);

stock cc_event_int(index)
{
    if (cc_call("__cc", "event_value", "i", index) < 1) return 0;
    return cc_result_int(0);
}

stock bool:cc_event_bool(index)
{
    if (cc_call("__cc", "event_value", "i", index) < 1) return false;
    return cc_result_bool(0);
}

stock Float:cc_event_float(index)
{
    if (cc_call("__cc", "event_value", "i", index) < 1) return Float:0;
    return cc_result_float(0);
}

stock cc_start_timer(milliseconds)
{
    if (cc_call("__cc", "start_timer", "ib", milliseconds, true) < 1) return -1;
    return cc_result_int(0);
}

// CC adaptation: timer events, not the standalone PAWN @timer callback.
stock settimer(milliseconds, bool:singleshot = false)
{
    if (cc_call("__cc", "start_timer", "ib", milliseconds, singleshot) < 1) return -1;
    return cc_result_int(0);
}

stock cc_cancel_timer(timer_id)
{
    if (cc_call("__cc", "cancel_timer", "i", timer_id) < 1) return -1;
    return cc_result_bool(0) ? 1 : 0;
}

stock cc_wait_timer(timer_id)
{
    if (timer_id < 0) return -1;
    for (;;)
    {
        if (cc_pull_event("timer") < 2) return -1;
        if (cc_event_int(1) == timer_id) return 0;
    }
}

stock delay(milliseconds)
    return cc_sleep(milliseconds);

// UTC; calendar calls do not change the host/server clock.
stock gettime(&hour = 0, &minute = 0, &second = 0)
{
    if (cc_call("__cc", "calendar", "") < 8) return -1;
    hour = cc_result_int(4);
    minute = cc_result_int(5);
    second = cc_result_int(6);
    return cc_result_int(0);
}

stock getdate(&year = 0, &month = 0, &day = 0)
{
    if (cc_call("__cc", "calendar", "") < 8) return -1;
    year = cc_result_int(1);
    month = cc_result_int(2);
    day = cc_result_int(3);
    return cc_result_int(7);
}

// Computer uptime in milliseconds, signed 32-bit wrapping; one game tick = 50 ms.
stock tickcount(&granularity = 0)
{
    if (cc_call("__cc", "tickcount", "") < 1) return -1;
    granularity = 50;
    return cc_result_int(0);
}
