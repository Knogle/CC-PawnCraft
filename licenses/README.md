# License inventory

PawnCraft's original code is MIT licensed. Bundled third-party code keeps
its own terms; the MIT license does not replace them.

| Component | Terms and retained notices |
| --- | --- |
| Original PawnCraft code | [MIT](../LICENSE), copyright 2026 Knogle Industries |
| NeoForge MDK template files | [MIT](../TEMPLATE_LICENSE.txt), copyright 2023 NeoForged project |
| CompuPhase PAWN compiler and AMX implementation | [PAWN license](pawn-LICENSE.txt) and [upstream notices](pawn-NOTICE.txt), principally Apache-2.0 with the upstream object-linking exception |
| Adapted PAWN `float.inc` | The same PAWN license and notices; its source header retains Artran/Greg Garner attribution and identifies local changes |
| Compiler `strlcpy` and `strlcat` | [ISC notice](ISC-strlcpy-strlcat.txt), copyright 1998 Todd C. Miller |
| Dynamically used system GNU C Library | [Runtime notice](glibc-NOTICE.txt) and [LGPL-2.1](LGPL-2.1.txt); glibc is not bundled |

## Provenance

- `pawn-LICENSE.txt` and `pawn-NOTICE.txt` are byte-for-byte copies of
  `LICENSE` and `NOTICE` from the PAWN submodule pinned at
  `db97293fa949d7627272a1c8d7a9470818bdbe91`. Their original line endings
  are preserved. Upstream: <https://github.com/compuphase/pawn>.
- `ISC-strlcpy-strlcat.txt` preserves the complete notice from
  `compiler/lstring.c` at that revision. The two functions carry identical
  notices; the text is included once, with only C comment markers removed.
- `LGPL-2.1.txt` is the complete GNU LGPL version 2.1 text, copied from
  the GNU C Library package's `COPYING.LIB`, including its application
  appendix. The [official GNU C Library copy](https://sourceware.org/glibc/manual/latest/html_node/Copying.html)
  provides the same license terms.

The PAWN exception only applies to qualifying object-form distributions
of unmodified upstream work. It does not replace the normal Apache source
requirements for the adapted `float.inc`, nor the separate ISC terms.
The full upstream NOTICE is retained, including acknowledgements of tools
and optional modules that PawnCraft does not compile or bundle.

`src/native/compiler.c` is an original MIT-licensed output adapter that includes
the unchanged upstream compiler source. It adds GNU C Library attribution when
the compiler displays its copyright banner. PawnCraft retains the complete
Apache license and notices for this adapted executable and does not rely on
the object-linking exception to omit them.

These checked-in copies make release notices available in source archives
without an initialized submodule. Building the native tools still requires
the pinned PAWN sources. Keep these copies synchronized when changing the
submodule revision, and reassess the inventory when adding compiled modules
or changing native linking.

A recursive source checkout also contains upstream PAWN documentation under
CC BY-SA 3.0, as described in `pawn-LICENSE.txt`. Those manuals are not bundled
in the PawnCraft JAR. Their terms are separate from independently authored
PawnCraft documentation.
