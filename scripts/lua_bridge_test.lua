-- Local Lua-only regression test. No Minecraft instance is contacted.
table.pack = table.pack or function(...) return { n = select("#", ...), ... } end
table.unpack = table.unpack or unpack
package.path = "src/main/resources/data/computercraft/lua/rom/modules/main/?.lua;" .. package.path
parallel = { waitForAny = function(first) return first() end }
local old_pull, old_start = os.pullEvent, os.startTimer
os.pullEvent = function() error("Terminated", 0) end
os.startTimer = function() return 1 end
os.cancelTimer = function() end

local function test_termination()
    local closed, responses = false, 0
    local messages = {
        "CALL\t1\t5f5f6363\t736c656570\t1\tI\t50",
        "DONE\t0",
    }
    shell = { resolve = function(path) return path end }
    fs = { open = function()
        return { readAll = function() return "mock AMX" end, close = function() end }
    end }
    sleep = function() error("Terminated", 0) end
    ccpawn_native = {
        start = function() return 1 end,
        take = function() return table.remove(messages, 1) end,
        respond = function() responses = responses + 1 end,
        close = function() closed = true end,
    }
    local run = assert(loadfile("src/main/resources/data/computercraft/lua/rom/programs/pawn.lua"))
    local ok, message = pcall(run, "ernter.amx")
    assert(not ok and message == "Terminated", tostring(message))
    assert(closed, "Ctrl+T must close the native process")
    assert(responses == 0, "termination must not be swallowed as an ordinary API failure")
end

local function test_numeric_result(value, expected_type, expected_error)
    local closed, response = false, nil
    local messages = {
        -- os.numericTest(), a local stub: no CC/world or native process exists.
        "CALL\t1\t6f73\t6e756d6572696354657374\t0",
        "DONE\t0",
    }
    os.numericTest = function() return value end
    shell = { resolve = function(path) return path end }
    fs = { open = function()
        return { readAll = function() return "mock AMX" end, close = function() end }
    end }
    ccpawn_native = {
        start = function() return 1 end,
        take = function() return table.remove(messages, 1) end,
        respond = function(_, _, result) response = result end,
        close = function() closed = true end,
    }
    assert(loadfile("src/main/resources/data/computercraft/lua/rom/programs/pawn.lua"))("numeric.amx")
    assert(closed and response, "numeric call must respond and close")
    if expected_error then
        local hex = assert(response:match("^ERROR\t1\t([0-9a-f]+)$"), response)
        local message = hex:gsub("..", function(pair) return string.char(tonumber(pair, 16)) end)
        assert(message:find(expected_error, 1, true), message)
    else
        local actual_type, number = response:match("^RETURN\t1\t1\t([IF])\t(.+)$")
        assert(actual_type == expected_type, response)
        local decoded = assert(tonumber(number), response)
        assert(decoded == decoded and decoded ~= math.huge and decoded ~= -math.huge, response)
        if value == 0 then assert(decoded == 0)
        else assert(math.abs((decoded - value) / value) < 1e-8, response) end
    end
end

test_termination()
print("Lua bridge test passed: termination closes the runner")
for _, value in ipairs({ 0 / 0, math.huge, -math.huge }) do
    test_numeric_result(value, nil, "non-finite")
end
for _, value in ipairs({ 1e100, -1e100, 3.4028236e38, -3.4028236e38 }) do
    test_numeric_result(value, nil, "Float32 range")
end
for _, value in ipairs({ 0, 2147483647, -2147483648 }) do
    test_numeric_result(value, "I")
end
for _, value in ipairs({ 0.5, -0.5, 2147483648, -2147483649, 3.4028234663852886e38, -3.4028234663852886e38 }) do
    test_numeric_result(value, "F")
end
os.numericTest = nil
print("Lua bridge tests passed: 16 finite/range/boundary numeric results")

local function test_process_monitor(crash)
    local saved_parallel, saved_pull = parallel, os.pullEvent
    local closed, replied, alive, probes = false, false, true, 0
    local messages = { "CALL\t1\t6f73\t6e756d6572696354657374\t0" }
    shell = { resolve = function(path) return path end }
    fs = { open = function()
        return { readAll = function() return "mock AMX" end, close = function() end }
    end }
    os.pullEvent = function(filter) return coroutine.yield(filter) end
    os.numericTest = function() os.pullEvent("test_release"); return 7 end
    ccpawn_native = {
        start = function() return 1 end,
        take = function() return table.remove(messages, 1) end,
        respond = function() replied = true; alive = false end,
        isAlive = function() probes = probes + 1; return alive end,
        close = function() closed = true end,
    }
    parallel = { waitForAny = function(run, _, monitor)
        assert(type(monitor) == "function", "a pending-call process monitor is required")
        local user, watch = coroutine.create(run), coroutine.create(monitor)
        local ok, filter = coroutine.resume(user)
        assert(ok and filter == "test_release", tostring(filter))
        ok, filter = coroutine.resume(watch)
        assert(ok and filter == "ccpawn_process", tostring(filter))
        local initial_probes = probes
        alive = not crash
        ok, filter = coroutine.resume(watch, "ccpawn_process", 99)
        assert(ok and filter == "ccpawn_process" and probes == initial_probes,
            "another program's process events must not trigger a probe")
        if crash then
            ok, filter = coroutine.resume(watch, "ccpawn_process", 1)
            assert(not ok, "dead worker must interrupt the pending CC call")
            error(filter, 0)
        end
        -- The CC call completes; responding makes the simulated worker exit.
        -- Delay DONE until the monitor has observed the process notification.
        ok, filter = coroutine.resume(user, "test_release")
        assert(ok and filter == "ccpawn_process" and replied, tostring(filter))
        initial_probes = probes
        ok, filter = coroutine.resume(watch, "ccpawn_process", 1)
        assert(ok and filter == "ccpawn_process" and probes == initial_probes,
            "normal completion after replying must not be misclassified as a crash")
        messages[1] = "DONE\t0"
        ok, filter = coroutine.resume(user, "ccpawn_process", 1)
        assert(ok and coroutine.status(user) == "dead", tostring(filter))
        return 1
    end }
    local run = assert(loadfile("src/main/resources/data/computercraft/lua/rom/programs/pawn.lua"))
    local ok, message = pcall(run, "monitor.amx")
    parallel, os.pullEvent, os.numericTest = saved_parallel, saved_pull, nil
    assert(closed, "monitor exit must run native-process cleanup")
    if crash then
        assert(not ok and message == "PAWN runner exited while waiting for a CC API call", tostring(message))
        assert(not replied, "do not respond to the dead worker")
    else
        assert(ok and replied, tostring(message))
    end
end

test_process_monitor(true)
test_process_monitor(false)
print("Lua bridge tests passed: worker crash interrupts CC wait, normal DONE is not a crash")
