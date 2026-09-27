# PawnCraft 0.4.1 – PAWN-Bibliotheken für ComputerCraft

`#include <computercraft>` lädt alle hier beschriebenen Helfer. Zusätzlich
akzeptiert der Ingame-Compiler die gebündelten Includes `ccstdlib`, `float`,
`string`, `core`, `console`, `time`, `ccevents`, `ccperipheral`, `ccjson` und
`default`. Fremde/absolute Include-Pfade bleiben gesperrt. Es wird weiterhin
PAWN 4.1 verwendet: Aufzählungen haben `const`-Syntax, nicht `enum`.

Das Addon muss auf dem Server installiert und der Server anschließend neu
gestartet werden. Ein lokaler Build allein aktualisiert keinen laufenden Server.
Alte AMX-Dateien mit unseren bisherigen Natives
bleiben vorgesehen kompatibel; neue Funktionen brauchen den neuen Runner,
die neuen Includes und die neue Lua-Brücke zusammen.

## Formatierung, Strings, Mathematik

```pawn
new text[64];
new signal = clamp(20, 0, 15);
strformat(text, sizeof text, false, "Signal: %d", signal);
printf("%s / Fuellstand: %.1f%%\n", text, 87.5);

new Float:value = floatsqroot(81.0);
printf("Wurzel: %.2f\n", value);
```

Enthalten sind `printf`, `strformat`, `strlen`, `strcmp`, `strcopy`, `strcat`,
`strfind`, `strval`, `valstr`, `strpack`, `strunpack`, `ispacked`, `strequal`,
`min`, `max`, `clamp`, `random`, `tolower`, `toupper` und die Float-Bibliothek
einschließlich ihrer Operatorüberladungen. Ausführliche Signaturen, Formate,
Puffergrenzen und Fehlerfälle: [PAWN-STDLIB.md](PAWN-STDLIB.md).

`print`, `cc_println` und `printf` gehen durch die CC-Terminalausgabe und
unterstützen jetzt echte Zeilenumbrüche/Scrolling sowie Monitor-Umleitung.
Sie schreiben nicht in den Minecraft-Chat.

## Events

```pawn
new event_name[32];
if (cc_pull_event("key") > 0)
{
    cc_event_name(event_name);
    new key_code = cc_event_int(1);
    printf("Event %s, Taste %d\n", event_name, key_code);
}
```

| Funktion | Bedeutung |
| --- | --- |
| `cc_pull_event(filter = leer)` | Blockiert bis zu einem passenden Event; Ergebnisanzahl oder −1 |
| `cc_event_count()` | Anzahl der Werte einschließlich Ereignisname |
| `cc_event_name(dest, size)` | Ereignisname kopieren |
| `cc_event_type(index)` | Einer der bestehenden `CC_RESULT_*`-Typen, −1 bei Fehler |
| `cc_event_string(index, dest, size)` | String bzw. JSON-Tabellenwert kopieren |
| `cc_event_int/bool/float(index)` | Typisierten Ereigniswert lesen |

Index 0 ist der Name, Index 1 das erste Argument. Wie bei Lua enthalten z. B.
`key`, `char`, `redstone`, `timer` und `modem_message` unterschiedliche Werte.
Für `redstone` liest man anschließend die Eingänge selbst. Der letzte Event-
Snapshot bleibt bis zum nächsten `cc_pull_event` erhalten, auch über `printf`
und andere CC-Aufrufe hinweg. Dagegen werden die allgemeinen `cc_result_*`
weiterhin durch jeden CC-Aufruf ersetzt.

Eine separate Lua-Coroutine sammelt Events während der nativen IPC-Wartezeiten.
Maximal 128 Ereignisse werden gepuffert; bei Überlauf endet das Programm mit
einem Fehler, statt unbegrenzt Speicher zu belegen. Interne `ccpawn_*`-Events
werden nicht an das PAWN-Programm geliefert. Beim gefilterten Warten werden
nicht passende Ereignisse verworfen, ähnlich wie bei Lua `os.pullEvent`.
Ctrl+T wird nicht als gewöhnlicher Fehler geschluckt, sondern beendet den Runner.

