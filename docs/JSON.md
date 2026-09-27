# PawnCraft – JSON in PAWN/ComputerCraft

Die Helfer sind mit `#include <computercraft>` verfügbar; alternativ kann
`#include <ccjson>` verwendet werden. Sie verwenden die echte JSON-API von
ComputerCraft (`textutils`), keine externen Prozesse oder Host-Dateien.

## Dokumente und Rückgabewerte

`cc_json_parse(text)`, `cc_json_object()` und `cc_json_array()` erzeugen einen
Dokument-Handle. Ein positiver Wert bedeutet Erfolg, `0` einen Fehler.
`cc_json_free(handle)` gibt ihn wieder frei. Alle Dokumente gehören zum aktuellen
PAWN-Programmlauf und werden nach dessen Ende automatisch verworfen. Handles
können nicht an andere Programme übergeben werden.

```pawn
#include <computercraft>

main()
{
    new document = cc_json_parse("{\"machine\":{\"active\":true},\"items\":[{\"count\":12}]}");
    if (!document) return 1;

    if (cc_json_get(document, "/items/0/count") < 1) return 2;
    new count = cc_result_int(0);

    if (!cc_json_set_int(document, "/items/0/count", count + 1)) return 3;
    if (!cc_json_set_bool(document, "/machine/active", false)) return 4;

    new text[512];
    if (cc_json_stringify(document, text) < 0) return 5;
    cc_println(text);
    cc_json_free(document);
    return 0;
}
```

Ein weiteres Beispiel steht in [examples/json.pwn](../examples/json.pwn).

| Funktion | Rückgabe / Verhalten |
| --- | --- |
| `cc_json_parse(text)` | Positiver Handle oder `0` bei ungültigem JSON / überschrittenem Limit |
| `cc_json_object()` / `cc_json_array()` | Neues leeres Objekt `{}` / Array `[]` |
| `cc_json_get(handle, pointer = leer)` | `1` oder `-1`; Wert sofort über `cc_result_*` lesen |
| `cc_json_type(handle, pointer = leer)` | `CC_JSON_NULL`, `BOOL`, `NUMBER`, `STRING`, `ARRAY`, `OBJECT` (jeweils Präfix `CC_JSON_`), oder `-1` |
| `cc_json_stringify(handle, buffer, size = sizeof buffer)` | Geschriebene Länge oder `-1`; ein zu kleiner Puffer ist ein Fehler, kein stilles Abschneiden |
| `cc_json_set_string/int/float/bool(handle, pointer, value)` | Wert setzen; `true` / `false` |
| `cc_json_set_null(handle, pointer)` | Explizites JSON `null` setzen; `true` / `false` |
| `cc_json_set_json(handle, pointer, text)` | JSON-Teilbaum aus Text einsetzen; `true` / `false` |
| `cc_json_free(handle)` | Freigeben; `true` / `false`; bereits freigegebene Handles sind ungültig |

Nach einem Fehler enthält `cc_last_error(buffer)` eine Erklärung. Wie bei jedem
anderen CC-Aufruf überschreibt der nächste Aufruf die `cc_result_*`-Werte.

`cc_json_get` liefert bei JSON `null` genau ein Ergebnis mit `CC_RESULT_NIL`.
Ein fehlender Pfad dagegen ist ein Fehler (`-1`). Zahlen, Booleans und Strings
lassen sich mit den üblichen `cc_result_int/float/bool/string`-Funktionen lesen.
Unterobjekte und Arrays werden als **JSON-Text mit `CC_RESULT_STRING`** geliefert;
dieser Text kann mit `cc_json_parse` in einen unabhängigen Handle übernommen
werden. `cc_json_type` unterscheidet Array und Objekt, auch wenn beide leer sind.

## JSON Pointer

Pfade sind JSON Pointer, keine Lua-Ausdrücke:

- `""` bezeichnet die Wurzel.
- `"/machine/active"` bezeichnet ein verschachteltes Objektfeld.
- `"/items/0/count"` liest das erste Arrayelement; Indizes beginnen bei **0**.
- `"/items/-"` hängt bei einem Setter ein Element an. Lesen von `-` ist ungültig.
- Ein Setter auf den Index direkt hinter dem letzten Element hängt ebenfalls an.
  Lücken in Arrays sind nicht erlaubt; `01` und negative Indizes sind ungültig.
- In Schlüsseln steht `~1` für `/`, `~0` für `~`. `"/"` bezeichnet den leeren
  Objektschlüssel, nicht die Wurzel.

