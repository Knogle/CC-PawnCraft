-- Per-PAWN-program JSON documents. No state is shared between program runs.
local MAX_TEXT, MAX_DEPTH, MAX_NODES, MAX_HANDLES = 16384, 32, 2048, 32
local type_ids = { null = 0, boolean = 1, number = 2, string = 3, array = 4, object = 5 }

local function fail(message) error("JSON: " .. message, 0) end
local function bounded_string(value, label)
    if type(value) ~= "string" then fail(label .. " must be a string") end
    if #value > MAX_TEXT then fail(label .. " exceeds 16 KiB") end
    -- PAWN strings are NUL-terminated. Reject rather than silently truncating.
    if value:find("\0", 1, true) then fail(label .. " contains a NUL byte") end
    return value
end

local function kind(value)
    if value == textutils.json_null then return "null" end
    if value == textutils.empty_json_array then return "array" end
    local result = type(value)
    if result == "table" then return rawget(value, 1) ~= nil and "array" or "object" end
    return result
end

-- Bound nesting before entering textutils' recursive parser (including invalid JSON).
local function preflight(text)
    bounded_string(text, "text")
    local depth, quoted, escaped = 0, false, false
    for index = 1, #text do
        local character = text:sub(index, index)
        if quoted then
            if escaped then escaped = false
            elseif character == "\\" then escaped = true
            elseif character == '"' then quoted = false end
        elseif character == '"' then quoted = true
        elseif character == "[" or character == "{" then
            depth = depth + 1
            if depth > MAX_DEPTH then fail("nesting exceeds 32 levels") end
        elseif character == "]" or character == "}" then depth = depth - 1 end
    end
end

local function validate(value, depth, budget)
    budget.count = budget.count + 1
    if budget.count > MAX_NODES then fail("document exceeds 2048 values") end
    local value_kind = kind(value)
    if value_kind == "number" then
        if value ~= value or value == math.huge or value == -math.huge then fail("number must be finite") end
    elseif value_kind == "string" then bounded_string(value, "string")
    elseif value_kind == "array" or value_kind == "object" then
        if depth >= MAX_DEPTH then fail("nesting exceeds 32 levels") end
        if value ~= textutils.empty_json_array then
            for key, child in pairs(value) do
                if type(key) == "string" then bounded_string(key, "object key") end
                validate(child, depth + 1, budget)
            end
        end
    elseif value_kind ~= "null" and value_kind ~= "boolean" then fail("unsupported value type") end
end

local function serialise(value)
    validate(value, 0, { count = 0 })
    local text = textutils.serialiseJSON(value, { unicode_strings = true })
    if #text > MAX_TEXT then fail("serialised document exceeds 16 KiB") end
    return text
end

local function parse(text)
    preflight(text)
    local value, message = textutils.unserialiseJSON(text, { parse_null = true, parse_empty_array = true })
    if value == nil then fail(message or "invalid JSON") end
    serialise(value)
    return value
end

local function pointer(path)
    bounded_string(path, "pointer")
    if path == "" then return {} end
    if path:sub(1, 1) ~= "/" then fail("pointer must be empty or start with /") end
    local result = {}
    for token in (path:sub(2) .. "/"):gmatch("(.-)/") do
        if token:gsub("~[01]", ""):find("~", 1, true) then fail("invalid JSON Pointer escape") end
        result[#result + 1] = token:gsub("~1", "/"):gsub("~0", "~")
        if #result > MAX_DEPTH then fail("pointer exceeds 32 levels") end
    end
    return result
end

local function key_for(parent, token, append)
    local parent_kind = kind(parent)
    if parent_kind == "object" then return token end
    if parent_kind ~= "array" then fail("pointer traverses a scalar") end
    local length = parent == textutils.empty_json_array and 0 or #parent
    if token == "-" and append then return length + 1 end
    if token ~= "0" and not token:match("^[1-9][0-9]*$") then fail("array index must be a zero-based integer") end
    local index = tonumber(token)
    if not index or index > length or (not append and index == length) then fail("array index out of range") end
    return index + 1
end

local function get(root, parts)
    local current = root
    for _, token in ipairs(parts) do
        local key = key_for(current, token, false)
        current = current[key]
        if current == nil then fail("pointer does not exist") end
    end
    return current
end

-- Copy only the path being changed. Validate before committing, so failed writes
-- cannot leave a partially modified document or mutate textutils' shared sentinels.
local function replace(current, parts, position, value)
    if position > #parts then return value end
    local last = position == #parts
    local key = key_for(current, parts[position], last)
    if not last and current[key] == nil then fail("parent pointer does not exist") end
    local copy = {}
    if current ~= textutils.empty_json_array then
        for old_key, old_value in pairs(current) do copy[old_key] = old_value end
    end
    if last then copy[key] = value
    else copy[key] = replace(current[key], parts, position + 1, value) end
    return copy
end

local function new()
    local documents, count, next_handle = {}, 0, 1
    local function document(handle)
        if type(handle) ~= "number" or handle % 1 ~= 0 or documents[handle] == nil then fail("invalid or freed handle") end
        return documents[handle]
    end
    local function allocate(value)
        if count >= MAX_HANDLES then fail("at most 32 live documents are allowed; free unused handles") end
        if next_handle > 2147483647 then fail("handle space exhausted") end
        local handle = next_handle
        next_handle, count = next_handle + 1, count + 1
        documents[handle] = value
        return handle
    end
    local function dispatch(method, args)
        if method == "json_parse" then return allocate(parse(args[1]))
        elseif method == "json_create" then
            if args[1] == "object" then return allocate({}) end
            if args[1] == "array" then return allocate(textutils.empty_json_array) end
            fail("create expects object or array")
        elseif method == "json_free" then
            document(args[1])
            documents[args[1]], count = nil, count - 1
            return true
        elseif method == "json_stringify" then
            local text = serialise(document(args[1]))
            if args[2] ~= nil and (type(args[2]) ~= "number" or args[2] % 1 ~= 0 or args[2] <= #text) then
                fail("destination buffer is too small (including the string terminator)")
            end
            return text
        elseif method == "json_get" or method == "json_type" then
            local value = get(document(args[1]), pointer(args[2] or ""))
            if method == "json_type" then return type_ids[kind(value)] end
            if value == textutils.json_null then return nil end
            if type(value) == "table" then return serialise(value) end
            return value
        elseif method == "json_set" then
            local root, parts = document(args[1]), pointer(args[2])
            local value_kind, value = args[3], args[4]
            if value_kind == "null" then value = textutils.json_null
            elseif value_kind == "json" then value = parse(value)
            elseif value_kind == "string" then bounded_string(value, "value")
            elseif value_kind == "number" or value_kind == "boolean" then
                if type(value) ~= value_kind then fail("incorrect setter value type") end
            else fail("unknown setter type") end
            local changed = replace(root, parts, 1, value)
            serialise(changed)
            documents[args[1]] = changed
            return true
        end
        fail("unknown operation " .. tostring(method))
    end
    return { dispatch = dispatch }
end

return { new = new }
