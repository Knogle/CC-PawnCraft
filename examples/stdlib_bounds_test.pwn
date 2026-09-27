#include <computercraft>
#include <ccstdlib>
#pragma dynamic 20000

// Deliberately malformed native declarations simulate malicious AMX bytecode.
// Native bounds validation must reject them without dereferencing host memory.
native invalid_strlen(address) = strlen;
native invalid_copy(address, const source[], capacity) = strcopy;
native invalid_source(destination[], address, capacity) = strcopy;
native invalid_arg(const format[], address) = printf;

main()
{
    new small[4], text[64], huge[16386];
    assert invalid_strlen(0) == -1;
    assert invalid_strlen(-1) == -1;
    assert invalid_copy(0, "safe", 4) == -1;
    assert invalid_source(text, 0, sizeof text) == -1;
    assert invalid_arg("%d", 0) == -1;
    assert invalid_arg("%s", 0) == -1;
    assert strcopy(text, "safe", 2147483647) == -1;
    assert strcopy(text, "safe", 0) == -1;
    assert strformat(text, -1, false, "safe") == -1;
    assert strformat(text, sizeof text, false, "%") == -1;
    assert strformat(text, sizeof text, false, "%n", 1) == -1;
    assert strformat(text, sizeof text, false, "%p", 1) == -1;
    assert strformat(text, sizeof text, false, "%ld", 1) == -1;
    assert strformat(text, sizeof text, false, "%*s", 1, "value") == -1;
    assert strformat(text, sizeof text, false, "%1$s", "value") == -1;
    assert strformat(text, sizeof text, false, "%.999999f", 1.0) == -1;
    assert strformat(text, sizeof text, false, "%c", 0) == -1;
    assert strformat(text, sizeof text, false, "%c", 256) == -1;
    assert strformat(text, sizeof text, false, "%08b", 7) == -1;
    assert strcmp(text, text, false, -1) == -1;
    assert strfind("abc", "b", false, -1) == -1;
    assert strfind("abc", "b", false, 4) == -1;
    assert strcopy(small, "abcdefghijklmnopq") == 15;
    assert strlen(small) == 15 && strcmp(small, "abcdefghijklmno") == 0;
    assert strcat(small, "XYZ") == 0 && strlen(small) == 15;
    assert strformat(small, sizeof small, true, "abcdefghijklmnop") == 1;
    assert strlen(small) == 15;
    strcopy(text, "hello");
    assert strcopy(text, text) == 5 && strcmp(text, "hello") == 0;
    assert strcat(text, text) == 5 && strcmp(text, "hellohello") == 0;
    for (new i = 0; i < 16385; i++) huge[i] = 'A';
    huge[16385] = 0;
    assert strlen(huge) == -1;
    assert strcopy(text, huge) == -1;
    assert printf("%s", huge) == -1;
    huge[16384] = 0;
    assert strlen(huge) == 16384;
    assert printf("%s!", huge) == -1;
    assert strformat(text, sizeof text, false, "%s", huge) == 1;
    assert strlen(text) == 63;
    cc_println("stdlib bounds OK");
    return 0;
}