Objektfelder können angelegt werden, aber Zwischenobjekte müssen bereits
existieren. Erst `cc_json_set_json(doc, "/machine", "{}")`, dann beispielsweise
`cc_json_set_bool(doc, "/machine/active", true)`.
Die Setter sind atomar: Bei einem Fehler bleibt das alte Dokument unverändert.
`null` setzt einen Wert, löscht aber kein Feld.

## Peripheral-Rückgaben

Bei einem Peripheral-Aufruf mit Tabellenrückgabe kann das vorhandene
`CC_RESULT_JSON` zunächst in einen PAWN-String kopiert und dann geparst werden:

```pawn
// Vorher: cc_call("peripheral", "call", ...), dessen erstes Ergebnis eine Tabelle ist.
new json_text[4096];
if (cc_result_string(0, json_text) >= sizeof json_text)
    return 1; // Vollständige Quelle benötigt; keine abgeschnittenen Daten parsen.
new document = cc_json_parse(json_text);
```

Die konkrete Methode und die Feldnamen hängen von der angeschlossenen Mod ab.
Lua-Funktionen und Datei-/Gerätehandles sind keine JSON-Werte.

## Grenzen und Fallstricke

- Pro Programmlauf höchstens **32 gleichzeitig lebende Dokumente**. Nicht mehr
  benötigte Handles explizit freigeben; freigegebene IDs werden nicht wiederverwendet.
- JSON-Quelltext und serialisiertes Dokument jeweils höchstens **16 KiB**.
  Höchstens **32 Verschachtelungsebenen** und **2048 Werte** pro Dokument,
  einschließlich der Wurzel. Jeder Container und jeder skalare Wert zählt.
- `null`, `{}` und `[]` bleiben unterscheidbar. NaN und Unendlich werden abgelehnt.
  NUL-Zeichen werden ebenfalls abgelehnt, weil PAWN-Strings NUL-terminiert sind.
- Die JSON-Helfer behandeln Strings als **UTF-8** (`unicode_strings = true`),
  damit beispielsweise Umlaute nicht bei jedem Roundtrip neu als Einzelbytes
  codiert werden. BMP-Umlaute und Teilbaum-Roundtrips sind getestet. Die Regeln
  des CC-`textutils`-Parsers gelten weiterhin, insbesondere für Unicode-Escapes;
  dies ist kein zusätzlicher vollständiger Unicode-/JSON-Konformitätsparser.
  Die Darstellung eines Zeichens im CC-Terminal ist davon unabhängig.
- Im Dokument selbst werden Zahlen als Lua-Zahlen gespeichert, nicht als
  beliebig genaue Dezimalzahlen. **Keine Garantie für große Integer außerhalb
  des exakt darstellbaren Bereichs**, keine Erhaltung der ursprünglichen
  Zahlen-Schreibweise. Große IDs und präzise Dezimalwerte besser als Strings speichern.
- Beim Rückweg nach PAWN passen Integer nur in eine **vorzeichenbehaftete
  32-Bit-Zelle**. Andere Zahlen laufen über **32-Bit-Float** und können runden.
  Beispielsweise ist ein großer Energiezähler nicht automatisch präzise, nur
  weil JSON ihn als Zahl darstellen kann. `cc_result_type` prüfen; große Werte
  nicht ungeprüft über `cc_result_int` lesen.
  Nicht-endliche Ergebnisse sowie Beträge oberhalb des Float32-Maximums
  (ungefähr `3.402823466e38`) werden beim numerischen Zugriff als API-Fehler
  abgewiesen, statt Infinity zurückzugeben. Beispielsweise kann `1e100` im
  JSON-Dokument geparst und wieder als JSON-Text serialisiert werden, aber
  `cc_json_get` dieses Zahlenwerts liefert `-1` mit `cc_last_error`.
- JSON-Ausgaben sind semantisch zu vergleichen; die Reihenfolge von Objektfeldern
  und die ursprüngliche Formatierung werden nicht erhalten.

## Lokale Tests

Die Modul-Regressionen verwenden die echte lokale CC-`textutils`-Implementierung:

```sh
luatex --luaonly scripts/pawn_json_test.lua /path/to/cc-tweaked/projects/core/src/main/resources/data/computercraft/lua
```

17 Testgruppen decken unter anderem leere Werte, Null, Pfade, False-Setter,
Array-Anhängen, Unicode, atomare Fehler, Größen-/Tiefen-/Handle-Limits und
Puffergrößen ab. Der Test benötigt Lua 5.3 oder höher (hier über LuaTeX); LuaJIT
mit Lua-5.1-Mustern ist für die unveränderte CC-`textutils`-Datei nicht geeignet.
