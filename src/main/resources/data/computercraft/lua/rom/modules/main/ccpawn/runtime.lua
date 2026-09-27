-- Per-program CC event/timer state. No host OS access or PAWN callbacks.
local module = {}

function module.new()
    local snapshot = { n = 0 }
    local timers, physical = {}, {}
    local count = 0
    local queue, first, last = {}, 1, 0
    local token = tostring({})
    local runtime = {}

    local function integer(value, name)
        if type(value) ~= "number" or value ~= math.floor(value) or value < 0 or value > 2147483647 then
            error("invalid " .. name, 0)
        end
        return value
    end

    local function consume(id)
        local timer = timers[id]
        if not timer or not timer.pending then return nil end
        timer.pending = false
        if timer.single then timers[id] = nil; count = count - 1 end
        return { n = 2, "timer", id }
    end

    local function push(event, timer)
        if last - first + 1 >= 128 then error("PAWN event queue exceeds 128 events", 0) end
        last = last + 1
        queue[last] = { event = event, timer = timer }
        os.queueEvent("ccpawn_event", token)
    end

    function runtime.pump()
        while true do
            local event = table.pack(os.pullEvent())
            local raw = event[1] == "timer" and event[2]
            local timer = raw and physical[raw]
            if timer then
                physical[raw] = nil
                timer.raw = nil
                if not timer.single then
                    timer.raw = os.startTimer(timer.seconds)
                    physical[timer.raw] = timer
                end
                if not timer.pending then
                    timer.pending = true
                    push({ n = 2, "timer", timer.id }, timer.id)
                end
            elseif event[1]:sub(1, 7) ~= "ccpawn_" then
                push(event)
            end
        end
    end

    local function pull(filter)
        if type(filter) ~= "string" then error("event filter must be a string", 0) end
        while true do
            while first <= last do
                local item = queue[first]
                queue[first], first = nil, first + 1
                local event = item.event
                if item.timer then event = consume(item.timer) end
                if event and (filter == "" or event[1] == filter) then return event end
            end
            queue, first, last = {}, 1, 0
            os.pullEvent("ccpawn_event")
        end
    end

    function runtime.sleep(milliseconds)
        local id = runtime.dispatch("start_timer", { milliseconds, true })
        repeat
            local event = pull("timer")
        until event[2] == id
    end

    function runtime.dispatch(method, args)
        if method == "pull_event" then
            snapshot = pull(args[1] or "")
            if snapshot.n > 16 then snapshot = { n = 0 }; error("at most 16 event values supported", 0) end
            return table.unpack(snapshot, 1, snapshot.n)
        elseif method == "event_count" then
            return snapshot.n
        elseif method == "event_value" then
            local index = integer(args[1], "event index")
            if index >= snapshot.n then error("event index out of range", 0) end
            return snapshot[index + 1]
        elseif method == "start_timer" then
            local ms = integer(args[1], "timer duration")
            if type(args[2]) ~= "boolean" then error("singleshot must be boolean", 0) end
            if count >= 32 then error("at most 32 outstanding PAWN timers", 0) end
            if not args[2] and ms == 0 then error("repeating timer duration must be positive", 0) end
            local seconds = math.max(ms, 50) / 1000
            local id = os.startTimer(seconds)
            local timer = { id = id, raw = id, seconds = seconds, single = args[2], pending = false }
            timers[id], physical[id] = timer, timer
            count = count + 1
            return id
        elseif method == "cancel_timer" then
            local id = integer(args[1], "timer ID")
            local timer = timers[id]
            if not timer then return false end
            if timer.raw then os.cancelTimer(timer.raw); physical[timer.raw] = nil end
            timers[id] = nil
            count = count - 1
            return true
        elseif method == "calendar" then
            local seconds = math.floor(os.epoch("utc") / 1000)
            if seconds < 0 or seconds > 2147483647 then error("Unix timestamp exceeds PAWN signed 32-bit range", 0) end
            local date = os.date("!*t", seconds)
            return seconds, date.year, date.month, date.day, date.hour, date.min, date.sec, date.yday
        elseif method == "tickcount" then
            local ms = math.floor(os.clock() * 1000 + 0.5) % 4294967296
            if ms >= 2147483648 then ms = ms - 4294967296 end
            return ms
        elseif method == "peripheral_find" then
            if type(args[1]) ~= "string" then error("peripheral type must be a string", 0) end
            local names = peripheral.getNames()
            table.sort(names)
            for _, name in ipairs(names) do
                if peripheral.hasType(name, args[1]) then return name end
            end
            return ""
        end
        error("unknown CC runtime method " .. tostring(method), 0)
    end

    function runtime.close()
        for _, timer in pairs(timers) do if timer.raw then os.cancelTimer(timer.raw) end end
        timers, physical, count = {}, {}, 0
        snapshot = { n = 0 }
        queue, first, last = {}, 1, 0
    end

    return runtime
end

return module
