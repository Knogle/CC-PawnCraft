#include <computercraft>
#include <ccstdlib>
#include <string>
#include <core>
#include <console>

stock check(bool:condition, const label[])
{
    if (!condition)
    {
        printf("FAILED: %s\n", label);
        assert condition;
    }
}

main()
{
    new text[128], small[4], packed[16];
    check(strlen("hello") == 5, "strlen packed");
    check(strunpack(text, "hello") == 5 && strlen(text) == 5, "strlen unpacked");
    check(!ispacked(text), "unpack flag");
    check(strpack(packed, text) == 5 && ispacked(packed), "pack flag");
    check(strcmp(packed, text) == 0, "mixed packing");
    check(strcmp("AbC", "abc", true) == 0, "ignore case");
    check(strcmp("AbC", "abc") < 0, "case sensitive");
    check(strcmp("alpha", "alphabet", false, 5) == 0, "prefix");
    check(strcopy(text, "hello") == 5, "strcopy");
    check(strcat(text, " world") == 6 && strcmp(text, "hello world") == 0, "strcat");
    check(strfind(text, "WORLD", true) == 6, "strfind");
    check(strfind(text, "hello", false, 1) == -1, "strfind index");
    check(strfind(text, "missing") == -1, "strfind absent");
    strunpack(text, "abcdef");
    check(strcopy(small, text) == 3 && strcmp(small, "abc") == 0, "unpacked truncation");
    check(strval("  -123tail") == -123, "strval");
    check(strval("xx42", 2) == 42, "strval offset");
    check(valstr(text, cellmin, false) == 11 && strcmp(text, "-2147483648") == 0, "valstr min");
    check(valstr(packed, 1234) == 4 && strcmp(packed, "1234") == 0, "valstr packed");
    check(valstr(small, 12345, false) == 3 && strcmp(small, "123") == 0, "valstr capacity");
    check(strformat(text, sizeof text, false, "v=%d s=%s f=%.2f %% %04X", 42, "ok", 1.5, 15) == 1, "strformat");
    check(strcmp(text, "v=42 s=ok f=1.50 % 000F") == 0, "formatted values");
    check(strformat(small, sizeof small, false, "abcdef") == 1 && strcmp(small, "abc") == 0, "format truncation");
    check(printf("OUTPUT %d %s %.2f %b\n", -42, "hello", 2.5, 5) == 26, "printf count");
    check(strformat(text, sizeof text, false, "%d %d", 1) == -1, "missing vararg");
    check(strformat(text, sizeof text, false, "%n", 1) == -1, "unsafe format");
    check(strformat(text, sizeof text, false, "%999999s", "x") == -1, "huge width");
    check(printf("do not emit %n", 1) == -1, "invalid output stays off protocol");
    check(min(3, 5) == 3 && max(3, 5) == 5, "min max");
    check(clamp(-5, 0, 15) == 0 && clamp(99, 0, 15) == 15, "clamp");
    check(tolower('A') == 'a' && toupper('z') == 'Z', "ascii case");
    for (new i = 0; i < 100; i++)
    {
        new value = random(7);
        check(value >= 0 && value < 7, "random range");
    }

    new Float:f = 2.0;
    f = (f + 3) * 2 - 1.0;
    check(floatabs(f - 9.0) < 0.001, "float operators");
    check(floatabs(20 / f - 2.222222) < 0.001, "mixed division");
    check(floatround(floatadd(1.0, 2.0)) == 3, "floatadd");
    check(floatround(floatsub(5.0, 2.0)) == 3, "floatsub");
    check(floatround(floatmul(2.0, 3.0)) == 6, "floatmul");
    check(floatround(floatdiv(6.0, 2.0)) == 3, "floatdiv");
    check(floatround(floatsqroot(81.0)) == 9, "sqrt");
    check(floatround(floatpower(2.0, 3.0)) == 8, "power");
    check(floatround(floatlog(100.0)) == 2, "log");
    check(floatabs(floatsin(90.0, degrees) - 1.0) < 0.001, "sin degrees");
    check(floatabs(floatcos(0.0) - 1.0) < 0.001, "cos radians");
    check(floatabs(floattan(50.0, grades) - 1.0) < 0.001, "tan grades");
    check(floatround(-1.5) == -1, "round ties toward positive");
    check(floatround(2.5, floatround_unbiased) == 2 && floatround(3.5, floatround_unbiased) == 4, "round ties even");
    check(floatround(-1.2, floatround_floor) == -2, "floor");
    check(floatround(-1.2, floatround_ceil) == -1, "ceil");
    check(floatint(-1.9) == -1 && floatround(-1.9, floatround_tozero) == -1, "truncate");
    check(floatabs(floatfract(-1.25) - 0.75) < 0.001, "fraction");
    check(floatabs(strfloat("3.25") - 3.25) < 0.001, "strfloat");
    check(floatcmp(1.0, 2.0) < 0 && floatcmp(2.0, 2.0) == 0, "floatcmp");
    cc_println("stdlib OK");
    return 0;
}
