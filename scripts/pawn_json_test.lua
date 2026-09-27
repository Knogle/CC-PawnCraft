-- Run inside CraftOS, or locally with:
-- luatex --luaonly scripts/pawn_json_test.lua /path/to/cc-tweaked/projects/core/src/main/resources/data/computercraft/lua
-- The local harness loads the real CC textutils JSON implementation. Only the
-- runtime environment (module loading and UTF-8 helpers) is shimmed, not JSON.
if not textutils then
    local rom_root = assert(arg and arg[1], "pass the local CC Lua resource directory")
    table.pack = table.pack or function(...) return { n = select("#", ...), ... } end
    table.unpack = table.unpack or unpack
    local function load_environment(path, environment)
        if not setfenv then return assert(loadfile(path, "t", environment)) end
        local chunk = assert(loadfile(path))
        setfenv(chunk, environment)
        return chunk
    end
    utf8 = utf8 or { char = function(code)
        if code < 0x80 then return string.char(code) end
        if code < 0x800 then return string.char(0xc0 + math.floor(code / 64), 0x80 + code % 64) end
        return string.char(0xe0 + math.floor(code / 4096), 0x80 + math.floor(code / 64) % 64, 0x80 + code % 64)
    end }
    local modules = {}
    local function cc_require(name)
        if modules[name] then return modules[name] end
        local environment = setmetatable({}, { __index = _G })
        environment.require = cc_require
        local chunk = load_environment(rom_root .. "/rom/modules/main/" .. name:gsub("%.", "/") .. ".lua", environment)
        modules[name] = chunk()
        return modules[name]
    end
    textutils = setmetatable({ dofile = function(path)
        assert(path == "rom/modules/main/cc/internal/tiny_require.lua")
        return cc_require
    end }, { __index = _G })
    local chunk = load_environment(rom_root .. "/rom/apis/textutils.lua", textutils)
    chunk()
end

local json = assert(loadfile("src/main/resources/data/computercraft/lua/rom/modules/main/ccpawn/json.lua"))()
local passed = 0
local function test(name, body)
    body()
    passed = passed + 1
    print("PASS JSON: " .. name)
end
local function store()
    local instance = json.new()
    return function(method, ...)
        return instance.dispatch("json_" .. method, { ... })
    end
end
local function fails(pattern, body)
    local ok, message = pcall(body)
    assert(not ok, "operation unexpectedly succeeded")
    assert(tostring(message):find(pattern, 1, true), tostring(message))
end

test("parse false and primitive roots", function()
    local call = store()
    local handle = call("parse", "false")
    assert(call("get", handle, "") == false)
    assert(call("type", handle, "") == 1)
    assert(call("stringify", handle) == "false")
    assert(call("get", call("parse", "42"), "") == 42)
    -- Documents may retain finite Lua numbers outside PAWN Float32 range.
    -- The bridge rejects numeric access, not JSON parsing/stringification.
    local large = call("parse", "1e100")
    assert(call("type", large, "") == 2)
    local large_roundtrip = call("parse", call("stringify", large))
    assert(call("get", large_roundtrip, "") == 1e100)
end)

test("null, empty arrays, empty objects and nested lookup", function()
    local call = store()
    local handle = call("parse", '{"n":null,"a":[],"o":{},"items":[{"count":12}]}')
    assert(call("type", handle, "/n") == 0 and call("get", handle, "/n") == nil)
    assert(call("type", handle, "/a") == 4 and call("type", handle, "/o") == 5)
    assert(call("get", handle, "/items/0/count") == 12)
    local roundtrip = call("parse", call("stringify", handle))
    assert(call("type", roundtrip, "/a") == 4 and call("type", roundtrip, "/n") == 0)
    assert(call("get", call("parse", "null"), "") == nil)
end)

test("JSON Pointer escapes and empty keys", function()
    local call = store()
    local handle = call("parse", '{"a/b":{"~key":7},"":8,"~1":9}')
    assert(call("get", handle, "/a~1b/~0key") == 7)
    assert(call("get", handle, "/") == 8)
    assert(call("get", handle, "/~01") == 9)
    fails("invalid JSON Pointer escape", function() call("get", handle, "/~2") end)
    fails("start with /", function() call("get", handle, "a") end)
end)

test("typed setters, nested replacement, false value", function()
    local call = store()
    local handle = call("create", "object")
    call("set", handle, "/active", "boolean", false)
    call("set", handle, "/label", "string", "Ernter")
    call("set", handle, "/count", "number", 12)
    call("set", handle, "/fraction", "number", 0.5)
    call("set", handle, "/nothing", "null")
    call("set", handle, "/nested", "json", '{"on":true}')
    call("set", handle, "/nested/on", "boolean", false)
    assert(call("get", handle, "/active") == false)
    assert(call("get", handle, "/nested/on") == false)
    assert(call("get", handle, "/label") == "Ernter")
    assert(call("get", handle, "/fraction") == 0.5)
end)

test("array append and replacement without sparse arrays", function()
    local call = store()
    local handle = call("create", "array")
    assert(call("stringify", handle) == "[]")
    call("set", handle, "/-", "number", 7)
    call("set", handle, "/1", "boolean", false)
    call("set", handle, "/0", "null")
    assert(call("stringify", handle) == "[null,false]")
    fails("out of range", function() call("set", handle, "/4", "number", 8) end)
    fails("zero-based", function() call("get", handle, "/01") end)
    fails("zero-based", function() call("get", handle, "/-1") end)
    fails("zero-based", function() call("get", handle, "/-") end)
end)

