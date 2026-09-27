# PawnCraft — Complete User and API Guide

Version: **0.4.0** · Compiler: **CompuPhase PAWN 4.1.7222**
Reference target: Minecraft **1.21.1**, NeoForge **21.1.248**, CC:Tweaked **1.120.2**.

This standalone reference is intended for writing, reviewing, and generating PAWN
programs for **this addon**. It covers every bundled public PAWN helper, the
generic ComputerCraft bridge, complete programs, and important limitations.
It is not a guide to SA-MP/open.mp PAWN, C, or the entire Lua standard library.

Every fenced `pawn` example is a complete, independent program: copy one into
its own `.pwn` file. API signatures in tables describe declarations, not code
to paste into a program. `lua` blocks are explicitly Lua. Commands are labelled
as **CraftOS** (in-game computer) or **host shell** (development machine).

## Contents

1. [What the addon does](#1-what-the-addon-does)
2. [Install, edit, compile, and run](#2-install-edit-compile-and-run)
3. [PAWN language essentials](#3-pawn-language-essentials)
4. [Calls, results, and errors](#4-calls-results-and-errors)
5. [Terminal output and formatting](#5-terminal-output-and-formatting)
6. [Strings](#6-strings)
7. [Integer and floating-point mathematics](#7-integer-and-floating-point-mathematics)
8. [Redstone](#8-redstone)
9. [Events](#9-events)
10. [Timers, sleeping, and dates](#10-timers-sleeping-and-dates)
11. [Peripherals](#11-peripherals)
12. [Monitors and larger terminals](#12-monitors-and-larger-terminals)
13. [Files and persistent configuration](#13-files-and-persistent-configuration)
14. [JSON](#14-json)
15. [Modems, rednet, turtles, and chat](#15-modems-rednet-turtles-and-chat)
16. [Limits, lifecycle, and security](#16-limits-lifecycle-and-security)
17. [Troubleshooting and migration](#17-troubleshooting-and-migration)
18. [Code-generation checklist](#18-code-generation-checklist)
19. [Building, testing, and source references](#19-building-testing-and-source-references)

## 1. What the addon does

PawnCraft is an actual server-side NeoForge addon. It adds `pawncc` and `pawn`
commands to existing CC:Tweaked computers. It does **not** add another computer
block, a different GUI, or a PAWN interpreter written entirely in Lua.

```text
.pwn source -> native PAWN compiler -> .amx bytecode -> native AMX process
                                                            |
                                                    Java transport
                                                            |
                                              CraftOS Lua dispatcher
                                                            |
                                                real computer APIs
```

PAWN controls the same redstone sides, peripherals, terminal, and virtual files
as Lua on that computer. It gains no extra permissions or access to unattached
devices. API availability depends on the computer: `turtle` requires a turtle;
`commands` requires a suitable command computer and server permissions.

Use `#include <computercraft>` for all functions documented here. Other
supported include names are `default`, `ccstdlib`, `core`, `string`, `console`,
`float`, `time`, `ccevents`, `ccperipheral`, and `ccjson`. These are bundled
compatibility files, **not** the entire upstream PAWN standard library.

PawnCraft supports PAWN as its additional in-game language. The addon itself
uses Java to integrate with Minecraft/NeoForge and transport requests, but it
does not provide an in-game Java compiler or JVM worker.

## 2. Install, edit, compile, and run

### 2.1 Operator installation

- Install `pawncraft-0.4.0.jar` alongside compatible CC:Tweaked in the server's
  `mods` directory. Keep only one version of the addon there.
- Stop Minecraft cleanly and back up the instance before replacing a JAR.
  Restart the Minecraft instance after replacing it.
- The 0.4.0 artifact described here bundles **Linux x86_64** native programs,
  requiring **glibc 2.38 or newer**. The JVM must be able to extract and execute
  them in its temporary directory; a `noexec` mount can prevent this.
- The target Minecraft/NeoForge environment uses **Java 21**.
- On a dedicated server, clients do not need this addon JAR: it adds no custom
  blocks, screens, or network packets. They still need the normal client modpack.
- Another platform needs compatible compiler, runner, and includes supplied
  through `-Dccpawn.nativeDir=/absolute/path`. This is an advanced operator
  option, not automatic cross-platform support.

Do not casually make a container privileged to work around an execution error.
See [security](#16-limits-lifecycle-and-security).

### 2.2 Your first program

In **CraftOS**, run `edit hello.pwn` and enter:

```pawn
// example: hello.pwn
// doc-test: console
#include <computercraft>

main()
{
    if (printf("Hello from PAWN!\n") < 0)
        return 1;
    return 0;
}
```

Press **Ctrl**, choose **Save**, then **Exit**. Back in CraftOS:

```text
pawncc hello.pwn -o hello.amx
pawn hello.amx
```

The same procedure applies to every PAWN example in this guide.

### 2.3 Command reference

| CraftOS command | Meaning |
| --- | --- |
| `pawncc source.pwn` | Compile; replace the final `.pwn` suffix with `.amx`, or append `.amx` if absent. |
| `pawncc source.pwn -o program.amx` | Compile to an explicit output path. |
| `pawncc --help` | Compiler-command usage. |
| `pawn program.amx` | Run bytecode on this computer. |
| `pawn --help` | Runner-command usage. |
| `help pawn` | Short installed help page. |

The in-game compiler wrapper supports **only** the optional `-o` argument,
not the host compiler's `-i`, `-d`, or other switches.

Source must be UTF-8 and use allowed bundled includes. Put includes on their own
lines: the source validator rejects a trailing comment on an `#include` line.
User-created includes and absolute include paths are not supported in-game.

Recompile after editing. Failed compilation can leave an older `.amx` at the
output path: **do not run stale bytecode after an error**. Extra arguments after
`pawn program.amx` are not exposed as PAWN command-line arguments; configure
programs through files or CC settings instead.

### 2.4 Stopping and automatic startup

Hold **Ctrl+T** to terminate. Turning off or unloading the computer also closes
its PAWN processes. `main` returning `0` means success; a nonzero return value
makes the CraftOS runner report failure.

To start automatically, compile `/controller.amx` first, then create
`/startup.lua` in the CC filesystem:

```lua
-- Lua, not PAWN.
if not shell.run("pawn /controller.amx") then
    printError("PAWN controller failed")
end
```

Preserve existing startup automation rather than blindly replacing it.
This uses CraftOS's normal [startup mechanism](https://www.tweaked.cc/guide/startup.html).

Do not assume termination has performed your intended redstone cleanup. Set a
safe output before a controlled return. Forced termination cannot execute a
PAWN cleanup handler; critical machinery also needs a safe physical circuit.

## 3. PAWN language essentials

### 3.1 This is PAWN 4.1, not C or SA-MP

The entry point is `main()`, not `int main()`. Declare variables with `new`.

```pawn
// example: language_basics.pwn
// doc-test: console
#include <computercraft>

#define LIMIT 3
const Direction: { Forward = 0, Backward };

add(left, right)
{
    return left + right;
}

main()
{
    new count = 0;
    new bool:enabled = true;
    new Float:ratio = 0.75;
    new Direction:direction = Forward;
    new samples[3] = [2, 4, 6];

    while (count < LIMIT)
    {
        printf("sample[%d] = %d\n", count, samples[count]);
        count++;
    }

    direction = Backward;
    if (enabled && direction == Backward)
        printf("sum=%d, ratio=%.2f\n", add(2, 3), ratio);
    return 0;
}
```

- Write `while (condition) { ... }`, not Lua's `while true do`.
- Use `if (...) { ... } else { ... }`; `==` compares, `=` assigns.
- Logical operators: `&&`, `||`, `!`. Boolean values: `true` and `false`.
- `for (;;)` is an infinite loop. Long-running loops must cooperate with CC:
  use event waits, sleeps, or another appropriate CC operation.
- Enumeration-like constants use **`const Direction: { Forward, Backward };`**.
  Do not generate the old `enum` syntax used in other PAWN dialects.
- Numeric array initializers use **`[1, 2, 3]`**, not C-style `{1, 2, 3}`.
  Specify numeric array dimensions explicitly.
- `switch` cases do not fall through. Do not insert C-style `break` statements:
  `break` targets an enclosing loop.
- Integer division rounds toward negative infinity: `-7 / 3` is `-3`.
  The remainder follows the divisor's sign. Do not assume C division behavior.
- `bool:` and `Float:` are tags; an ordinary cell is signed 32-bit.
  There are no C pointers, heap objects, or Lua tables as PAWN variables.
- Reference parameters have `&` in their declaration, but call
  `gettime(hour, minute, second)` without adding `&` to the arguments.

### 3.2 Strings are arrays

`new text[128];` allocates **128 cells**. Strings are NUL-terminated.

Normal `"text"` literals are packed: four bytes per cell. Unpacked strings use
one byte per cell. Library functions accept both, but raw indexing differs:

- `text[index]` accesses a cell; `text{index}` accesses a packed byte.
- `sizeof text` counts cells, which the addon’s capacity parameters expect.
- `strcopy(text, "hello")` preserves the packed source representation.
- For predictable single-character `[]` indexing, use `strunpack` or
  `strformat(..., false, ...)`.
- Compare strings using `strcmp`/`strequal`, not `==`.

```pawn
// example: string_basics.pwn
// doc-test: console
#include <computercraft>

main()
{
    new text[64];
    if (strunpack(text, "Hello") < 0) return 1;
    text[0] = tolower(text[0]);
    if (!strequal(text, "hello")) return 2;

    if (strformat(text, sizeof text, false, "Count: %d", 12) < 0)
        return 3;
    printf("%s\n", text);
    return 0;
}
```

Strings have byte semantics. UTF-8 bytes can be transported, but indices and
lengths are not Unicode character counts. Case folding is ASCII-only;
truncation may split a UTF-8 sequence.

### 3.3 Functions and includes

`#include <computercraft>` already includes Float support and all addon helpers.
Define your own functions in the same `.pwn` file. `stock` is useful for optional
helper functions; `native` declares a function supplied by the host.

**Declaring a new native does not implement it.** Do not invent native
declarations for unsupported Lua or PAWN APIs.

## 4. Calls, results, and errors

### 4.1 Generic call

```text
cc_call(const api[], const method[], const format[], {Float,_}:...)
```

Success returns the **number of Lua return values**, 0–16. Failure returns
**−1**. This is not the first returned value. A successful setter often returns
**0**: `if (!cc_call(...))` is generally the wrong error check.

| Format | PAWN argument | Lua interpretation |
| --- | --- | --- |
| `i` | Integer cell | Integer-valued number |
| `b` | Boolean/cell | Zero → false; nonzero → true |
| `f` | `Float:` value | Floating-point number |
| `s` | String | String |
| `j` | JSON text string | Decoded JSON value, commonly a table |

Use `""` for no arguments. Otherwise supply one character per argument, without
spaces, `%`, or separators. Maximum: **16 arguments**. Extra PAWN arguments
beyond the format can be ignored; always match the counts exactly.

Pass scalar values normally, not addresses. For `f`, use `1.0` or
`float(integer)`. `Float:integer` reinterprets bits; it does not convert numbers.

`j` accepts JSON text, **not** a document handle. It uses the generic CC JSON
parser defaults, not the document API's null/empty-array preservation options.
There is no explicit nil format character; omit trailing optional arguments
where the underlying method allows this.

### 4.2 Available Lua API tables

The whitelist is exactly:

```text
term redstone fs peripheral turtle pocket os disk gps settings rednet
commands paintutils colors keys help
```

`__cc` contains internal dispatcher operations; application code should use the
documented helpers instead.

The bridge calls functions, not fields. `colors.red` and `keys.enter` are
constants, not callable methods. Lua aliases `rs` and `colours` are not allowed
API table names here.

Not directly bridged: `shell`, `http`, `io`, `textutils`, `window`, `parallel`,
`vector`, `math`, and Lua `string`. Use the addon helpers or CraftOS outside
PAWN. A whitelisted API can still be absent on a particular computer.

Only nil, booleans, numbers, strings, and JSON-compatible tables cross the
bridge. File handles, closures, wrapped peripherals, metatables, callbacks,
and terminal redirect objects do not. Being in the whitelist does not make
every method usable: do not request `fs.open` or `peripheral.wrap` as PAWN
object handles.

### 4.3 Result accessors and constants

Results are indexed from **0**.

| Constant | Value | Meaning |
| --- | --- | --- |
| `CC_RESULT_NIL` | 0 | Lua nil, or an invalid result index |
| `CC_RESULT_INT` | 1 | Signed 32-bit integer |
| `CC_RESULT_BOOL` | 2 | Boolean |
| `CC_RESULT_FLOAT` | 3 | Float32 |
| `CC_RESULT_STRING` | 4 | String |
| `CC_RESULT_JSON` | 5 | A Lua table serialized as JSON text |

| Function | Return / behavior |
| --- | --- |
| `cc_result_count()` | Stored return-value count. |
| `cc_result_type(index)` | Constant above; invalid index also gives `CC_RESULT_NIL`. |
| `cc_result_int(index)` | Stored integer/bool cell; **not** Float-to-int conversion. |
| `bool:cc_result_bool(index)` | Whether the stored integer/bool is nonzero; not general Lua truthiness. |
| `Float:cc_result_float(index)` | Stored Float, or numeric conversion of integer/bool. |
| `cc_result_string(index, destination[], size = sizeof destination)` | Copy string/JSON as **unpacked** text; return original source byte length, or −1 for invalid destination. Truncates and terminates if necessary. |
| `cc_last_error(destination[], size = sizeof destination)` | Copy diagnostic as unpacked text; return original diagnostic byte length, or −1 for invalid destination. |

Invalid indices and mismatched getters commonly yield 0, false, or an empty
string instead of a useful error. Check count/type first. Use
`floatround(cc_result_float(index))` for an intentional Float-to-int conversion.

**Copy results before another CC operation.** `printf`, `print`, sleeps,
event getters, peripheral helpers, and JSON helpers replace the generic result
buffer. Plain `cc_result_*` and `cc_last_error` accessors do not make another
CC call.

```pawn
// example: terminal_size.pwn
#include <computercraft>

main()
{
    if (cc_call("term", "getSize", "") != 2)
    {
        new error[512];
        cc_last_error(error);
        printf("Cannot read terminal size: %s\n", error);
        return 1;
    }
    new width = cc_result_int(0);
    new height = cc_result_int(1); // Copy before printf replaces the results.
    printf("Terminal: %d columns x %d rows\n", width, height);
    return 0;
}
```

For unpacked string getters, test `length < 0 || length >= sizeof buffer`.
That detects failure/truncation. Standard string helpers use different return
conventions, described below.

### 4.4 Error handling

| Family | Failure convention |
| --- | --- |
| `cc_call` / `cc_peripheral_call` | −1; 0 can be success |
| `cc_sleep` / `delay` / `cc_wait_timer` | −1; success is 0 |
| JSON allocation/parse | 0; success is a positive handle |
| JSON setters/free, boolean convenience helpers | false; some helpers cannot distinguish a legitimate false value |
| `printf` / `strformat` and most string-copy helpers | −1; other string functions have their own sentinel values |
| Invalid math domains / invalid native parameter vectors | AMX error terminating the program |

Read `cc_last_error` **before** printing or calling another helper. Local
format/string-library failures do not necessarily populate it: provide your own
message for those errors.

A convenience getter such as `cc_redstone_get_input` returns false for either
a low signal or an API error. Use `cc_call` and a type check when the distinction
matters:

```pawn
// example: checked_input.pwn
#include <computercraft>

fail_cc(const operation[])
{
    new error[512];
    cc_last_error(error);
    printf("%s failed: %s\n", operation, error);
    return 1;
}

main()
{
    if (cc_call("redstone", "getInput", "s", "right") != 1)
        return fail_cc("Read right input");
    if (cc_result_type(0) != CC_RESULT_BOOL)
    {
        cc_println("Unexpected input result type");
        return 1;
    }
    new bool:signal = cc_result_bool(0);
    printf("Right input: %d\n", signal);
    return 0;
}
```

Ctrl+T, a watchdog abort, and a dead runner stop the outer CraftOS program;
they are not ordinary recoverable `cc_call == -1` results. There is no PAWN
`try/catch` or Lua-style `pcall` helper.

## 5. Terminal output and formatting

| Function | Behavior / return |
| --- | --- |
| `print(const text[])` | Write text without adding a newline; 0 on success, −1 on error. Only one text argument is supported. |
| `cc_println(const text[])` | Write text followed by a newline. Convenience wrapper; do not rely on its return value for error handling. |
| `printf(const format[], {Float,bool,_}:...)` | Formatted output; emitted byte count or −1. Does not automatically add a newline. |
| `strformat(destination[], size = sizeof destination, bool:pack = true, const format[], {Float,bool,_}:...)` | Format into a buffer; **1 on success, including truncation**, −1 on failure. No terminal output. |

For `strformat`, explicitly supply the first four arguments:
`strformat(buffer, sizeof buffer, false, "Count: %d", count)`.
`false` requests unpacked output; the declaration's default packing is `true`.
There is no separate SA-MP-style `format` function.

`printf`, `print`, and `cc_println` use the current CC terminal and support
newlines, wrapping, scrolling, and an existing monitor redirect.
They do **not** send Minecraft chat messages.

| Conversion | Meaning |
| --- | --- |
| `%d`, `%i` | Signed integer; booleans display as 0/1 |
| `%u` | Unsigned 32-bit integer representation |
| `%x`, `%X`, `%o` | Hexadecimal / uppercase hexadecimal / octal |
| `%b` | Binary representation; no flags, width, or precision |
| `%c` | Single byte, value 1–255; not a Unicode code point |
| `%s` | Packed or unpacked string |
| `%f`, `%F`, `%e`, `%E`, `%g`, `%G` | `Float:` value |
| `%%` | Literal percent sign |

Fixed width/precision are available for ordinary numerical/string conversions:
`%04d`, `%8.2f`, `%.20s`. Width and precision are each limited to 1024.
Do not use `%n`, `%p`, `%ld`, `%*s`, positional formats, or unknown conversions.

`%f` requires Float bits: `printf("%f", 3)` does not mean `3.0`.
Always match the format to the argument. Use `printf("%s", untrusted_text)`,
never treat incoming text as the format itself.

The entire formatted output is checked before emission and is limited to
16,384 bytes. `strformat` may then truncate it to fit the destination, while
still returning 1; its return is **not** a required-length measurement.

```pawn
// example: formatting.pwn
// doc-test: console
#include <computercraft>

main()
{
    new message[128];
    new bool:signal = true;
    new Float:fill = 83.25;

    if (strformat(message, sizeof message, false,
                  "Signal=%d, fill=%.1f%%", signal, fill) < 0)
        return 1;
    printf("%s\n", message);
    printf("Timer: %d:%02d, mask=%b, hex=%04X\n", 2, 5, 5, 255);
    return 0;
}
```

## 6. Strings

All sizes below count **cells**, normally passed as `sizeof destination`.
Text lengths and search indices count **bytes**, excluding the terminator.

| Function | Meaning / return |
| --- | --- |
| `strlen(const string[])` | Byte length; −1 for invalid/unterminated/overlong text. |
| `strcmp(const first[], const second[], bool:ignorecase = false, length = cellmax)` | −1, 0, or 1 for less/equal/greater. Invalid input can also yield −1. |
| `bool:strequal(const first[], const second[], bool:ignorecase = false, length = cellmax)` | `strcmp(...) == 0`. |
| `strcopy(destination[], const source[], maxlength = sizeof destination)` | Copy, preserving source packing; return bytes actually written or −1. |
| `strcat(destination[], const source[], maxlength = sizeof destination)` | Append; return newly appended bytes, not the final length; −1 on failure. |
| `strfind(const string[], const substring[], bool:ignorecase = false, index = 0)` | First matching byte position at/after index; −1 if absent/invalid. |
| `strval(const string[], index = 0)` | Read a decimal integer prefix from index; 0 if no number/invalid input. Overflow terminates with an AMX domain error. |
| `valstr(destination[], value, bool:pack = true, size = sizeof destination)` | Integer to string; bytes actually written or −1. |
| `bool:ispacked(const string[])` | Query representation of a valid string; not a memory-validation function. |
| `strpack(destination[], const source[], maxlength = sizeof destination)` | Convert to packed text; bytes actually written or −1. |
| `strunpack(destination[], const source[], maxlength = sizeof destination)` | Convert to unpacked text; bytes actually written or −1. |

`strcat` keeps a nonempty destination's packing; an empty destination adopts
the source's packing. Copying functions truncate and NUL-terminate if necessary.
Unlike `cc_result_string`, their returned length is the copied length, so it
does not independently prove that the whole source fitted.

`valstr` has an extra optional size parameter compared with the original
three-argument PAWN native. Recompile external bytecode which used a different
declaration; source using two or three arguments remains supported.

Limits:

- Text scanned by these helpers: 16,384 bytes.
- Destination capacity: 1–16,385 cells.
- Unpacked character values must be bytes, 0–255.
- No embedded NUL data.
- `ignorecase`, `tolower`, and `toupper` are ASCII-only.
- Never lie about the destination size: VM-bound checks cannot reconstruct the
  actual size of an individual array from an incorrect argument.

`strval` accepts whitespace/sign and ignores text after a valid numeric prefix.
It is not strict configuration validation. Do not feed it unbounded untrusted
numbers expecting a recoverable overflow result.

```pawn
// example: string_parsing.pwn
// doc-test: console
#include <computercraft>

main()
{
    new setting[64], report[96];
    if (strunpack(setting, "delay=180") < 0) return 1;
    new equals = strfind(setting, "=");
    if (equals < 0) return 2;

    new seconds = strval(setting, equals + 1);
    seconds = clamp(seconds, 1, 3600);
    if (strformat(report, sizeof report, false,
                  "Delay: %d seconds", seconds) < 0)
        return 3;
    printf("%s\n", report);
    return 0;
}
```

## 7. Integer and floating-point mathematics

### 7.1 General helpers

| Function | Meaning |
| --- | --- |
| `min(value1, value2)` | Smaller integer. |
| `max(value1, value2)` | Larger integer. |
| `clamp(value, minimum = cellmin, maximum = cellmax)` | Clamp an integer to inclusive bounds. |
| `random(maximum)` | Integer from 0 through maximum−1; maximum must be positive. |
| `tolower(character)` | Convert an ASCII uppercase letter to lowercase. |
| `toupper(character)` | Convert an ASCII lowercase letter to uppercase. |

`random` is not cryptographically secure. `random(0)`, negative maxima, and
reversed `clamp` bounds raise an AMX domain error and terminate the program.
`min`/`max`/`clamp` are integer helpers, not overloaded Float functions.

### 7.2 Float API

All `Float:` values are IEEE-754 **32-bit**, not Lua's typical 64-bit numbers.

| Function | Meaning |
| --- | --- |
| `Float:float(value)` | Numeric integer-to-Float conversion. |
| `Float:strfloat(const string[])` | Parse a floating-point prefix; nonnumeric text gives 0.0. Not strict validation. |
| `Float:floatadd(Float:left, Float:right)` | Addition. |
| `Float:floatsub(Float:left, Float:right)` | Subtraction. |
| `Float:floatmul(Float:left, Float:right)` | Multiplication. |
| `Float:floatdiv(Float:left, Float:right)` | Division; denominator must not be zero. |
| `Float:floatabs(Float:value)` | Absolute value. |
| `Float:floatfract(Float:value)` | Value minus floor; `floatfract(-1.25)` is `0.75`. |
| `Float:floatsqroot(Float:value)` | Square root; value must be nonnegative. |
| `Float:floatpower(Float:value, Float:exponent)` | Exponentiation; arguments/result must have a valid real domain. |
| `Float:floatlog(Float:value, Float:base = 10.0)` | Logarithm; value >0, base >0 and !=1. |
| `Float:floatsin(Float:value, anglemode:mode = radian)` | Sine. |
| `Float:floatcos(Float:value, anglemode:mode = radian)` | Cosine. |
| `Float:floattan(Float:value, anglemode:mode = radian)` | Tangent. |
| `floatcmp(Float:left, Float:right)` | −1, 0, or 1. |
| `floatround(Float:value, floatround_method:method = floatround_round)` | Integer according to the selected rounding mode. |
| `floatint(Float:value)` | Integer truncated toward zero. |

Angle modes: `radian = 0`, `degrees = 1`, `grades = 2`. A full turn is 400 grades.

| Rounding mode | Value | Behavior |
| --- | --- | --- |
| `floatround_round` | 0 | Nearest; ties toward positive infinity: −1.5 → −1. |
| `floatround_floor` | 1 | Toward negative infinity. |
| `floatround_ceil` | 2 | Toward positive infinity. |
| `floatround_tozero` | 3 | Toward zero. |
| `floatround_unbiased` | 4 | Nearest; ties to even. |

The include implements Float arithmetic, comparisons, unary minus, increments,
and integer-to-Float assignment. Float remainder `%` is intentionally undefined.
Use Float literals or `float(...)` for **direct** Float-native arguments:
`floatsqroot(9.0)`, not `floatsqroot(9)`.

```pawn
// example: mathematics.pwn
// doc-test: console
#include <computercraft>

main()
{
    new strength = clamp(20, 0, 15);
    new Float:ratio = float(strength) / 15.0;
    new Float:percent = ratio * 100.0;

    printf("Level=%d, fill=%.1f%%\n", strength, percent);
    printf("sqrt=%.2f, sin90=%.2f, power=%.2f\n",
           floatsqroot(81.0), floatsin(90.0, degrees),
           floatpower(2.0, 3.0));

    if (floatround(-1.5) != -1) return 1;
    if (floatint(-1.75) != -1) return 2;
    return 0;
}
```

Nonfinite arithmetic results, invalid domains, and out-of-range integer
conversions terminate with an AMX error. They do not return a `cc_last_error`
diagnostic. Validate denominators/ranges beforehand.

Across the CC bridge, signed-32-bit integral numbers arrive as `CC_RESULT_INT`;
other representable numbers arrive as Float32. NaN, infinity, Float32 overflow,
and nonzero values underflowing completely to zero are rejected. Representable
subnormal values such as `1e-40` survive, but precision is still Float32.
Store exact large identifiers and precision-sensitive decimals as strings.

## 8. Redstone

Sides are relative to the computer: `"front"`, `"back"`, `"left"`, `"right"`,
`"top"`, and `"bottom"`. Digital reads are boolean; analogue strength is 0–15.
Digital true drives strength 15. See the underlying
[CC redstone reference](https://tweaked.cc/module/redstone.html).

| Function | Return / behavior |
| --- | --- |
| `bool:cc_redstone_get_input(const side[])` | Input high/low; also false on API error. |
| `cc_redstone_set_output(const side[], bool:value)` | 0 for successful CC setter, −1 on error. |
| `cc_redstone_get_analog_input(const side[])` | Input strength 0–15; also 0 on API error. |
| `cc_redstone_set_analog_output(const side[], value)` | Set 0–15; 0 on success, −1 on error. |
| `bool:cc_redstone_get_output(const side[])` | Current configured output; also false on API error. |
| `cc_redstone_get_analog_output(const side[])` | Current output strength; −1 on API error. |

To distinguish input false/zero from failure, call `redstone.getInput` or
`redstone.getAnalogInput` through `cc_call` and inspect the result.

### 8.1 Edge-triggered two-input controller

This starts `back` **on** and toggles it whenever the OR of `right` and `left`
changes from low to high. Both inputs must become low before another rising
edge. An input already high at startup establishes the initial state and does
not immediately toggle. This is edge detection, not contact-bounce filtering.

**Hardware effect:** this program drives a real redstone output. Adjust the
sides and starting state before using it on machinery.

```pawn
// example: redstone_toggle.pwn
#include <computercraft>

#define INPUT_1 "right"
#define INPUT_2 "left"
#define OUTPUT  "back"

fail_cc(const operation[])
{
    new error[512];
    cc_last_error(error);
    printf("%s: %s\n", operation, error);
    return 1;
}

sample_inputs(&combined)
{
    if (cc_call("redstone", "getInput", "s", INPUT_1) != 1) return -1;
    new bool:first = cc_result_bool(0);
    if (cc_call("redstone", "getInput", "s", INPUT_2) != 1) return -1;
    new bool:second = cc_result_bool(0);
    combined = (first || second) ? 1 : 0;
    return 0;
}

main()
{
    new bool:enabled = true;
    new previous;
    if (sample_inputs(previous) < 0) return fail_cc("Initial read");
    if (cc_redstone_set_output(OUTPUT, enabled) < 0)
        return fail_cc("Initial output");

    for (;;)
    {
        if (cc_pull_event("redstone") < 1) return fail_cc("Wait");
        new current;
        if (sample_inputs(current) < 0) return fail_cc("Input read");
        if (current && !previous)
        {
            enabled = !enabled;
            if (cc_redstone_set_output(OUTPUT, enabled) < 0)
                return fail_cc("Output");
            printf("Output enabled=%d\n", enabled);
        }
        previous = current;
    }
}
```

There is no recursive call to `main` or a controller function. The loop stores
state explicitly. Very short or multiple transitions may be missed while
processing: CC events do not provide a time-stamped sample of every input edge.

### 8.2 Analogue and bundled signals

Use `clamp(value, 0, 15)` before setting an analogue output.
Bundled cable methods, where the installed mods support them, use the generic
bridge:

```pawn
// example: analog_and_bundled.pwn
#include <computercraft>

main()
{
    if (cc_redstone_set_analog_output("back", clamp(12, 0, 15)) < 0)
        return 1;
    if (cc_call("redstone", "getBundledInput", "s", "right") != 1)
        return 2;
    new mask = cc_result_int(0);
    printf("Bundled bit mask: %d\n", mask);
    return 0;
}
```

## 9. Events

Events let a program wait without repeatedly polling or blocking the Minecraft
server thread. The event snapshot is separate from the generic result buffer.

### 9.1 Event API

In signatures below, `filter = empty` means omit the argument or pass `""`.

| Function | Return / behavior |
| --- | --- |
| `cc_pull_event(const filter[] = empty)` | Wait for a matching event; return value count including its name, or −1. |
| `cc_event_count()` | Snapshot value count, initially 0; −1 on API error. |
| `cc_event_type(index)` | `CC_RESULT_*` type; −1 for invalid index/API error. |
| `cc_event_string(index, destination[], size = sizeof destination)` | Copy string/JSON as unpacked text; original byte length or −1. |
| `cc_event_name(destination[], size = sizeof destination)` | Copy event value 0; same semantics as `cc_event_string`. |
| `cc_event_int(index)` | Integer/bool value; 0 on access error. |
| `bool:cc_event_bool(index)` | Boolean/integer value; false on access error. |
| `Float:cc_event_float(index)` | Float or converted integer/bool; 0.0 on access error. |

Index 0 is the event name; indices 1 onward are its arguments.
The snapshot survives printing, ordinary CC calls, and sleeps until replaced
by another event pull. **Event getters still replace generic `cc_result_*`
storage**, so do not interleave them with reading an unrelated result.

All values are serialized on pulling an event. Events carrying file handles,
functions, or other non-JSON-compatible values may fail even if you only wanted
their name. Maximum: 16 values including the name; an oversized event clears
the snapshot and fails.

### 9.2 Common event layouts

| Event name | Arguments after index 0 |
| --- | --- |
| `redstone` | None. Read the actual redstone inputs afterward. |
| `timer` | 1: timer ID |
| `char` | 1: typed text string |
| `key` | 1: key code; 2: held/repeated boolean |
| `key_up` | 1: key code |
| `paste` | 1: pasted text |
| `peripheral` / `peripheral_detach` | 1: peripheral name |
| `monitor_touch` | 1: monitor name; 2: x; 3: y |
| `monitor_resize` | 1: monitor name |
| `term_resize` | None. Query terminal size again. |
| `modem_message` | 1: receiving modem name; 2: channel; 3: reply channel; 4: payload; 5: distance or nil |
| `rednet_message` | 1: sender computer ID; 2: message; 3: protocol, if supplied |

Use `char` for text and `key` for physical keys. A modem payload may be text,
a number, a boolean, or a JSON-compatible table; check its type. Official
layouts: [key](https://tweaked.cc/event/key.html),
[modem_message](https://tweaked.cc/event/modem_message.html),
[monitor_touch](https://tweaked.cc/event/monitor_touch.html).

### 9.3 Filtering and event loss

**Filtering discards nonmatching events.** It does not save them for a later
wait. `cc_wait_timer`, `cc_sleep`, `delay`, and `cc_yield` also discard unrelated
events while waiting and may consume other one-shot timer notifications.

For several event sources, use **one unfiltered `cc_pull_event()` loop** and
dispatch by name. Do not alternate “wait for redstone” and “wait for timer”
expecting both streams to remain available.

A coroutine buffers up to **128 external events** during native transport waits.
Overflow terminates the program rather than growing without a limit. Internal
`ccpawn_*` events are hidden. Ctrl+T always terminates; do not attempt to use
`pullEventRaw` as a guarantee of ignoring termination.

### 9.4 A timer and keyboard in one loop

This receives one-second timer events while still allowing `q` to stop.
It deliberately has no `sleep` inside the loop.

```pawn
// example: timer_and_keyboard.pwn
#include <computercraft>

main()
{
    new timer = settimer(1000); // Repeating CC timer.
    if (timer < 0) return 1;
    new ticks = 0;
    new event[32], character[16];

    for (;;)
    {
        if (cc_pull_event() < 1) return 2;
        new length = cc_event_name(event);
        if (length < 0 || length >= sizeof event) continue;

        if (strequal(event, "timer"))
        {
            if (cc_event_int(1) == timer)
                printf("Tick %d; type q to quit\n", ++ticks);
        }
        else if (strequal(event, "char"))
        {
            if (cc_event_string(1, character) >= 0
                && strequal(character, "q"))
                break;
        }
    }

    if (cc_cancel_timer(timer) < 0) return 3;
    return 0;
}
```

## 10. Timers, sleeping, and dates

### 10.1 API

Durations in the addon helpers are **milliseconds**, not seconds.

| Function | Return / behavior |
| --- | --- |
| `cc_sleep(milliseconds)` | Wait; 0 on success, −1 on API error. |
| `delay(milliseconds)` | Alias for `cc_sleep`. |
| `cc_yield()` | Wait at least one game tick using an internal timer; 0 or −1. Not a pure zero-duration scheduler yield. |
| `cc_start_timer(milliseconds)` | One-shot timer ID, or −1. |
| `settimer(milliseconds, bool:singleshot = false)` | Repeating by default; stable logical timer ID, or −1. `true` makes it one-shot. |
| `cc_cancel_timer(timer_id)` | 1 canceled, 0 unknown/already consumed, −1 on error. |
| `cc_wait_timer(timer_id)` | Wait for that ID; 0 when received, −1 on error. |
| `gettime(&hour = 0, &minute = 0, &second = 0)` | Real UTC Unix seconds or −1; write UTC hour/minute/second. |
| `getdate(&year = 0, &month = 0, &day = 0)` | UTC day-of-year 1–366 or −1; write calendar year/month/day. |
| `tickcount(&granularity = 0)` | CC computer uptime in signed-wrapping milliseconds; granularity output is 50. |

`if (cc_sleep(1000) < 0)` means “wait one second, then check whether the call
failed.” It does not compare a duration with zero. You must call the function
with parentheses and its duration.

These timers are a deliberate CC adaptation: **`settimer` generates events;
it does not call a PAWN `@timer` callback**. Stop it with `cc_cancel_timer(id)`,
not `settimer(0)`.

Rules and limits:

- Integer durations: 0 through 2,147,483,647; repeating duration must be positive.
- Minimum actual delay is 50 ms. Longer waits follow Minecraft tick scheduling
  and can be extended by server lag; these are not hard real-time timers.
- Maximum 32 outstanding timers, including expired but unread one-shots and
  the internal timer needed by a sleep/yield.
- Repeated notifications coalesce while one tick is pending. A blocked program
  is not guaranteed to receive every missed tick.
- A positive unknown, canceled, or already-consumed timer ID can make
  `cc_wait_timer` wait indefinitely. A negative ID fails immediately.
- Canceling removes managed pending notifications, but a physical timer event
  already queued inside CC can still arrive. Check active state and expected ID.
- Managed timers are canceled at program end. They do not keep `main` alive.
- Prefer these timer helpers consistently; generic `os.startTimer` uses
  **seconds** and bypasses the managed timer bookkeeping.

### 10.2 Three-minute countdown

This is a simple game-tick countdown. It intentionally waits one second after
each print; printing/processing and lag can make the real duration longer than
three minutes. Use elapsed-time measurements if that distinction matters.

```pawn
// example: countdown.pwn
#include <computercraft>

main()
{
    new remaining = 180;
    while (remaining > 0)
    {
        if (printf("Remaining: %d:%02d\n", remaining / 60, remaining % 60) < 0)
            return 1;
        if (cc_sleep(1000) < 0)
            return 2;
        remaining--;
    }
    cc_println("Countdown finished!");
    return 0;
}
```

For a one-shot “wait three minutes” task, start `cc_start_timer(180000)`, check
that the ID is nonnegative, then call `cc_wait_timer(id)`. For a countdown which
must also handle redstone or modem events, adapt the unfiltered loop above.

### 10.3 UTC versus computer uptime

`gettime` and `getdate` read real **UTC**, not local timezone or Minecraft's
day/night clock. Their valid timestamp range ends at
**2038-01-19 03:14:07 UTC**; both fail outside Unix seconds 0..2,147,483,647.
They sample separately, so reading both across midnight is not atomic.

`tickcount` is CC computer uptime, not CPU time or UTC. It becomes negative at
2³¹ ms and wraps at 2³² ms. A negative value, including −1, is not by itself
proof of failure. Do not compare wrapped absolute deadlines naively.

```pawn
// example: utc_clock.pwn
#include <computercraft>

main()
{
    new hour, minute, second, year, month, day;
    new timestamp = gettime(hour, minute, second);
    if (timestamp < 0) return 1;
    new day_of_year = getdate(year, month, day);
    if (day_of_year < 0) return 2;

    printf("UTC time: %02d:%02d:%02d (Unix %d)\n",
           hour, minute, second, timestamp);
    printf("UTC date: %04d-%02d-%02d (day %d)\n",
           year, month, day, day_of_year);
    return 0;
}
```

No host-clock mutation functions (`settime`, `setdate`, `settimestamp`) are exposed.

## 11. Peripherals

Adjacent peripherals use side names; a monitor above the computer is normally
`"top"`. Wired networks can expose names such as `"monitor_0"`. A networked
device must actually be connected/exposed. Discover names rather than assuming
a machine is attached simply because it is nearby.
See [CC peripheral discovery](https://tweaked.cc/module/peripheral.html).

### 11.1 Complete helper API

| Function | Return / behavior |
| --- | --- |
| `bool:cc_peripheral_is_present(const name[])` | Whether attached; false also on API error. |
| `bool:cc_peripheral_has_type(const name[], const type[])` | Whether it has the requested type; false also on API error. |
| `cc_peripheral_get_type(const name[], destination[], size = sizeof destination)` | Copy the first type; original string length or −1. |
| `cc_peripheral_find(const type[], destination[], size = sizeof destination)` | First matching name in **sorted name order**; original length >0, 0 for no match, −1 on error. |
| `cc_peripheral_get_names()` | Return-value count or −1; name list is JSON in result 0. |
| `cc_peripheral_get_methods(const name[])` | Return-value count or −1; method list is JSON in result 0. |
| `cc_peripheral_call(const name[], const method[], const format[], {Float,_}:...)` | Invoke a method; result count or −1. Read `cc_result_*` afterward. |

The call formats are the same `i/b/f/s/j` codes as `cc_call`, but there are
at most **14 method arguments** because the wrapper also transports the device
and method names.

A device may have several types. `get_type` returns only the first; use
`has_type` to test a particular capability. The generic `peripheral.getType`
call can return multiple values if you need them.

`cc_peripheral_find` returns a **name string**, not a wrapped Lua object.
Check for name truncation before using the buffer. You cannot write
`device.someMethod()` in PAWN.

Method errors have two layers: the bridge may fail, or the method may
successfully return `false` / `nil, error`. Check the method's documented
results in addition to `cc_peripheral_call >= 0`. `cc_last_error` only describes
the bridge/API failure, not necessarily a method-returned diagnostic.

### 11.2 Discovery example

```pawn
// example: peripheral_discovery.pwn
#include <computercraft>
#pragma dynamic 16384

main()
{
    new names[4096], methods[4096], device[128];
    if (cc_peripheral_get_names() != 1) return 1;
    new length = cc_result_string(0, names);
    if (length < 0 || length >= sizeof names) return 2;
    printf("Attached devices: %s\n", names);

    length = cc_peripheral_find("monitor", device);
    if (length < 0 || length >= sizeof device) return 3;
    if (length == 0)
    {
        cc_println("No monitor attached");
        return 0;
    }

    if (cc_peripheral_get_methods(device) != 1) return 4;
    length = cc_result_string(0, methods);
    if (length < 0 || length >= sizeof methods) return 5;
    printf("%s methods: %s\n", device, methods);
    return 0;
}
```

Large local arrays consume the AMX stack. This example increases the combined
stack/heap allowance with `#pragma dynamic 16384`; it does not change the
overall 1 MiB VM cap.

## 12. Monitors and larger terminals

### 12.1 Redirect an existing PAWN program

In **CraftOS**, with a monitor attached above the computer:

```text
monitor scale top 0.5
monitor top pawn hello.amx
```

This runs the program with its terminal redirected. `printf`, `print`,
`cc_println`, and `cc_call("term", ...)` then target that monitor.
For the normal CraftOS editor on a monitor, use `monitor top edit hello.pwn`;
keyboard input still comes from the computer interface.

A monitor supports scale values **0.5–5 in 0.5 increments**; smaller text gives
more character cells. Build a larger connected monitor for more display area.
Only advanced monitors provide touch events and color. These are normal
[CC monitor capabilities](https://tweaked.cc/peripheral/monitor.html).

The computer GUI itself is not resized by a PAWN function. Its terminal
dimensions are operator-controlled CC configuration. Prefer `term.getSize`
instead of hard-coding the dimensions of somebody else's server.

Do not call `term.redirect` with a JSON object: a real redirect contains Lua
functions, which cannot be represented by the PAWN bridge.

### 12.2 Write to a monitor without redirecting

This needs an attached monitor. It writes to that peripheral while terminal
debug output remains on the computer.

```pawn
// example: monitor_status.pwn
#include <computercraft>

main()
{
    new monitor[128];
    new length = cc_peripheral_find("monitor", monitor);
    if (length <= 0 || length >= sizeof monitor)
    {
        cc_println("Attach a monitor first");
        return 1;
    }

    if (cc_peripheral_call(monitor, "setTextScale", "f", 0.5) < 0) return 2;
    if (cc_peripheral_call(monitor, "clear", "") < 0) return 3;
    if (cc_peripheral_call(monitor, "setCursorPos", "ii", 1, 1) < 0) return 4;
    if (cc_peripheral_call(monitor, "write", "s", "PAWN controller ready") < 0)
        return 5;

    if (cc_peripheral_call(monitor, "getSize", "") != 2) return 6;
    new width = cc_result_int(0);
    new height = cc_result_int(1);
    printf("Monitor %s: %d x %d\n", monitor, width, height);
    return 0;
}
```

Monitor/`term.write` is a low-level single-line write, unlike the addon’s
newline-aware `print`/`printf`. Position rows explicitly for a dashboard or use
the CraftOS redirect.

Terminal colors are integer bit values, e.g. white=1, red=16384, black=32768.
`colors` constants are not PAWN identifiers. You can define your own constants
and pass them to `term.setTextColor`/`setBackgroundColor` if color is supported.

## 13. Files and persistent configuration

These are **CC virtual filesystem** operations, not server host-file access.

| Function | Return / behavior |
| --- | --- |
| `bool:cc_fs_exists(const path[])` | Exists; false also on API error. |
| `cc_fs_read(const path[], destination[], size)` | Read entire file and copy unpacked text; return original source length. API/read failure returns **0**; destination-copy failure returns −1. |
| `bool:cc_fs_write(const path[], const contents[], bool:append = false)` | Write text; true on success, false on error. Default replaces contents; `true` appends. |

Unlike many wrappers, **`cc_fs_read` requires its size argument**.
A 0 result can mean an empty file or a read error. Read `cc_last_error`
immediately to distinguish ordinary file/API failures; a failed read need not
update the buffer. Some malformed/oversized transport failures can currently
lose their diagnostic, so an empty diagnostic is not universal proof of success.
For externally supplied files, first check `fs.getSize` through `cc_call`,
reject files exceeding your buffer/application limit, and validate the contents.
Do not silently treat an unexpected empty read as valid configuration.

All normal CC read-only mounts, capacity restrictions, and path rules apply.
Use absolute CC paths for application files: generic `fs` operations are not
automatically resolved against the CraftOS shell's current directory.
The command wrappers `pawn`/`pawncc` do resolve their own file arguments.

These text helpers cannot preserve embedded NUL bytes. They are unsuitable
for arbitrary binary files or copying an `.amx`. Keep configuration text small;
there is no streaming file-handle API. Very large files/results can exceed
transport or buffer limits.

```pawn
// example: text_file.pwn
#include <computercraft>

main()
{
    // This replaces only this example's own file.
    if (!cc_fs_write("/pawn-demo.txt", "first line\n")) return 1;
    if (!cc_fs_write("/pawn-demo.txt", "second line\n", true)) return 2;

    new text[512], error[512];
    new length = cc_fs_read("/pawn-demo.txt", text, sizeof text);
    new error_length = cc_last_error(error); // Before printf.
    if (length < 0 || length >= sizeof text || error_length > 0)
    {
        printf("Read failed or did not fit: %s\n", error);
        return 3;
    }
    printf("%s", text);
    return 0;
}
```

`fs.list`, `fs.makeDir`, `fs.getSize`, and other value-based functions are
available through `cc_call`. Do not request an `fs.open` handle.
Deletion/move operations are also real operations on the computer's files:
do not use them casually in generated code.

File writes are not a transactional database and overwrites are not guaranteed
crash-atomic. For important configuration, retain a previous copy and validate
new data before replacing the active file.

## 14. JSON

JSON helpers store bounded documents on the Lua side of **one program run**.
A positive integer handle identifies a document; it is not a Lua object pointer.

### 14.1 Complete JSON API

In signatures, `pointer = empty` means omit it or pass `""` for the root.

| Function | Return / behavior |
| --- | --- |
| `cc_json_parse(const text[])` | Positive handle; 0 on parse/limit failure. |
| `cc_json_object()` | New `{}` document; positive handle or 0. |
| `cc_json_array()` | New `[]` document; positive handle or 0. |
| `cc_json_stringify(handle, destination[], size = sizeof destination)` | Complete JSON byte length or −1; insufficient space including NUL is an error, **not truncation**. |
| `cc_json_get(handle, const pointer[] = empty)` | Return count 1 or −1; read `cc_result_*` immediately. |
| `cc_json_type(handle, const pointer[] = empty)` | `CC_JSON_*` type or −1. |
| `bool:cc_json_set_string(handle, const pointer[], const value[])` | Set a string; true/false. |
| `bool:cc_json_set_int(handle, const pointer[], value)` | Set an integer; true/false. |
| `bool:cc_json_set_float(handle, const pointer[], Float:value)` | Set a Float value; true/false. |
| `bool:cc_json_set_bool(handle, const pointer[], bool:value)` | Set a boolean; true/false. |
| `bool:cc_json_set_null(handle, const pointer[])` | Set JSON null; true/false. |
| `bool:cc_json_set_json(handle, const pointer[], const text[])` | Parse and insert/replace a JSON subtree; true/false. |
| `bool:cc_json_free(handle)` | Release a document; true/false. Double-free is an error. |

On failure, copy `cc_last_error` before any other CC operation.
Setters are atomic at document level: failed updates keep the previous document.
`set_null` does not delete a field. There is no dedicated remove/array-length/
object-key-iteration helper in this version.

| JSON constant | Value |
| --- | --- |
| `CC_JSON_NULL` | 0 |
| `CC_JSON_BOOL` | 1 |
| `CC_JSON_NUMBER` | 2 |
| `CC_JSON_STRING` | 3 |
| `CC_JSON_ARRAY` | 4 |
| `CC_JSON_OBJECT` | 5 |

**`CC_JSON_*` and `CC_RESULT_*` are different namespaces with different
meanings.** Use `cc_json_type` to inspect the document, and `cc_result_type`
to inspect a value returned across the bridge.

`cc_json_get` behavior:

- JSON null: one result with `CC_RESULT_NIL`.
- Missing path: −1, not null.
- String/bool/number: the corresponding bridge value.
- Object/array: serialized JSON **text with `CC_RESULT_STRING`**, not
  `CC_RESULT_JSON` and not a new handle.
- To create an independent document from that subtree, copy the string first,
  then call `cc_json_parse` on it.
- Calling `cc_json_type` after `cc_json_get` overwrites the generic result
  buffer! Either check type before get or consume get's result immediately.

### 14.2 JSON Pointer paths

| Pointer | Meaning |
| --- | --- |
| `""` | Whole document/root |
| `"/machine/active"` | Nested object field |
| `"/items/0"` | First array element; **zero-based** |
| `"/items/-"` | Append to array; setter only |
| `"/"` | Object field whose name is the empty string |
| `"/a~1b"` | Object field named `a/b` |
| `"/~0key"` | Object field named `~key` |

An array setter at its current length also appends. Negative indices, leading
zeroes such as `01`, and sparse-array insertion are rejected.
Intermediate parents must exist: first create `"/machine"` as `{}` before
setting `"/machine/active"`. The root can be replaced with a scalar or container.
Primitive documents such as `false`, `42`, and `null` are valid.

### 14.3 Build, inspect, and modify a document

```pawn
// example: json_document.pwn
#include <computercraft>

main()
{
    new doc = cc_json_object();
    if (!doc) return 1;
    if (!cc_json_set_string(doc, "/name", "Harvester")) return 2;
    if (!cc_json_set_json(doc, "/machine", "{}")) return 3;
    if (!cc_json_set_bool(doc, "/machine/active", true)) return 4;
    if (!cc_json_set_json(doc, "/items", "[]")) return 5;
    if (!cc_json_set_json(doc, "/items/-", "{\"count\":12}")) return 6;

    if (cc_json_type(doc, "/items/0/count") != CC_JSON_NUMBER) return 7;
    if (cc_json_get(doc, "/items/0/count") != 1) return 8;
    if (cc_result_type(0) != CC_RESULT_INT) return 9;
    new count = cc_result_int(0); // Store before another JSON/print call.

    if (!cc_json_set_int(doc, "/items/0/count", count + 1)) return 10;
    new output[512];
    if (cc_json_stringify(doc, output) < 0) return 11;
    printf("%s\n", output);
    if (!cc_json_free(doc)) return 12;
    return 0;
}
```

Documents are cleaned up when the program exits, including error exits. In a
long-running program, free documents explicitly after use so repeated operations
do not exhaust the live-handle limit.

### 14.4 JSON configuration file

This creates `/pawn-controller.json` if missing, validates it, prints its
values, and writes the validated document back. It does **not** energize
redstone. The example uses strict result types instead of permissive `strval`.

```pawn
// example: json_configuration.pwn
#include <computercraft>
#pragma dynamic 8192

main()
{
    new text[2048], error[512];
    new doc;
    if (cc_call("fs", "exists", "s", "/pawn-controller.json") != 1)
        return 1;
    new bool:exists = cc_result_bool(0);

    if (exists)
    {
        new length = cc_fs_read("/pawn-controller.json", text, sizeof text);
        new error_length = cc_last_error(error);
        if (length < 0 || length >= sizeof text || error_length > 0)
        {
            printf("Cannot read configuration: %s\n", error);
            return 2;
        }
        doc = cc_json_parse(text);
    }
    else
    {
        doc = cc_json_parse("{\"enabled\":true,\"interval_ms\":1000}");
    }
    if (!doc) return 3;

    if (cc_json_type(doc, "/enabled") != CC_JSON_BOOL) return 4;
    if (cc_json_get(doc, "/enabled") != 1) return 5;
    new bool:enabled = cc_result_bool(0);

    if (cc_json_get(doc, "/interval_ms") != 1) return 6;
    if (cc_result_type(0) != CC_RESULT_INT) return 7;
    new interval_ms = cc_result_int(0);
    if (interval_ms < 50 || interval_ms > 3600000) return 8;

    printf("enabled=%d, interval=%d ms\n", enabled, interval_ms);
    if (cc_json_stringify(doc, text) < 0) return 9;
    if (!cc_fs_write("/pawn-controller.json", text)) return 10;
    if (!cc_json_free(doc)) return 11;
    return 0;
}
```

### 14.5 A peripheral table to JSON

Example prerequisite: an attached CC generic inventory containing an item in
slot 1. Peripheral slot indices are **1-based**; JSON Pointer array indices
remain **0-based**. This example is read-only.

```pawn
// example: inventory_item.pwn
#include <computercraft>
#pragma dynamic 8192

main()
{
    new inventory[128], json[4096], item_name[256];
    new length = cc_peripheral_find("inventory", inventory);
    if (length <= 0 || length >= sizeof inventory) return 1;

    if (cc_peripheral_call(inventory, "getItemDetail", "i", 1) != 1)
        return 2;
    if (cc_result_type(0) == CC_RESULT_NIL)
    {
        cc_println("Slot 1 is empty");
        return 0;
    }
    if (cc_result_type(0) != CC_RESULT_JSON) return 3;
    length = cc_result_string(0, json);
    if (length < 0 || length >= sizeof json) return 4;

    new doc = cc_json_parse(json);
    if (!doc) return 5;
    if (cc_json_get(doc, "/name") != 1) return 6;
    if (cc_result_type(0) != CC_RESULT_STRING) return 7;
    length = cc_result_string(0, item_name);
    if (length < 0 || length >= sizeof item_name) return 8;

    if (cc_json_get(doc, "/count") != 1) return 9;
    if (cc_result_type(0) != CC_RESULT_INT) return 10;
    new count = cc_result_int(0);
    if (!cc_json_free(doc)) return 11;
    printf("Slot 1: %d x %s\n", count, item_name);
    return 0;
}
```

Do not assume every Lua table has a lossless JSON mapping. For example,
`inventory.list` uses a sparse numeric-keyed table; prefer per-slot
`getItemDetail` when a direct JSON mapping is uncertain.
See the underlying [inventory API](https://tweaked.cc/generic_peripheral/inventory.html).

### 14.6 JSON limits and precision

- At most **32 live documents** per program; IDs are not reused within a run.
- Input JSON and serialized output each at most **16 KiB**.
- At most **32 nesting levels** and **2048 values**, including containers/root.
- NUL bytes, NaN, and infinities are rejected.
- No preservation of whitespace, object key order, or original numeric spelling.
- Dedicated document helpers serialize strings as UTF-8. BMP text round trips
  are tested; this is not a new complete Unicode/JSON conformance parser.
  The generic Lua-table bridge uses its own CC serializer defaults, so do not
  assume identical UTF-8 handling for every third-party table.
- Document numbers are Lua numbers, not arbitrary-precision decimals.
  Numeric retrieval must fit a PAWN integer or Float32. A document can contain
  `1e100` and serialize it as JSON text, but cannot return it as a PAWN number.
- Store exact large IDs and precision-sensitive decimal values as strings.

JSON handles are per-run only. They cannot be saved to a file and reused after
a restart; save the serialized JSON instead.

## 15. Modems, rednet, turtles, and chat

These are examples of using the generic bridge, not additional native helper
families. The attached hardware/mod version determines which methods exist.

### 15.1 Raw modem sender

Prerequisite: an attached modem and another computer listening on channel 42.
The example emits a network message.

```pawn
// example: modem_sender.pwn
#include <computercraft>

main()
{
    new modem[128];
    new length = cc_peripheral_find("modem", modem);
    if (length <= 0 || length >= sizeof modem) return 1;
    if (cc_peripheral_call(modem, "transmit", "iis",
                           42, 43, "Hello from PAWN") < 0)
        return 2;
    return 0;
}
```

Channels are 0–65535. Transmitting does not itself open a receive channel.
The reply channel is supplied by the sender. These are the normal
[CC modem methods](https://tweaked.cc/peripheral/modem.html), not authentication:
do not grant machine-control privileges to arbitrary incoming text.

### 15.2 Raw modem receiver

Run on a different reachable computer with a suitable modem. This receives one
text message on channel 42 and closes its channel on a normal exit.

```pawn
// example: modem_receiver.pwn
#include <computercraft>

main()
{
    new modem[128], message[512], receiving_modem[128];
    new length = cc_peripheral_find("modem", modem);
    if (length <= 0 || length >= sizeof modem) return 1;
    if (cc_peripheral_call(modem, "open", "i", 42) < 0) return 2;

    for (;;)
    {
        if (cc_pull_event("modem_message") < 5) return 3;
        length = cc_event_string(1, receiving_modem);
        if (length < 0 || length >= sizeof receiving_modem) continue;
        if (!strequal(receiving_modem, modem)) continue;
        if (cc_event_int(2) != 42) continue;
        if (cc_event_type(4) != CC_RESULT_STRING) continue;

        length = cc_event_string(4, message);
        if (length < 0 || length >= sizeof message) continue;
        new reply_channel = cc_event_int(3);
        printf("Message: %s (reply channel %d)\n", message, reply_channel);
        break;
    }

    if (cc_peripheral_call(modem, "close", "i", 42) < 0) return 4;
    return 0;
}
```

A table payload has `CC_RESULT_JSON`; copy it with `cc_event_string` and parse
it using `cc_json_parse` if appropriate. Distance can be nil: do not assume
`cc_event_float(5) == 0.0` means the sender is physically at the same position.
Modem channels, rednet registrations, and peripheral state are not automatically
restored by PAWN's timer/document cleanup.

### 15.3 Rednet

Rednet is CC's higher-level messaging layer. Use a modem and a protocol string.
This example broadcasts a message, waits up to five seconds for a reply on that
protocol, then closes the modem for rednet.

```pawn
// example: rednet_exchange.pwn
#include <computercraft>

main()
{
    new modem[128], reply[256];
    new length = cc_peripheral_find("modem", modem);
    if (length <= 0 || length >= sizeof modem) return 1;
    if (cc_call("rednet", "open", "s", modem) < 0) return 2;
    if (cc_call("rednet", "broadcast", "ss", "Hello", "pawn-demo") < 0)
        return 3;

    new count = cc_call("rednet", "receive", "sf", "pawn-demo", 5.0);
    if (count < 0) return 4;
    if (count >= 2 && cc_result_type(0) == CC_RESULT_INT
                   && cc_result_type(1) == CC_RESULT_STRING)
    {
        new sender = cc_result_int(0);
        length = cc_result_string(1, reply);
        if (length < 0 || length >= sizeof reply) return 5;
        printf("Reply from %d: %s\n", sender, reply);
    }
    else
    {
        cc_println("No text reply received");
    }

    if (cc_call("rednet", "close", "s", modem) < 0) return 6;
    return 0;
}
```

`rednet.receive` timeout is **seconds**, following its
[Lua API](https://tweaked.cc/module/rednet.html), not the milliseconds of
`cc_sleep`. An ordinary timeout is a successful call returning nil, not
necessarily `cc_call == -1`. For mixed event sources, use the unfiltered event
loop and `rednet_message` rather than a separate blocking receive.

### 15.4 Turtles and other APIs

A turtle method commonly returns a success boolean and possibly a diagnostic.
Always distinguish bridge success from operation success. Movement/digging
methods change the world; inspect fuel, inventory, direction, and permissions
before using them.

Here is a read-only fuel query, including the unlimited-fuel string case:

```pawn
// example: turtle_fuel.pwn
#include <computercraft>

main()
{
    if (cc_call("turtle", "getFuelLevel", "") != 1)
    {
        cc_println("Fuel query failed; this may not be a turtle");
        return 1;
    }
    if (cc_result_type(0) == CC_RESULT_INT)
    {
        new fuel = cc_result_int(0);
        printf("Fuel: %d\n", fuel);
    }
    else if (cc_result_type(0) == CC_RESULT_STRING)
    {
        new text[64];
        new length = cc_result_string(0, text);
        if (length < 0 || length >= sizeof text) return 2;
        printf("Fuel: %s\n", text);
    }
    else return 3;
    return 0;
}
```

Other bridge patterns:

| Need | Call shape | Read immediately afterward |
| --- | --- | --- |
| Computer ID | `cc_call("os", "getComputerID", "")` | Integer result 0 |
| Terminal size | `cc_call("term", "getSize", "")` | Width 0, height 1 |
| Set cursor | `cc_call("term", "setCursorPos", "ii", x, y)` | Usually no values |
| List files | `cc_call("fs", "list", "s", "/")` | JSON text result 0 |
| Read CC setting | `cc_call("settings", "get", "s", "application.key")` | Type-dependent value or nil |
| GPS location | `cc_call("gps", "locate", "f", 2.0)` | x/y/z if successful; handle missing fix |
| Turtle move | `cc_call("turtle", "forward", "")` | Boolean result 0, possible diagnostic 1 |
| Queue scalar event | `cc_call("os", "queueEvent", "si", "app_tick", 7)` | No values; receive through the event loop |

These examples are mappings, not permission guarantees. A method can be absent,
fail normally, or return a type requiring explicit handling. CC method names
retain their exact Lua casing, such as `getComputerID`.

### 15.5 Minecraft chat

There is no built-in `cc_chat` or `SendClientMessage` function, and `printf`
only writes to the terminal. A mod exposing a suitable peripheral can provide
chat access; a command computer is a separate, permission-controlled option.

**Optional example:** Advanced Peripherals chat box. Type names and signatures
depend on its version: the
[0.7 documentation](https://docs.advanced-peripherals.de/0.7/peripherals/chat_box/)
uses `chatBox`, while the
[0.8 documentation](https://docs.advanced-peripherals.de/0.8/peripherals/chat_box/)
uses `chat_box`. The one-argument `sendMessage` call avoids optional-argument
differences. Respect that mod's cooldowns, permissions, and configuration.

This sends a real Minecraft chat message if compatible hardware is attached:

```pawn
// example: chat_message.pwn
#include <computercraft>

main()
{
    new device[128], error[512];
    new length = cc_peripheral_find("chat_box", device);
    if (length == 0)
        length = cc_peripheral_find("chatBox", device);
    if (length <= 0 || length >= sizeof device)
    {
        cc_println("No compatible chat box found");
        return 1;
    }

    new count = cc_peripheral_call(device, "sendMessage", "s",
                                  "Controller finished its task.");
    if (count < 0)
    {
        cc_last_error(error);
        printf("Chat API error: %s\n", error);
        return 2;
    }
    if (count < 1 || cc_result_type(0) != CC_RESULT_BOOL
                  || !cc_result_bool(0))
    {
        // A peripheral may return nil/false plus its own error text.
        if (count > 1 && cc_result_type(1) == CC_RESULT_STRING)
            cc_result_string(1, error);
        else
            strunpack(error, "Message was not accepted");
        printf("Chat not sent: %s\n", error);
        return 3;
    }
    return 0;
}
```

The addon does not install Advanced Peripherals or guarantee its presence.
The local integration tests use a simulated test peripheral, not the full
third-party mod. Discover available methods and consult the documentation for
the actual installed version before generating more elaborate chat code.

## 16. Limits, lifecycle, and security

### 16.1 Resource limits

| Resource | Limit / rule in 0.4.0 |
| --- | --- |
| Source text | 256 KiB; valid UTF-8 |
| Compiled AMX / VM allocation | At most 1 MiB, subject to the program's own stack/heap allocation |
| Concurrent PAWN runners | 4 per CC computer |
| Compiler timeout | 10 seconds |
| Pure PAWN execution watchdog | About 2 seconds without a CC call |
| Generic call arguments / results | 16 / 16 |
| Peripheral method arguments | 14 |
| Native parameter vectors | 128 total arguments; this does not increase the CC bridge's 16-argument limit |
| `printf` arguments | Up to 127 format values |
| `strformat` arguments | Up to 124 format values |
| Scanned PAWN strings / formatted output | 16 KiB |
| Native string destination capacities | 1–16,385 cells |
| Protocol line | About 1 MiB including framing/hex encoding; practical payloads must be smaller |
| Decoded returned protocol string | At most 256 KiB internally; public destination and JSON limits are smaller |
| External event queue | 128 entries; overflow terminates |
| Event values | 16 including name |
| Managed outstanding timers | 32 |
| JSON live documents | 32 |
| JSON input / serialized output | 16 KiB each |
| JSON nesting / values | 32 levels / 2048 values |

The VM cap is not a promise of 1 MiB available for every array. Local arrays,
calls, recursion, and heap/stack share the AMX allocation. Use reasonable buffer
sizes and, if necessary, `#pragma dynamic` within the overall cap.

### 16.2 Scheduling and performance

An event wait/sleep yields through CraftOS rather than blocking a Minecraft
thread. Pure string/math natives and result getters do **not** reset a long
CPU-bound loop just because they are function calls. Cooperate with CC.

However, frequent CC operations also have transport cost. Read values once,
cache them in PAWN variables, avoid printing every game tick, and process
events with bounded work. Do not use recursion as an endless control loop.

`cc_yield` is not suitable for preserving unrelated queued events: like sleep,
it consumes events while waiting for its own timer. Prefer bounded work between
unfiltered pulls in event-driven applications.

### 16.3 Restart and chunk lifecycle

Program memory, JSON handles, timers, and event snapshots do not survive a
restart. Persist necessary state in the CC filesystem and reload it on startup.
A timer is not a persistent alarm service.

This addon is **not a chunk loader**. Whether a computer continues running with
no players depends on the world, loaded chunks, server pause behavior, and
other mods' permissions/configuration. No PAWN helper overrides those controls.
Unloading or switching off a computer closes its processes.

An unexpected native-runner exit while waiting on a CC call is detected and
reported, rather than leaving that wait hanging indefinitely.

### 16.4 Security boundary

PAWN's compiler and VM run in separate native processes, but they are
**not an operating-system sandbox**. Process separation helps contain ordinary
VM crashes; it is not sufficient isolation for arbitrary untrusted bytecode.

No PAWN host filesystem, process execution, sockets, dynamic-library loading,
or host-console natives are registered. Exposed world/file/peripheral actions
still have real effects within CC permissions. Compiler/VM environments are
cleared, and size/time limits are applied, but this is not a complete security
audit or a guarantee against native-code vulnerabilities.

Operators should treat in-game PAWN compilation as a trusted-player feature.
Keep backups and normal container/host resource restrictions. Do not weaken
container isolation or grant broad host privileges to work around compiler or
runner execution errors.

Validate text and network data before using them as settings or commands.
Do not treat a modem channel or claimed sender name as authorization.

## 17. Troubleshooting and migration

| Symptom | Check / explanation |
| --- | --- |
| `pawncc` or `pawn` not found | Addon installed on the server? Server restarted? Wrong CC instance or conflicting ROM overrides? |
| Undefined symbol for a documented helper | Keep all components at the same version: JAR, ROM dispatcher, compiler includes, and native runner. |
| Native not found when running AMX | Bytecode references unsupported natives or was built for another PAWN dialect/ABI. Recompile with the bundled compiler. |
| `enum` or numeric array syntax fails | Use PAWN 4.1 `const Tag: { ... };` and `[1, 2, 3]` initializers. |
| Tag mismatch around Float | Use Float literals or `float(value)`; do not reinterpret integer bits with `Float:value`. |
| Warning 206 on `while (true)` | Constant-condition warning, not necessarily a failure. `for (;;)` avoids that pattern. |
| Warning 229 around strings/arrays | Packed/unpacked mismatch; use the appropriate representation and indexing. |
| “invalid or unsafe #include directive” | Use one allowed include name, on a clean line without trailing comments. |
| Setter returns 0 | Often successful: bridge return is the result count, not a boolean. |
| Input false/0 seems indistinguishable from error | Use checked generic call and type inspection, or immediately inspect diagnostic. |
| Result disappears after a debug print | `printf` itself is a CC operation; copy results first. |
| String silently shortened | Check the correct family’s length convention and destination capacity. |
| `strformat` returns 1, not the text length | By design; it can return 1 even after truncating. |
| Output has no line break | `print`/`printf` do not append one; use `\n` or `cc_println`. |
| Monitor above the computer stays blank | It is normally named `top`, but output is not automatically redirected. Use CraftOS `monitor` or direct peripheral calls. |
| Countdown is slower than wall time | Tick scheduling, lag, and processing/printing overhead. |
| Other events disappear during a delay | Filtered waits/sleeps discard them. Use one unfiltered event loop. |
| Wait on timer never returns | ID canceled, already consumed, or never created? Was another filtered wait consuming timer events? |
| JSON handle limit reached | Free documents in long-running loops. |
| JSON null confused with absent field | Null is one nil result; an absent path is an API error. |
| Large number changes value | Signed-32-bit / Float32 precision limits. Preserve exact large values as strings. |
| Filesystem read returns 0 | Could be empty data or an error; capture `cc_last_error` immediately. |
| AMX stack/heap collision | Reduce buffers/recursion or review `#pragma dynamic` within the VM cap. |
| Watchdog stops loop | Pure PAWN work is too long without a cooperating CC call. |
| `GLIBC_2.38 not found` / permission denied | Container libc too old or executable extraction blocked; fix the platform setup, not program syntax. |
| Works until nobody is online | Verify chunk loading/server pause policy separately from PAWN. |

Version notes:

- **0.1.0:** had a scalar-vararg bug; setting redstone to false could behave
  incorrectly. Do not use it for these controller examples.
- **0.1.1:** corrected scalar passing.
- **0.3.0:** introduced the standard helpers, formatting, Float library, event snapshots,
  managed timers/time functions, peripheral helpers, JSON, and bridge hardening
  described here.
- **0.4.0:** renamed the project to PawnCraft and removed the separate in-game
  Java runtime. Java 21 remains the Minecraft/addon runtime requirement.

When upgrading, remove the previous `ccpawn-*.jar` from `mods/` before installing
`pawncraft-0.4.0.jar`; do not install both. The internal mod ID `ccpawn`, PAWN
commands, include names, native API names, and resource paths remain unchanged.
Stop the server and back up the instance before replacing the JAR, then restart.

Recompile source with the installed 0.4.0 toolchain for reproducible behavior.
Older bytecode using the addon's original natives is intended to remain
compatible, but bytecode from another host/library declaration is not guaranteed.
In particular, `valstr` uses this addon's extra capacity parameter.

### Functions not provided

Do not generate calls to unsupported functions merely because another PAWN host
provides them. Examples **not available here**:

- `format`, `sscanf`, SA-MP/open.mp player APIs or callbacks, `SendClientMessage`.
- `strmid`, `strins`, `strdel`, `memcpy`, `uuencode`, `uudecode`.
- `heapspace`, `funcidx`, `numargs`, `getarg`, `setarg`, `swapchars`,
  `getproperty`, `setproperty`, `deleteproperty`, `existproperty`.
- Host console functions such as `getchar`, `getstring`, `getvalue`, `console`,
  `clrscr`, `clreol`, `gotoxy`, `wherexy`, `setattr`, and `@keypressed`.
- Host file/process/network functions, `settime`, `setdate`, `settimestamp`.
- Automatic `@timer` callbacks, a `SetTimer` API from other hosts, and `Fixed:`
  arithmetic support.

For keyboard input, use events. For terminal control, use `term`. For files,
use CC helpers. For text formatting, use `strformat`. For floating point, use
`Float:`.

## 18. Code-generation checklist

When giving this file to a coding assistant, specify the physical setup and
desired behavior, then require code targeting **PawnCraft 0.4.0 / PAWN 4.1.7222**.

A useful prompt:

> Generate one complete PAWN program for PawnCraft 0.4.0, using this guide as the
> API contract. Use only the bundled functions and verified CC/peripheral
> methods. State hardware assumptions, name the source file, and give the
> CraftOS edit/compile/run commands. Check errors and buffer sizes. Do not
> invent native functions, use SA-MP syntax, or assume that successful bridge
> invocation means a peripheral operation succeeded.

Before accepting generated code:

1. One complete `main()` and `#include <computercraft>`; no C headers or Lua code
   mixed into PAWN.
2. Correct PAWN 4.1 constants/array syntax; no old `enum`.
3. Explicit device names/types, redstone sides, initial output state, and behavior
   when an input is already high at startup.
4. Every CC format string matches the number and actual type of arguments.
5. Bridge result **count** is checked; returned values are read separately.
6. All needed generic results are copied before printing or another CC helper.
7. Booleans, nil, missing JSON paths, empty strings, and errors are distinguished
   wherever the application requires it.
8. Buffers have truthful capacities; truncation is checked using that function's
   actual return convention.
9. Integers and Float values are converted numerically, not retagged by mistake.
10. Timer IDs are checked with `id < 0`, not `!id`: ID 0 can be valid.
11. Mixed-source applications use an unfiltered event loop rather than sleeps
    which discard events.
12. Persistent data is serialized, not saved as transient document/timer IDs.
13. Controlled exits restore intended outputs/channels; forced-exit behavior is
    acknowledged.
14. Optional third-party methods are discovered/documented, not assumed from a
    test peripheral or another modpack.
15. Compile locally or in-game before using on machinery. A compile check does
    not prove hardware behavior, permissions, or fail-safe operation.

If a feature is missing, say so. A small Lua launcher or a genuine addon change
may be appropriate, but a declaration alone cannot add a native API.

## 19. Building, testing, and source references

### 19.1 Build and local regression commands

These commands run in the **host shell at the repository root**, not CraftOS.
They do not contact or restart a Minecraft server.

Prerequisites: Linux x86_64, JDK 21, CMake, a C compiler, Python 3.11+, and the
pinned PAWN submodule. JDK 21 builds the Minecraft addon and runs the local
integration harness; no in-game Java worker or its sandbox tools are included.

```sh
git submodule update --init --recursive
./gradlew build --console=plain
```

Artifact: `build/libs/pawncraft-0.4.0.jar`. PAWN-focused tests:

```sh
./gradlew headlessTest pawnStdlibTest pawnTransportTest ccPawnIntegrationTest --console=plain
luajit scripts/lua_bridge_test.lua
luajit scripts/pawn_runtime_test.lua
python3 scripts/check_pawn_guide.py
python3 scripts/check_pawncraft_jar.py
```

The guide checker compiles every `pawn` block using the actual bundled
compiler/includes and checks that public helper names are documented.
Console-only examples are additionally executed with the native AMX runner
and a mock terminal transport. Hardware examples are **not** executed against
a live world. The JAR checker verifies addon metadata and required PAWN
resources, and rejects accidentally packaged remnants of the removed in-game
Java runtime. Both documentation and packaging checks are part of Gradle
`check`, alongside the runtime regression tests.

Dedicated JSON module tests use the real CC `textutils` source with a small
local Lua environment shim; substitute your local CC source directory:

```sh
luatex --luaonly scripts/pawn_json_test.lua /path/to/cc-tweaked/projects/core/src/main/resources/data/computercraft/lua
```

This JSON harness needs Lua 5.3+ (LuaTeX here); LuaJIT's Lua-5.1 pattern behavior
is not compatible with the unchanged CC JSON parser in that harness.

### 19.2 What testing proves

The test suite covers native compilation/VM execution, string/format/math
boundaries, malformed transport data, event/timer limits, JSON behavior, and
real CraftOS-core integration. The latter includes CC redstone/files,
peripheral dispatch, Ctrl+T, shutdown, watchdog behavior, and runner death
during a blocked event wait.

Headless peripherals are simulated test devices. These tests do **not** load
every modpack, prove every third-party peripheral API, or constitute a security
audit. Compilation of guide examples proves syntax/native declarations, not
every possible in-game outcome.

### 19.3 Authoritative local implementation

If code and prose ever disagree, inspect the version you actually installed:

| Area | Source |
| --- | --- |
| All public PAWN declarations | `src/main/resources/ccpawn-native/include/` |
| Native bridge / result access | `src/native/runner.c`, `src/native/transport_ext.h` |
| Standard strings/math/formatting | `src/native/stdlib.c` |
| In-game compiler/runner commands | `src/main/resources/data/computercraft/lua/rom/programs/pawncc.lua` and `pawn.lua` |
| Events/timers/UTC | `src/main/resources/data/computercraft/lua/rom/modules/main/ccpawn/runtime.lua` |
| JSON documents | `src/main/resources/data/computercraft/lua/rom/modules/main/ccpawn/json.lua` |
| Compiler/process constraints | `src/main/java/industries/knogle/ccpawn/PawnApi.java` and `NativeTools.java` |
| Real CraftOS integration tests | `src/ccIntegration/java/industries/knogle/ccintegration/PawnCraftOsTest.java` |

The generic bridge deliberately retains the underlying API's method names,
arguments, permissions, and hardware requirements. For methods not illustrated
here, consult the matching-version [CC:Tweaked reference](https://tweaked.cc/)
and the specific peripheral mod's official documentation, then apply the
bridge/type/limit rules in this guide. Do not copy Lua object-oriented examples
literally into PAWN.
