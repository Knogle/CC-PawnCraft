# PawnCraft – PAWN-Standardfunktionen in ComputerCraft

Die Erweiterungen werden mit `#include <computercraft>` automatisch geladen.
`<ccstdlib>`, `<core>`, `<string>`, `<console>` und `<float>` funktionieren ebenfalls
als Includes. Es handelt sich um einen bewusst begrenzten, CC-tauglichen Teil
der PAWN-Standardbibliothek, nicht um die vollständige Lua-Standardbibliothek.
Host-Dateien, Prozesse, dynamische Bibliotheken oder Host-Netzwerk werden dadurch
nicht als PAWN-Natives freigegeben. Die bereits vorhandene PAWN-Laufzeit ist
deshalb aber nicht automatisch eine vollständige Betriebssystem-Sandbox.

## Ausgabe und Formatierung

```pawn
#include <computercraft>

main()
{
    new signal = 12;
    new Float:fuellstand = 83.25;
    new text[128];

    printf("Signal: %d, Lager: %.1f%%\n", signal, fuellstand);
    strformat(text, sizeof text, false, "Noch %d Sekunden", 180);
    cc_println(text);
}
```

- `printf(const format[], ...)`: Ausgabe auf dem CC-Terminal bzw. dem aktuell
  umgeleiteten Monitor; kein Zugriff auf stdout des Hosts. Rückgabe:
  ausgegebene Byteanzahl, bei ungültigem Format oder CC-Fehler `-1`.
- `strformat(destination[], size, bool:pack, const format[], ...)`:
  schreibt formatierten Text in ein PAWN-Array. Rückgabe `1` bei Erfolg,
  `-1` bei Fehler. Zu kleine Zielarrays werden gekürzt und nullterminiert.
  Diese Funktion verwendet die PAWN-4.1-Signatur; ein aus anderen PAWN-Dialekten
  bekanntes `format(destination, size, ...)` ist nicht dasselbe.
- `print(text)` und `cc_println(text)` bleiben verfügbar.

| Format | Bedeutung |
| --- | --- |
| `%d`, `%i` | Vorzeichenbehafteter 32-Bit-Integer; bool ergibt `0`/`1` |
| `%u`, `%x`, `%X`, `%o` | Vorzeichenlos, hexadezimal bzw. oktal |
| `%b` | Binärdarstellung ohne Breiten-/Präzisionsangaben |
| `%c` | Ein einzelnes Byte von 1 bis 255 |
| `%s` | Packed oder unpacked PAWN-String |
| `%f`, `%F`, `%e`, `%E`, `%g`, `%G` | `Float:`-Werte |
| `%%` | Wörtliches Prozentzeichen |

Für die üblichen numerischen/String-Konvertierungen sind feste Breite und
Präzision erlaubt, etwa `%04d`, `%8.2f`, `%.20s`. Breite/Präzision sind jeweils
auf 1024 begrenzt. `%n`, `%p`, dynamische Breiten wie `%*s`, Positionsargumente
wie `%2$s`, Längenmodifikatoren wie `%ld` und unbekannte Konvertierungen werden
abgewiesen. Es gibt keine dynamische Typprüfung für Variadic-Argumente:
`%f` benötigt `Float:`-Bits, `%d` einen Integer. Ein Integer `3` wird für `%f`
nicht automatisch zu `3.0` konvertiert.

Ein vollständiges Formatierungsergebnis darf höchstens 16384 Bytes lang sein.
Ein Native-Aufruf darf maximal 128 Argumente insgesamt enthalten (also bei
`printf` bis zu 127 Formatargumente, bei `strformat` bis zu 124).
Ein ungültiges Format, fehlende Argumente oder eine zu lange Gesamtausgabe
führen zu `-1`, ohne bereits Teile des Textes auszugeben. Ein kürzeres Zielarray
bei `strformat` darf die erfolgreich erzeugte Ausgabe anschließend abschneiden.

## Strings