test("empty-array sentinels never mutated", function()
    local call = store()
    local handle = call("parse", '{"a":[],"b":[]}')
    call("set", handle, "/a/-", "string", "x")
    assert(call("get", handle, "/a/0") == "x")
    assert(call("get", handle, "/b") == "[]")
    assert(textutils.serialiseJSON(textutils.empty_json_array) == "[]")
end)

test("missing paths distinct from null and scalar traversal", function()
    local call = store()
    local handle = call("parse", '{"nil":null,"scalar":3}')
    fails("does not exist", function() call("get", handle, "/absent") end)
    fails("traverses a scalar", function() call("get", handle, "/scalar/x") end)
    fails("parent pointer", function() call("set", handle, "/absent/x", "number", 1) end)
    fails("traverses a scalar", function() call("set", handle, "/nil/x", "number", 1) end)
end)

test("root replacement", function()
    local call = store()
    local handle = call("create", "object")
    call("set", handle, "", "boolean", false)
    assert(call("stringify", handle) == "false")
    call("set", handle, "", "null")
    assert(call("stringify", handle) == "null")
    call("set", handle, "", "json", "[]")
    assert(call("stringify", handle) == "[]")
end)

test("bad JSON and non-finite numbers fail", function()
    local call = store()
    fails("JSON:", function() call("parse", "{broken}") end)
    fails("finite", function() call("parse", "1e9999") end)
    local handle = call("create", "object")
    fails("finite", function() call("set", handle, "/bad", "number", 0 / 0) end)
    fails("finite", function() call("set", handle, "/bad", "number", math.huge) end)
    fails("incorrect setter", function() call("set", handle, "/bad", "boolean", 1) end)
    assert(call("stringify", handle) == "{}")
end)

test("NUL rejected instead of PAWN string truncation", function()
    local call = store()
    fails("NUL", function() call("parse", '"hello\\u0000world"') end)
    fails("NUL", function() call("parse", '{"\\u0000":1}') end)
    fails("NUL", function() call("parse", '"\0"') end)
end)

test("depth bound before parsing and on mutation", function()
    local call = store()
    call("parse", string.rep("[", 32) .. "0" .. string.rep("]", 32))
    fails("32 levels", function() call("parse", string.rep("[", 33) .. "0" .. string.rep("]", 33)) end)
    local handle = call("parse", string.rep("[", 32) .. "0" .. string.rep("]", 32))
    local before = call("stringify", handle)
    fails("32 levels", function() call("set", handle, string.rep("/0", 32), "json", "{}") end)
    assert(call("stringify", handle) == before)
    call("parse", '"' .. string.rep("[", 50) .. '"')
end)

test("node budget", function()
    local call = store()
    call("parse", "[" .. string.rep("0,", 2046) .. "0]")
    fails("2048 values", function() call("parse", "[" .. string.rep("0,", 2047) .. "0]") end)
end)

test("text/encoded budget and atomic writes", function()
    local call = store()
    fails("16 KiB", function() call("parse", '"' .. string.rep("a", 16383) .. '"') end)
    local handle = call("parse", '{"keep":3}')
    local before = call("stringify", handle)
    fails("16 KiB", function() call("set", handle, "/too_large", "string", string.rep("\n", 9000)) end)
    assert(call("stringify", handle) == before)
end)

test("destination buffer includes terminator", function()
    local call = store()
    local handle = call("create", "object")
    assert(call("stringify", handle, 3) == "{}")
    fails("too small", function() call("stringify", handle, 2) end)
    fails("too small", function() call("stringify", handle, 0) end)
end)

test("bounded handles, explicit freeing, no stale reuse", function()
    local call = store()
    local first = call("create", "object")
    for _ = 2, 32 do call("create", "array") end
    fails("32 live", function() call("create", "object") end)
    assert(call("free", first))
    local replacement = call("create", "object")
    assert(replacement ~= first)
    fails("freed handle", function() call("get", first, "") end)
    fails("freed handle", function() call("free", first) end)
end)

test("per-program ownership and unknown operations", function()
    local call, other = store(), store()
    local handle = call("create", "object")
    fails("freed handle", function() other("get", handle, "") end)
    fails("unknown operation", function() call("no_such_method") end)
    fails("freed handle", function() call("get", 1.5, "") end)
end)

test("UTF-8 BMP text and composite results round-trip", function()
    local call = store()
    local handle = call("parse", '{"nested":{"name":"Gr\\u00fc\\u00dfe"}}')
    local expected = "Gr" .. utf8.char(252) .. utf8.char(223) .. "e"
    assert(call("get", handle, "/nested/name") == expected)
    local roundtrip = call("parse", call("stringify", handle))
    assert(call("get", roundtrip, "/nested/name") == expected)
    local nested = call("parse", call("get", handle, "/nested"))
    assert(call("get", nested, "/name") == expected)
    call("set", handle, "/label", "string", utf8.char(228))
    assert(call("get", call("parse", call("stringify", handle)), "/label") == utf8.char(228))
end)

print(("JSON module: %d tests passed using real CC textutils"):format(passed))
