-- Fast, deterministic tests of the real runtime module. No Minecraft/server access.
table.pack = table.pack or function(...) return { n = select("#", ...), ... } end
table.unpack = table.unpack or unpack
local real_os = os
local runtime_module = assert(loadfile("src/main/resources/data/computercraft/lua/rom/modules/main/ccpawn/runtime.lua"))()
local passed = 0

local function equal(actual, expected, label)
    assert(actual == expected, (label or "value") .. ": expected " .. tostring(expected) .. ", got " .. tostring(actual))
end

local function fails(action, fragment)
    local ok, message = pcall(action)
    assert(not ok, "expected an error containing " .. fragment)
    assert(tostring(message):find(fragment, 1, true), "unexpected error: " .. tostring(message))
end

local function size(value)
    local count = 0
    for _ in pairs(value) do count = count + 1 end
    return count
end

local function fixture()
    local f = { active = {}, cancelled = {}, wakes = {}, next_id = 0, clock = 0, epoch = 0 }
    os = {
        pullEvent = function(filter)
            local event = table.pack(coroutine.yield(filter))
            if event[1] == "terminate" then error("Terminated", 0) end
            assert(not filter or filter == event[1], "mock scheduler delivered an event to the wrong filter")
            return table.unpack(event, 1, event.n)
        end,
        queueEvent = function(...) f.wakes[#f.wakes + 1] = table.pack(...) end,
        startTimer = function(seconds)
            f.next_id = f.next_id + 1
            f.active[f.next_id] = seconds
            return f.next_id
        end,
        cancelTimer = function(id)
            f.cancelled[id] = true
            f.active[id] = nil
        end,
        epoch = function(locale) equal(locale, "utc", "calendar locale"); return f.epoch end,
        date = function(format, timestamp)
            equal(format, "!*t", "calendar date format")
            return real_os.date(format, timestamp)
        end,
        clock = function() return f.clock end,
    }
    f.runtime = runtime_module.new()
    f.pump = coroutine.create(f.runtime.pump)
    assert(coroutine.resume(f.pump))

    function f.emit(...)
        local ok, message = coroutine.resume(f.pump, ...)
        assert(ok, tostring(message))
        equal(coroutine.status(f.pump), "suspended", "pump remains active")
    end

    function f.fire(id)
        assert(f.active[id], "cannot fire an inactive timer " .. tostring(id))
        f.active[id] = nil
        f.emit("timer", id)
    end

    function f.read(filter)
        return table.pack(f.runtime.dispatch("pull_event", { filter or "" }))
    end

    function f.wake(thread)
        local wake = table.remove(f.wakes, 1)
        assert(wake, "expected an internal event notification")
        local result = table.pack(coroutine.resume(thread, table.unpack(wake, 1, wake.n)))
        assert(result[1], tostring(result[2]))
        return result
    end

    return f
end

local function test(name, action)
    local f = fixture()
    local ok, message = pcall(action, f)
    f.runtime.close()
    equal(size(f.active), 0, "cleanup must cancel every physical timer")
    os = real_os
    assert(ok, name .. ": " .. tostring(message))
    passed = passed + 1
    print("PASS " .. name)
end

test("128 buffered events preserve order and queue capacity is reusable", function(f)
    for index = 1, 128 do f.emit("sample", index) end
    equal(#f.wakes, 128)
    for index = 1, 128 do
        local event = f.read()
        equal(event.n, 2)
        equal(event[1], "sample")
        equal(event[2], index)
    end
    f.emit("after_drain", 129)
    equal(f.read()[1], "after_drain")
end)

test("129th queued event fails closed instead of growing indefinitely", function(f)
    for index = 1, 128 do f.emit("sample", index) end
    local ok, message = coroutine.resume(f.pump, "overflow", 129)
    equal(ok, false)
    assert(tostring(message):find("exceeds 128 events", 1, true), tostring(message))
    equal(coroutine.status(f.pump), "dead")
    equal(#f.wakes, 128)
end)

test("internal IPC events do not fill the user queue", function(f)
    for index = 1, 300 do f.emit("ccpawn_process", index) end
    f.emit("ccpawn_event", "different runtime token")
    equal(#f.wakes, 0)
    f.emit("redstone")
    equal(f.read()[1], "redstone")
end)

test("event filter, nil arguments and stable typed snapshot", function(f)
    f.emit("discard_me", 12)
    f.emit("sample", nil, false, -7, "text", 1.25)
    local event = f.read("sample")
    equal(event.n, 6)
    equal(f.runtime.dispatch("event_count", {}), 6)
    f.runtime.dispatch("calendar", {})
    equal(f.runtime.dispatch("event_value", { 0 }), "sample")
    equal(f.runtime.dispatch("event_value", { 1 }), nil)
    equal(f.runtime.dispatch("event_value", { 2 }), false)
    equal(f.runtime.dispatch("event_value", { 3 }), -7)
    equal(f.runtime.dispatch("event_value", { 4 }), "text")
    equal(f.runtime.dispatch("event_value", { 5 }), 1.25)
    fails(function() f.runtime.dispatch("event_value", { 6 }) end, "out of range")
    fails(function() f.runtime.dispatch("event_value", { -1 }) end, "event index")
    fails(function() f.runtime.dispatch("event_value", { 1.5 }) end, "event index")
end)

test("oversized event resets snapshot rather than truncating", function(f)
    local values = {}
    for index = 1, 16 do values[index] = index end
    f.emit("too_many", table.unpack(values))
    fails(function() f.read() end, "at most 16 event values")
    equal(f.runtime.dispatch("event_count", {}), 0)
end)

test("32 outstanding timer limit and capacity after cancellation", function(f)
    local ids = {}
    for index = 1, 32 do ids[index] = f.runtime.dispatch("start_timer", { 100, true }) end
    equal(size(f.active), 32)
    fails(function() f.runtime.dispatch("start_timer", { 100, true }) end, "at most 32")
    equal(f.runtime.dispatch("cancel_timer", { ids[1] }), true)
    equal(f.runtime.dispatch("cancel_timer", { ids[1] }), false)
    local replacement = f.runtime.dispatch("start_timer", { 100, true })
    assert(replacement > ids[32])
    equal(size(f.active), 32)
end)

test("one-shot timer frees capacity only after delivery", function(f)
    local ids = {}
    for index = 1, 32 do ids[index] = f.runtime.dispatch("start_timer", { 100, true }) end
    f.fire(ids[1])
    fails(function() f.runtime.dispatch("start_timer", { 100, true }) end, "at most 32")
    local event = f.read("timer")
    equal(event[2], ids[1])
    f.runtime.dispatch("start_timer", { 100, true })
    equal(size(f.active), 32)
end)

test("200 missed repeating ticks coalesce into one stable logical ID", function(f)
    local logical = f.runtime.dispatch("start_timer", { 100, false })
    for _ = 1, 200 do
        equal(size(f.active), 1)
        local physical = next(f.active)
        f.fire(physical)
    end
    equal(#f.wakes, 1, "missed intervals must not flood the queue")
    equal(f.read("timer")[2], logical)
    f.fire(next(f.active))
    equal(f.read("timer")[2], logical)
    equal(f.runtime.dispatch("cancel_timer", { logical }), true)
    equal(size(f.active), 0)
end)

test("cancellation removes a pending timer notification", function(f)
    for _, single in ipairs({ true, false }) do
        local logical = f.runtime.dispatch("start_timer", { 100, single })
        f.fire(logical)
        equal(f.runtime.dispatch("cancel_timer", { logical }), true)
        f.emit("marker", single)
        local event = f.read()
        equal(event[1], "marker", "cancelled pending timer must not be delivered")
        equal(event[2], single)
    end
end)

test("timer validation and minimum one-tick delay", function(f)
    local id = f.runtime.dispatch("start_timer", { 0, true })
    equal(f.active[id], 0.05)
    fails(function() f.runtime.dispatch("start_timer", { 0, false }) end, "must be positive")
    fails(function() f.runtime.dispatch("start_timer", { -1, true }) end, "timer duration")
    fails(function() f.runtime.dispatch("start_timer", { 1.5, true }) end, "timer duration")
    fails(function() f.runtime.dispatch("start_timer", { 2147483648, true }) end, "timer duration")
    fails(function() f.runtime.dispatch("start_timer", { 100, 1 }) end, "singleshot must be boolean")
end)

test("CC sleep waits for its own timer and leaves no timer event", function(f)
    f.emit("before_sleep", "preserve snapshot")
    f.read()
    f.wakes = {}
    local sleeper = coroutine.create(function() f.runtime.sleep(10) end)
    local ok, filter = coroutine.resume(sleeper)
    assert(ok, tostring(filter))
    equal(filter, "ccpawn_event")
    local physical = next(f.active)
    equal(f.active[physical], 0.05)
    f.fire(physical)
    f.wake(sleeper)
    equal(coroutine.status(sleeper), "dead")
    equal(f.runtime.dispatch("event_value", { 0 }), "before_sleep")
    f.emit("after_sleep")
    equal(f.read()[1], "after_sleep", "sleep timer leaked to the next event")
end)

test("close clears timers, queued events and snapshots", function(f)
    f.emit("old_snapshot")
    f.read()
    local repeating = f.runtime.dispatch("start_timer", { 100, false })
    local one_shot = f.runtime.dispatch("start_timer", { 1000, true })
    f.fire(repeating)
    f.emit("old_queue")
    local replacement = next(f.active)
    f.runtime.close()
    equal(size(f.active), 0)
    equal(f.cancelled[one_shot], true)
    equal(f.cancelled[replacement], true)
    equal(f.runtime.dispatch("event_count", {}), 0)
    f.emit("fresh_event")
    equal(f.read()[1], "fresh_event")
end)

test("UTC calendar and explicit signed-32-bit epoch boundary", function(f)
    f.epoch = 999
    local date = table.pack(f.runtime.dispatch("calendar", {}))
    equal(date.n, 8)
    for index, value in ipairs({ 0, 1970, 1, 1, 0, 0, 0, 1 }) do equal(date[index], value) end
    f.epoch = 2147483647000
    date = table.pack(f.runtime.dispatch("calendar", {}))
    for index, value in ipairs({ 2147483647, 2038, 1, 19, 3, 14, 7, 19 }) do equal(date[index], value) end
    f.epoch = 2147483648000
    fails(function() f.runtime.dispatch("calendar", {}) end, "signed 32-bit range")
    f.epoch = -1
    fails(function() f.runtime.dispatch("calendar", {}) end, "signed 32-bit range")
end)

test("tickcount wraps at the signed and unsigned 32-bit boundaries", function(f)
    for _, case in ipairs({ { 0, 0 }, { 0.05, 50 }, { 2147483.647, 2147483647 },
            { 2147483.648, -2147483648 }, { 4294967.295, -1 }, { 4294967.296, 0 }, { 4294967.346, 50 } }) do
        f.clock = case[1]
        equal(f.runtime.dispatch("tickcount", {}), case[2])
    end
end)

test("terminate escapes both event wait and event pump", function(f)
    local reader = coroutine.create(function() f.read() end)
    assert(coroutine.resume(reader))
    local ok, message = coroutine.resume(reader, "terminate")
    equal(ok, false)
    equal(message, "Terminated")
    ok, message = coroutine.resume(f.pump, "terminate")
    equal(ok, false)
    equal(message, "Terminated")
end)

print("PASS: " .. passed .. " PAWN runtime regression tests")