| Funktion | Bedeutung / Rückgabe |
| --- | --- |
| `strlen(text)` | Textlänge ohne Nullterminator |
| `strcmp(a, b, bool:ignorecase=false, length=cellmax)` | Negativ / 0 / positiv für kleiner / gleich / größer |
| `strcopy(dest, source, maxlength=sizeof dest)` | Kopieren; Anzahl geschriebener Bytes |
| `strcat(dest, source, maxlength=sizeof dest)` | Anhängen; Anzahl **zusätzlich** geschriebener Bytes |
| `strfind(text, substring, bool:ignorecase=false, index=0)` | Fundposition oder `-1` |
| `strval(text, index=0)` | Dezimalzahl lesen; führende Leerzeichen/Vorzeichen möglich; `0`, wenn keine Zahl beginnt |
| `valstr(dest, value, bool:pack=true, size=sizeof dest)` | Integer in Text umwandeln; geschriebene Byteanzahl |
| `ispacked(text)` | Speicherformat abfragen |
| `strpack(dest, source, maxlength=sizeof dest)` | In gepackten String umwandeln |
| `strunpack(dest, source, maxlength=sizeof dest)` | In ungepackten String umwandeln |
| `strequal(a, b, bool:ignorecase=false, length=cellmax)` | Komfortfunktion für `strcmp(...) == 0` |

Strings dürfen maximal 16384 Bytes haben. Beide PAWN-Speicherformate werden
unterstützt: gepackt vier Bytes je 32-Bit-Zelle und ungepackt ein Byte je Zelle.
Die Größenparameter sind **Zellanzahlen**, nicht Byteanzahlen. Deshalb nach
Möglichkeit `sizeof destination` verwenden. `strcopy` übernimmt das Format des
Quellstrings. `strcat` behält das Zielformat, bei leerem Ziel das Quellformat.
`strpack` und `strunpack` erlauben eine ausdrückliche Entscheidung.

`valstr` hat gegenüber der ursprünglichen Drei-Parameter-Native einen
zusätzlichen optionalen Größenparameter. Bestehende **Quelltexte** mit zwei
oder drei Argumenten kompilieren weiterhin; alte extern kompilierte AMX-Dateien
mit einer anderen Native-Deklaration sollten neu kompiliert werden.

Zu kleine Zielarrays werden gekürzt und nullterminiert. Ungültige Adressen,
nicht terminierte/überlange Texte oder unzulässige Zielgrößen werden abgewiesen.
Die Native prüft VM-Grenzen, kann jedoch die tatsächliche Objektgröße eines
Arrays nicht aus einer absichtlich falschen Größenangabe rekonstruieren.

Strings verwenden Byte-Semantik, keine Unicode-Codepoint-Semantik: UTF-8 kann
transportiert werden, aber `strlen`, Indizes, Kürzung und `%c` zählen Bytes.
`ignorecase`, `tolower` und `toupper` bearbeiten nur ASCII. Eine Kürzung kann
ein mehrbyteiges UTF-8-Zeichen zerschneiden. Unpacked-Zeichenwerte über 255
werden nicht akzeptiert.

## Allgemeine Helfer

```pawn
new signal = clamp(20, 0, 15); // 15
new kleiner = min(3, 8);     // 3
new groesser = max(3, 8);    // 8
new zufall = random(10);     // 0 bis 9
new buchstabe = tolower('A'); // 'a'
```

- `min(a,b)`, `max(a,b)` arbeiten mit Integern.
- `clamp(value, minimum=cellmin, maximum=cellmax)` begrenzt Integer.
- `random(maximum)` liefert Werte von `0` bis `maximum-1`; `maximum` muss
  positiv sein. Die Zufallsfolge wird pro Runner initialisiert und ist **nicht
  kryptografisch sicher**. Nicht für Passwörter/Authentifizierung verwenden.
- `tolower(character)` und `toupper(character)` ändern ASCII-Buchstaben;
  andere Byte-/Zellwerte bleiben unverändert.

Ein nichtpositives `random`-Maximum oder vertauschte `clamp`-Grenzen lösen einen
AMX-Domänenfehler aus und beenden das laufende PAWN-Programm.

## Float-Mathematik

`Float:` verwendet IEEE-754 mit **32 Bit**, nicht Luas typische 64-Bit-Zahlen.
Das Include bindet die Float-Operatoren ein, sodass normale Ausdrücke möglich
sind. Integer werden bei gemischten Operatoren konvertiert:

```pawn
new Float:anteil = 12.0 / 15.0;
new Float:prozent = anteil * 100;
printf("Fuellstand: %.1f%%\n", prozent);
```

