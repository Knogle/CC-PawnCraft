#if defined _ccstdlib_included
  #endinput
#endif
#define _ccstdlib_included

// CC-safe subset of the PAWN 4.1 standard library. No host OS APIs.
#include <float>

native printf(const format[], {Float,bool,_}:...);
native strformat(destination[], size = sizeof destination, bool:pack = true, const format[], {Float,bool,_}:...);
native strlen(const string[]);
native strcmp(const first[], const second[], bool:ignorecase = false, length = cellmax);
native strcopy(destination[], const source[], maxlength = sizeof destination);
native strcat(destination[], const source[], maxlength = sizeof destination);
native strfind(const string[], const substring[], bool:ignorecase = false, index = 0);
native strval(const string[], index = 0);
// The extra optional size keeps existing 2/3 argument calls source-compatible
// while ensuring the native never needs to guess a destination's capacity.
native valstr(destination[], value, bool:pack = true, size = sizeof destination);
native bool:ispacked(const string[]);
native strpack(destination[], const source[], maxlength = sizeof destination);
native strunpack(destination[], const source[], maxlength = sizeof destination);
stock bool:strequal(const first[], const second[], bool:ignorecase = false, length = cellmax)
    return strcmp(first, second, ignorecase, length) == 0;

native min(value1, value2);
native max(value1, value2);
native clamp(value, minimum = cellmin, maximum = cellmax);
native random(maximum);
native tolower(character);
native toupper(character);
