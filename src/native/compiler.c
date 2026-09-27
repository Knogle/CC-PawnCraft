/* SPDX-License-Identifier: MIT
 * Original PawnCraft adapter, copyright (c) 2026 Knogle Industries.
 * Include the unchanged upstream compiler source with one output adapter.
 * The included code retains its upstream license and notices; see
 * licenses/pawn-LICENSE.txt and licenses/pawn-NOTICE.txt. Our adapter adds
 * the GNU C Library attribution when upstream prints its copyright banner.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int pawncraft_compiler_vprintf(const char *format, va_list arguments) {
    int result = vprintf(format, arguments);
    /* Match only setcaption() from the pinned compiler, not user messages.
     * The CLI regression test deliberately detects future banner changes. */
    if (strcmp(format,
               "Pawn compiler %-25s Copyright (c) 1997-2024, CompuPhase\n\n") == 0) {
        fputs("GNU C Library: Copyright (C) Free Software Foundation, Inc. and other contributors.\n"
              "LGPL-2.1-or-later; see META-INF/licenses/glibc/LGPL-2.1.txt in the PawnCraft JAR.\n\n",
              stdout);
    }
    return result;
}

/* Parse stdio above before adapting this call, so libc inline definitions
 * remain untouched. Upstream keeps main(), argument parsing and all I/O. */
#define vprintf pawncraft_compiler_vprintf
#include "../../third_party/pawn/compiler/sc1.c"
#undef vprintf