Verfügbar sind:

- `Float:float(integer)`, `Float:strfloat(text)`
- `Float:floatadd(a,b)`, `floatsub(a,b)`, `floatmul(a,b)`, `floatdiv(a,b)`
- `Float:floatsqroot(value)`, `floatpower(value, exponent)`
- `Float:floatlog(value, base=10.0)`, `floatabs(value)`, `floatfract(value)`
- `Float:floatsin(value, mode=radian)`, `floatcos(...)`, `floattan(...)`
- `floatcmp(a,b)`: negativ / 0 / positiv
- `floatround(value, method=floatround_round)`, `floatint(value)`
- Arithmetische Operatoren `+`, `-`, `*`, `/`, `++`, `--`, Vergleiche und
  Integer-zu-Float-Zuweisung; `%` ist für Floats ausdrücklich nicht definiert.

Winkelmodi sind `radian`, `degrees` und `grades` (400 Gon pro Vollkreis).
Alle direkt an Float-Natives übergebenen numerischen Argumente müssen
`Float:`-Werte sein: `floatsqroot(9.0)`, nicht `floatsqroot(9)`.

| Rundung | Verhalten |
| --- | --- |
| `floatround_round` | Nächster Integer, halbe Werte Richtung positiv; `-1.5` → `-1` |
| `floatround_floor` | Abrunden |
| `floatround_ceil` | Aufrunden |
| `floatround_tozero` | Abschneiden Richtung 0 |
| `floatround_unbiased` | Nächster Integer, halbe Werte zur geraden Zahl |

`floatint` schneidet Richtung 0 ab. `floatfract(-1.25)` ergibt gemäß PAWN-
Konvention `0.75`. Nichtendliche Ergebnisse, Division durch null, negative
Quadratwurzeln, ungültige Logarithmen oder Integer-Konvertierungen außerhalb
des 32-Bit-Bereichs lösen einen AMX-Domänenfehler aus. Das ist ein kontrollierter
Programmabbruch, kein Lua-artiger `pcall` und keine `cc_last_error`-Rückgabe.

Beim Empfang von CC-API-Antworten werden darstellbare subnormale Float32-Werte
(z.B. `1e-40` und der kleinste positive Wert von etwa `1.40129846e-45`) erhalten.
Eine Antwort wie `1e-99`, die beim Umwandeln zu Float32 vollständig auf `0`
unterlaufen würde, wird stattdessen als CC-Aufruffehler zurückgegeben:
`cc_call(...) == -1`, Fehlertext über `cc_last_error(...)`. NaN, Infinity und
Float32-Überlauf werden ebenso abgewiesen; echte `0` und `-0` bleiben zulässig.

## Lokale Tests

```sh
python3 scripts/stdlib_test.py \
  build/native/bin/ccpawn-pawncc build/native/bin/ccpawn-runner
```

Die Tests kompilieren und starten echte PAWN-Programme. Acht Programme prüfen
Strings, beide Speicherformate, abgeschnittene/überlappende Zielarrays,
Formatierung, ungültige Formate/Argumentadressen, 16-KiB-Grenzen, alle obigen
Float-Funktionen und Operatoren sowie sechs kontrollierte Domänenfehler.
Jeder Runner hat ein hartes 10-Sekunden-Testlimit.

Die ausführlichen Testquellen sind [stdlib_test.pwn](../examples/stdlib_test.pwn)
und [stdlib_bounds_test.pwn](../examples/stdlib_bounds_test.pwn). Sie sind
Regressionstests; für Anwendungsprogramme genügen die kleinen Beispiele oben.
Zusätzlich wurden dieselben acht Programme mit AddressSanitizer und
UndefinedBehaviorSanitizer im Trap-Modus erfolgreich ausgeführt, ohne
Sanitizer-Diagnosen. Ein zusätzlicher C-Grenztest in
[`scripts/stdlib_native_test.c`](../scripts/stdlib_native_test.c) prüft absichtlich
gefälschte Parametervektoren und unterminierte Strings exakt am VM-Speicherende;
auch dieser lief erfolgreich mit beiden Sanitizern. Der reproduzierbare
Buildbefehl steht im Dateikopf. Das ersetzt kein vollständiges Sicherheitsaudit.
