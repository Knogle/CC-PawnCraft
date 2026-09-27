local args = { ... }

local function usage()
    print("Usage: pawncc <source.pwn> [-o <program.amx>]")
end

if #args == 0 or args[1] == "--help" then
    usage()
    return
end

local source_path = shell.resolve(args[1])
local output_path
local index = 2
while index <= #args do
    if args[index] == "-o" and args[index + 1] then
        output_path = shell.resolve(args[index + 1])
        index = index + 2
    else
        error("unknown option: " .. tostring(args[index]), 0)
    end
end
if not output_path then
    output_path = source_path:gsub("%.pwn$", "") .. ".amx"
end

local input, open_error = fs.open(source_path, "rb")
if not input then error(open_error or ("cannot open " .. source_path), 0) end
local source = input.readAll()
input.close()

local job = ccpawn_native.compile(source)
while true do
    local done, successful, program, diagnostics = ccpawn_native.compileResult(job)
    if done then
        if diagnostics and diagnostics ~= "" then write(diagnostics) end
        if not successful then error("PAWN compilation failed", 0) end
        local output, output_error = fs.open(output_path, "wb")
        if not output then error(output_error or ("cannot write " .. output_path), 0) end
        output.write(program)
        output.close()
        print(("Wrote %s (%d bytes)"):format(output_path, #program))
        return
    end
    repeat
        local _, completed = os.pullEvent("ccpawn_compile")
    until completed == job
end