## Timer und Zeit

```pawn
new timer = cc_start_timer(180000); // 3 Minuten, einmalig
if (timer < 0 || cc_wait_timer(timer) < 0)
    return 1;
printf("Fertig nach drei Minuten!\n");
```

| Funktion | Semantik |
| --- | --- |
| `cc_sleep(ms)`, `delay(ms)` | CC-Warten; 0 bei Erfolg, −1 bei API-Fehler |
| `cc_start_timer(ms)` | Einmaligen Timer starten; ID oder −1 |
| `settimer(ms, singleshot = false)` | Standardmäßig wiederholender **CC-Event-Timer**, ID oder −1 |
| `cc_cancel_timer(id)` | Timer entfernen: 1 abgebrochen, 0 unbekannt/beendet, −1 Fehler |
| `cc_wait_timer(id)` | Auf diese ID warten; andere Ereignisse werden beim Warten verworfen |
| `gettime(&h, &m, &s)` | UTC-Stunde/Minute/Sekunde; Rückgabe Unixsekunden oder −1 |
| `getdate(&y, &m, &d)` | UTC-Datum; Rückgabe Jahrestag 1–366 oder −1 |
| `tickcount(&granularity)` | Laufzeit des CC-Computers in Millisekunden, Granularität 50 ms |

**`settimer` ist hier eine ausdrücklich CC-spezifische Anpassung:** Es erzeugt
`timer`-Events mit einer stabilen ID, ruft aber kein PAWN-`@timer` auf. Für
Wiederholung mehrfach `cc_wait_timer(id)` verwenden; zum Stoppen
`cc_cancel_timer(id)`, nicht `settimer(0)`. Ein wiederholender Timer muss eine
positive Dauer haben. Maximal 32 noch nicht konsumierte/aktive Timer pro
Programmlauf. Mehrere noch nicht konsumierte Ticks desselben wiederholenden
Timers werden zusammengefasst, nicht unbegrenzt nachgeholt.

Wie bei CC selbst kann beim Abbrechen eines Timers ein bereits in der
CC-Ereigniswarteschlange liegendes Timer-Ereignis noch ankommen. Nach
`cc_cancel_timer` deshalb keine Aktionen allein wegen eines verspäteten Events
auslösen, sondern den eigenen aktiven Zustand beziehungsweise die erwartete ID
prüfen; bei wiederholenden Timern kann ein bereits eingereihter Roh-Tick auftauchen.

Alle Timer laufen über CC und damit auf Minecraft-Ticks (mindestens 50 ms);
Serverlag verlängert reale Wartezeiten. Es sind keine Echtzeitgarantien.
`gettime`/`getdate` lesen dagegen das reale UTC-Datum über die CC-API, nicht die
Minecraft-Tageszeit. Unixsekunden passen nur bis Januar 2038 in eine positive
32-Bit-Zelle; außerhalb des Bereichs wird ein Fehler geliefert. `tickcount`
läuft nach 2³² ms um und wird ab 2³¹ ms negativ. Es darf dann nicht einfach
als −1-Fehlerkonvention interpretiert werden.

`settime`, `setdate` und `settimestamp` sind nicht freigegeben: Ein Spieler-
Programm darf die Serveruhr nicht ändern. Beim Programmende werden unsere
noch laufenden Timer abgebrochen. Normales `main`-Ende hält das Programm nicht
allein wegen eines Timers am Leben.

## Peripherals

```pawn
new name[128];
if (cc_peripheral_find("monitor", name) > 0)
{
    cc_peripheral_call(name, "setTextScale", "f", 0.5);
    cc_peripheral_call(name, "setCursorPos", "ii", 1, 1);
    cc_peripheral_call(name, "write", "s", "Hallo aus PAWN!");
}
```

