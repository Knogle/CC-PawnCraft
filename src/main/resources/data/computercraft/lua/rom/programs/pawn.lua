local arguments = { ... }
if #arguments == 0 or arguments[1] == "--help" then
    print("Usage: pawn <program.amx>")
    return
end

local runtime = require("ccpawn.runtime").new()
local json = require("ccpawn.json").new()
local function hex_decode(value)
    if value == "-" then return "" end
    if #value % 2 ~= 0 or value:find("[^0-9a-fA-F]") then error("invalid runner hex string", 0) end
    return (value:gsub("..", function(pair) return string.char(tonumber(pair, 16)) end))
end

local function hex_encode(value)
    if value == "" then return "-" end
    return (value:gsub(".", function(character) return ("%02x"):format(character:byte()) end))
end

local function split(line)
    local fields = {}
    for value in (line .. "\t"):gmatch("(.-)\t") do fields[#fields + 1] = value end
    return fields
end

local allowed_apis = {
    term = term,
    redstone = redstone,
    fs = fs,
    peripheral = peripheral,
    turtle = turtle,
    pocket = pocket,
    os = os,
    disk = disk,
    gps = gps,
    settings = settings,
    rednet = rednet,
    commands = commands,
    paintutils = paintutils,
    colors = colors,
    keys = keys,
    help = help,
}

local function decode_argument(kind, value)
    if kind == "I" or kind == "F" then return assert(tonumber(value), "invalid number") end
    if kind == "B" then return value ~= "0" end
    if kind == "S" then return hex_decode(value) end
    if kind == "J" then return textutils.unserialiseJSON(hex_decode(value)) end
    error("unsupported PAWN argument type " .. tostring(kind), 0)
end

local function encode_result(value)
    local kind = type(value)
    if kind == "nil" then return "N", "-" end
    if kind == "boolean" then return "B", value and "1" or "0" end
    if kind == "number" then
        -- Lua numbers are wider than PAWN Float cells. Reject non-finite or
        -- out-of-range results instead of silently returning Infinity to PAWN.
        if value ~= value or value == math.huge or value == -math.huge then
            error("CC returned a non-finite number", 0)
        end
        if math.abs(value) > 3.4028234663852886e38 then error("CC number exceeds PAWN Float32 range", 0) end
        if value == math.floor(value) and value >= -2147483648 and value <= 2147483647 then
            return "I", tostring(value)
        end
        return "F", ("%.9g"):format(value)
    end
    if kind == "string" then return "S", hex_encode(value) end
    if kind == "table" then return "J", hex_encode(textutils.serialiseJSON(value)) end
    error("CC returned unsupported type " .. kind, 0)
end

local function special_call(method, call_arguments)
    if method == "write" then
        write(call_arguments[1] or "")
        return
    elseif method == "sleep" then
        runtime.sleep(call_arguments[1] or 0)
        return
    elseif method == "yield" then
        runtime.sleep(0)
        return
    elseif method == "fs_read" then
        local file, message = fs.open(call_arguments[1], "rb")
        if not file then error(message or "cannot open file", 0) end
        local contents = file.readAll()
        file.close()
        return contents
    elseif method == "fs_write" then
        local mode = call_arguments[3] and "ab" or "wb"
        local file, message = fs.open(call_arguments[1], mode)
        if not file then error(message or "cannot open file", 0) end
        file.write(call_arguments[2])
        file.close()
        return true
    end
    if method:sub(1, 5) == "json_" then return json.dispatch(method, call_arguments) end
    return runtime.dispatch(method, call_arguments)
end

local function dispatch(api_name, method, call_arguments)
    if api_name == "__cc" then return special_call(method, call_arguments) end
    if api_name == "os" and method == "pullEvent" then
        return runtime.dispatch("pull_event", { call_arguments[1] or "" })
    end
    if api_name == "os" and method == "sleep" then
        return runtime.sleep(math.ceil((call_arguments[1] or 0) * 1000))
    end
    local api = allowed_apis[api_name]
    if not api then error("CC API is not available/allowed: " .. api_name, 0) end
    local callable = api[method]
    if type(callable) ~= "function" then error("unknown " .. api_name .. "." .. method, 0) end
    return callable(table.unpack(call_arguments, 1, call_arguments.n))
end

local waiting_for_cc = false
local function handle_call(handle, fields)
    local sequence = assert(tonumber(fields[2]), "missing call sequence")
    local api_name = hex_decode(fields[3])
    local method = hex_decode(fields[4])
    local count = assert(tonumber(fields[5]), "missing argument count")
    if count < 0 or count > 16 or #fields ~= 5 + count * 2 then error("malformed CALL", 0) end
    local call_arguments = { n = count }
    for index = 1, count do
        call_arguments[index] = decode_argument(fields[4 + index * 2], fields[5 + index * 2])
    end

    waiting_for_cc = true
    local returned = table.pack(pcall(dispatch, api_name, method, call_arguments))
    -- Clear before responding: the worker may finish normally as soon as it
    -- receives this reply, which must not race with the process monitor.
    waiting_for_cc = false
    if not returned[1] then
        -- Ctrl+T must unwind run() so the outer cleanup closes the native VM.
        if returned[2] == "Terminated" then error("Terminated", 0) end
        ccpawn_native.respond(handle, sequence, "ERROR\t" .. sequence .. "\t" .. hex_encode(tostring(returned[2])))
        return
    end
    local encoded_ok, response = pcall(function()
        local encoded = {}
        for index = 2, returned.n do
            local kind, value = encode_result(returned[index])
            encoded[#encoded + 1] = kind
            encoded[#encoded + 1] = value
        end
        local result = "RETURN\t" .. sequence .. "\t" .. (returned.n - 1)
        if #encoded > 0 then result = result .. "\t" .. table.concat(encoded, "\t") end
        return result
    end)
    if not encoded_ok then
        response = "ERROR\t" .. sequence .. "\t" .. hex_encode(tostring(response))
    end
    ccpawn_native.respond(handle, sequence, response)
end

local path = shell.resolve(arguments[1])
local file, open_error = fs.open(path, "rb")
if not file then error(open_error or ("cannot open " .. path), 0) end
local program = file.readAll()
file.close()

local handle = ccpawn_native.start(program)
local function run()
    while true do
        local message = ccpawn_native.take(handle)
        if not message then
            repeat
                local _, changed = os.pullEvent("ccpawn_process")
            until changed == handle
        else
            local fields = split(message)
            if fields[1] == "CALL" then
                handle_call(handle, fields)
            elseif fields[1] == "DONE" then
                return tonumber(fields[2]) or 0
            elseif fields[1] == "READY" then
                -- The next queued message contains either a call or completion.
            elseif fields[1] == "ERROR" then
                error(("AMX error %s: %s"):format(fields[2] or "?", fields[3] or "unknown"), 0)
            elseif fields[1] == "WATCHDOG" then
                error(fields[2] or "PAWN watchdog stopped the program", 0)
            elseif fields[1] == "PROCESS_ERROR" then
                error("PAWN transport error: " .. (fields[2] or "unknown"), 0)
            elseif fields[1] == "PROCESS_EXIT" and not ccpawn_native.isAlive(handle) then
                error("PAWN runner exited unexpectedly (" .. (fields[2] or "?") .. ")", 0)
            end
        end
    end
end

local function monitor_process()
    while true do
        -- Check before waiting too: the process may have died before this
        -- coroutine first starts, leaving a queued call waiting for an event.
        if waiting_for_cc and not ccpawn_native.isAlive(handle) then
            error("PAWN runner exited while waiting for a CC API call", 0)
        end
        repeat
            local _, changed = os.pullEvent("ccpawn_process")
        until changed == handle
    end
end

local ok, result = pcall(function()
    local exit_code
    parallel.waitForAny(function() exit_code = run() end, runtime.pump, monitor_process)
    return exit_code
end)
runtime.close()
ccpawn_native.close(handle)
if not ok then error(result, 0) end
if result ~= 0 then error("PAWN program returned " .. result, 0) end