- `cc_peripheral_is_present(name)` und `cc_peripheral_has_type(name, type)`:
  boolesche Prüfung. Nicht vorhandene Geräte bzw. API-Fehler ergeben false;
  für Diagnose `cc_last_error` sofort danach lesen.
- `cc_peripheral_find(type, dest, size)` gibt den ersten passenden Namen in
  sortierter Reihenfolge zurück: Länge >0, 0 bei keinem Treffer, −1 bei Fehler.
- `cc_peripheral_get_type(name, dest, size)` kopiert den ersten Gerätetyp.
- `cc_peripheral_call(name, method, format, ...)` verwendet `i`, `b`, `f`, `s`,
  `j` wie `cc_call`; höchstens 14 Methodenargumente. Rückgabeanzahl oder −1,
  Ergebnisse anschließend mit `cc_result_*` lesen.
- `cc_peripheral_get_names()` und `cc_peripheral_get_methods(name)` liefern
  ihre Liste über `cc_result_string(0, ...)` als JSON.

Es entstehen keine Lua-Closures wie bei `peripheral.wrap`; der Geräte-Name
bleibt ein normaler String. Die Geräte müssen wirklich angeschlossen sein,
und es gelten ihre normalen CC-Berechtigungen. Keine zusätzlichen Rechte.

Zusätzlich: `cc_redstone_get_output(side)` und
`cc_redstone_get_analog_output(side)` ergänzen die vorhandenen Redstone-Helfer.

## JSON

Einzelheiten und Grenzen stehen in [JSON.md](JSON.md). Typisches Vorgehen:

```pawn
new doc = cc_json_parse("{\"machine\":{\"enabled\":true}}");
if (!doc) return 1;
cc_json_get(doc, "/machine/enabled");
new bool:enabled = cc_result_bool(0);
cc_json_set_bool(doc, "/machine/enabled", !enabled);
new output[256];
if (cc_json_stringify(doc, output) >= 0) printf("%s\n", output);
cc_json_free(doc);
```

Eigene Handles statt beliebiger Lua-Tabellen. JSON Pointer verwenden
0-basierte Array-Indizes. Null und fehlender Schlüssel werden unterschieden;
zu kleine Stringify-Puffer sind Fehler statt stiller Datenverlust. Handles
gehören zum jeweiligen PAWN-Programmlauf und müssen bei längeren Programmen
freigegeben werden. Lua-Zahlen und PAWN-Float32 haben unterschiedliche Präzision.

## Lokal testen

```sh
./gradlew clean build --console=plain
./gradlew pawnStdlibTest pawnTransportTest headlessTest ccPawnIntegrationTest --console=plain
luajit scripts/lua_bridge_test.lua
luajit scripts/pawn_runtime_test.lua
```

Echte native Compiler-/VM-Tests prüfen Strings, Formatierung, Puffergrenzen,
Mathematik und ungültige Aufrufe. `ccPawnIntegrationTest` führt echte CraftOS-
Programme aus: Compiler, Runner, Lua-Brücke, CC-Redstone/Dateien, Ereignisse,
Timer, UTC-Zeit und JSON. Peripherals werden im Headless-Test simuliert;
dieser Test lädt keine Drittanbieter-Mod und startet keine Minecraft-Welt.

Die schnellen Lua-Regressionen prüfen zusätzlich Timer-/Event-Limits,
Wiederholungs-Coalescing, Abbruch, Ressourcenfreigabe und Zahlen-Grenzwerte.
Die separaten JSON-Tests mit echter CC-`textutils`-Implementierung sind in
[JSON.md](JSON.md#lokale-tests) beschrieben.

Java 21 wird zum Bauen und Ausführen des Minecraft-/NeoForge-Addons benötigt;
eine Ingame-Java-Laufzeit ist in PawnCraft nicht enthalten.
Die native PAWN-Ausführung erhält durch diese Bibliothekserweiterung keine
neue Betriebssystem-Sandbox. Host-Datei-/Prozess-/Netzwerk-Natives bleiben aus.
