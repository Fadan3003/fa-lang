# Fa — eine kleine, einfache Programmiersprache (v0.18)

Ein einziges Programm in C++ (`fa`). Kein Python, keine Abhängigkeiten (raylib nur für Fenster).

## Installieren

```
sh install.sh          # baut Fa und installiert es nach ~/.local/bin (installiert g++, falls es fehlt)
```
Danach ein neues Terminal öffnen:
```
fa new meineseite      # erstellt meineseite.fa (eine Start-Webseite)
fa meineseite          # starten -> http://localhost:8080
```
`fa datei.fa` startet ein Programm, `fa` allein öffnet die Eingabe (mit Leerzeile beendet man einen Block), `fa help` zeigt alles.

Fenster auf dem Bildschirm (`window`): einmal `sh install-raylib.sh`, dann `sh install.sh gui`.

## Die ganze Sprache auf einer Seite

Wie Python: **Einrückung** zeigt, was zusammengehört. Kein `end`, keine geschweiften Klammern.

```
# Kommentar

name = "Fadan"                  # Variable anlegen (oder ändern)
print "Hallo {name}!"           # {...} setzt einen Wert in den Text

alter = 20
if alter >= 18
    print "erwachsen"
else if alter >= 13
    print "Teenager"
else
    print "Kind"

for i in 1..5                   # 1, 2, 3, 4, 5
    print i

while alter < 25
    alter += 1

def addiere a, b                # Funktion
    return a + b
print addiere(2, 3)

dinge = ["Schwert", "Bogen"]    # Liste
dinge.add "Schild"
print dinge[0], len(dinge)      # Positionen beginnen bei 0

spieler = {name: "Steve", leben: 20}    # Map: Namen mit Werten
spieler.leben -= 7
print spieler.name, spieler.leben
```

Das ist alles:

- **Werte:** Zahlen, Text, `true` / `false`, `nothing`, Listen, Maps.
- **Variablen:** `x = 5` legt `x` an oder ändert es, falls es schon existiert. Parameter einer Funktion gehören immer der Funktion.
- **Blöcke** (nach `if`, `else`, `while`, `for`, `def`) sind eingerückt. Ein `:` am Ende der Zeile ist erlaubt, aber nicht nötig.
- **Ein Name allein in einer Zeile ruft die Funktion auf:** `spacer`, `br` oder `speichern` (wie `speichern()`).
- **Klammern sind meist optional:** `print "Hallo"` und `print("Hallo")` sind gleich. Ohne Klammern geht es, wenn der Aufruf die ganze Zeile ist (oder rechts von `=` steht). Mitten in einem Ausdruck nimmt man Klammern: `print addiere(2, 3)`. Bei `def` sind die Klammern auch optional: `def addiere a, b`.
- **Logik:** `and`, `or`, `not`, `==`, `!=`, `<`, `>`, `<=`, `>=`. Nur `false` und `nothing` zählen als „nein“.
- **Fehlermeldungen** nennen die Zeile und sagen, was zu tun ist.

### Mit Punkt

| Auf | Was |
|---|---|
| Text | `upper lower trim split contains replace starts_with ends_with slice` |
| Liste | `add pop join contains sort reverse slice` |
| Map | `keys values has get remove` und `spieler.leben` für einen Wert, `m.get("x", 0)` mit Vorgabewert |

Text ist echtes Unicode: `"Grüße".upper()`, `len("größe")` und Namen wie `größe = 5` gehen.

### Eingebaute Funktionen

| Gruppe | Funktionen |
|---|---|
| Ausgabe, Eingabe | `print ask len text number chr ord` |
| Mathe | `sqrt round floor abs min max random` |
| Dateien | `read_file write_file file_exists list_files delete_file` |
| System | `run env wait now exit args` und die Werte `here` (Ordner des Programms), `interpreter` (Pfad zu fa), `os` |
| Web | `route serve status content_type json page document h1 h2 h3 p div span ul ol li pre bold italic code link image input br hr tag raw` |
| Fenster | `window label button` |

### Webseiten

Eine Seite ist ein `route`. Darunter steht, was die Seite zeigt. Dann `serve`.

```
besucher = 0

route "/"
    besucher += 1
    page "Meine Seite"
        h1 "Hallo von Fa"
        p "Du bist Besucher Nummer {besucher}."
        ul
            for ding in ["Eins", "Zwei"]
                li ding
            li link("/about", "Über uns")

route "/api"
    return {besucher: besucher}          # eine Map oder Liste wird als JSON gesendet

route "*"                                # alle anderen Adressen
    status 404
    page "Nicht gefunden"
        p "Die Seite {path} gibt es nicht."

serve 8080
```
Das Prinzip: **Unter einem Aufruf darf ein eingerückter Block stehen. Jede Zeile darin, die etwas ergibt, wird gesammelt und dem Aufruf mitgegeben.**
So wird `h1 "Hallo"` zum Inhalt von `page`, und `li ding` zum Inhalt von `ul`. `for` und `if` funktionieren dazwischen.
Attribute stehen in einer Map: `div {class: "box"}`. Text wird automatisch sicher gemacht (kein HTML-Einschleusen).
In einer `route` gibt es diese Variablen:

| Variable | Inhalt |
|---|---|
| `path` | die Adresse ohne `?...` |
| `method` | `"GET"` oder `"POST"` |
| `query` | die Werte aus `?a=1&b=2` als Map, z. B. `query.get("a")` |
| `body` | der Text, den ein POST mitschickt |

`status 404` setzt den Status, `content_type "text/plain"` die Art der Antwort.
Für eine Seite ohne feste Gestaltung (Web-Apps) gibt es `document "Titel", {css: "/a.css", js: "/a.js"}` statt `page`.

Eigene Bausteine: eine Funktion bekommt den Block als letzten Wert und ruft ihn mit `()` auf. Sie muss ihr Ergebnis mit `return` liefern.

```
def karte titel, inhalt
    return div {class: "karte"}
        h2 titel
        inhalt()

route "/"
    page "Test"
        karte "Meine Karte"
            p "Das steht in der Karte"
```

### Was im Inspektor (F12) steht: dein Fa-Code

**Standardmäßig steht im Inspektor deine ganze `.fa`-Datei.** Du musst dafür nichts schreiben: `serve 8080` reicht. Wer F12 drückt, sieht im Reiter „Elemente“
kein `<!DOCTYPE html>`, kein `<html>`, `<head>` oder `<body>`, sondern deinen Fa-Code, Zeile für Zeile, vom ersten Kommentar bis zur letzten Zeile. Im Reiter „Sources“ liegt
dieselbe Datei als normaler Code (mit Zeilennummern, ohne Anführungszeichen). Die Seite sieht dabei genauso aus und funktioniert genauso.

```
serve 8080                            # Standard: deine ganze Datei im Inspektor
serve 8080, {code_root: false}        # aus: normale Seite (nur geschützt), kein Code im Inspektor
serve 8080, {show_code: true}         # nur der Code des route-Blocks statt der ganzen Datei
serve 8080, {show_code: false}        # kein Code, aber die Struktur ohne html/head/body
serve 8080, {code_root: "sources"}    # Elemente fast leer, die ganze Datei nur im Reiter Sources
```
**Achtung, das ist wichtig:** Weil das der Standard ist, bekommt **jeder Besucher deiner Seite deine ganze Datei**, mit allem, was darin steht:
Passwörter, Schlüssel, Dateipfade. Schreibe solche Dinge nie in eine Datei mit `serve`, oder schalte es mit `code_root: false` ab.
Fa schreibt dir bei jedem Start eine Warnung in die Konsole. In `fa new` steht dazu ein Hinweis in der Vorlage.

**Seiten mit eigenem JavaScript** funktionieren so nicht (siehe unten bei „Statt html, head und body“). Für sie gilt: `serve 8080, {code_root: false, obfuscate: "light"}`.
Das macht auch `editor/editor.fa`, der Code-Editor im Browser.

### Der Quelltext deiner Seite ist geschützt

Die echte Seite (in `<fa-view>`) und ihr Quelltext sind **standardmäßig geschützt**. Wer Strg+U drückt (Quelltext) oder im Netzwerk-Reiter nachsieht,
findet nur Zeichensalat. Öffnet man die Seite im Inspektor weiter (`<fa-view>` aufklappen), sieht sie so aus:

```
<body><zekax3e k2hqp2b=""><z02zl7j>Überschrift</z02zl7j><zmfrxqk id="q4f7uas" kyblb2x=""><zceul4b kjq14qk="">…
```
Alle Tags (`div`, `h1`, `p`, `ul`, `li`, `span` ...) heißen nur noch Buchstaben und Zahlen, und die Wörter `class` und `id` kommen nicht mehr vor:
aus `class="karte"` wird ein Attribut mit zufälligem Namen. Deine CSS-Dateien werden passend umgebaut (`.karte`, `#menu` und `h1` funktionieren weiter,
auch die Gewichtung stimmt: eine ID schlägt eine Klasse wie vorher). Die Seite sieht genau gleich aus und funktioniert genauso
(Links, Anker, Beschriftungen, Eingabefelder, mehrere Seiten).

```
serve 8080, {code_root: false}      # ohne deinen Code im Inspektor, die Seite ist nur geschützt
serve 8080, {obfuscate: false}      # aus, zum Fehlersuchen: der Quelltext ist normal lesbar
serve 8080, {obfuscate: "light"}    # nur der Quelltext ist verschlüsselt, nichts wird umbenannt
serve 8080, {keep: ["menu"]}        # diese Klassen- und ID-Namen bleiben, wie sie sind
```
Wenn eine Seite **eigenes JavaScript** hat, das Elemente über ihren Namen findet (`getElementById("menu")`, `querySelector("div")`),
findet es sie nicht mehr, solange die Namen umbenannt sind. Fa schreibt dann beim Start einen Hinweis. Dafür gibt es
`obfuscate: "light"` (alles läuft, nur der Quelltext ist verschlüsselt) oder `keep: [...]` mit den Namen, die dein Skript braucht.
Der Code-Editor im Browser (`editor/editor.fa`) benutzt `{code_root: false, obfuscate: "light"}`. In deinem CSS funktionieren Auswahlen wie `[class="box"]` nicht.

**Was im Inspektor lesbar bleibt:** `html`, `head`, `body`, `title`, `meta`, `link`, `style` (das gehört zu jeder Seite, und die Zeichen `<` und `>`
zeichnet der Inspektor des Browsers selbst), Elemente, die zum Funktionieren echt sein müssen (`a href`, `img src`, `input`, `label`, `button`, `form`),
IDs, auf die ein Anker oder eine Beschriftung zeigt (mit zufälligem Wert), und vor allem **der Text, den man auf der Seite sieht**.

**Wichtig und ehrlich:** Ein Browser muss die Seite am Ende lesen können, um sie anzuzeigen. Der Schutz macht es schwer, die Seite
zu verstehen und abzuschreiben, er ist kein echtes Geheimnis. Geheime Dinge (Passwörter, Schlüssel) gehören nie in eine Seite, sondern auf den Server.
Ohne JavaScript im Browser zeigt eine geschützte Seite nichts an.

Wie es funktioniert: Der Server schickt eine winzige Seite mit dem verschlüsselten Inhalt. Der Browser baut daraus die echte Seite und
ersetzt sich damit selbst, es bleibt also nichts Lesbares im Dokument. Das normale Aussehen der umbenannten Tags wird per CSS nachgebaut.

### Der Code im Inspektor: die Einstellungen im Einzelnen

Wie viel von deinem Code im Inspektor steht, und wo:

```
serve 8080, {show_code: true}      # der Code der Seite (der route-Block, der sie aufgebaut hat)
serve 8080, {show_code: "file"}    # die ganze .fa-Datei (das ist bei code_root schon der Standard)
```
**Wo man ihn sieht: im Reiter „Sources“.** Dort steht deine Datei (zum Beispiel `server.fa`) neben `(index)` im Dateibaum und öffnet sich als ganz normaler Code:
mit Zeilennummern, Farben und Einrückung, **ohne Anführungszeichen, ohne `<!--` und ohne Tags**. Die Zeilennummern stimmen mit denen in deiner Datei überein
(bei `true` stehen vor deinem `route` leere Zeilen, damit die Nummern passen).

Im Reiter „Elemente“ geht „ohne alles“ nicht, denn der Inspektor zeichnet dort jeden Knoten der Seite mit seiner Schreibweise: Text in Anführungszeichen,
Kommentare mit `<!--`, Elemente mit `<tag>`. Das lässt sich nicht abstellen. Wie nah man herankommt, steht im nächsten Abschnitt.

**Achtung:** Jeder Besucher kann den Code lesen. Das hebt den Seitenschutz teilweise auf (wer den Fa-Code sieht, versteht die Seite),
und bei `"file"` (der Standard) wird die **ganze Datei** an jeden Besucher geschickt, mit Passwörtern, Schlüsseln und Dateipfaden, falls du so etwas darin hast.
Mit `show_code: true` schickt Fa nur die Zeilen unter dem `route`, der die Seite gebaut hat, aber auch dort gehören keine Geheimnisse hin.
Fa schreibt beim Start eine Warnung.

#### Statt html, head und body: dein Code

`code_root` (Standard) lässt den Elemente-Baum nicht mit `<html>`, `<head>` und `<body>` anfangen. Das Wurzel-Element ist ein eigenes, `<fa-code>`:

```
serve 8080                                         # Standard (= {code_root: true}): Elemente: deine GANZE Datei als Textzeilen. Dazu die Datei im Reiter Sources.
serve 8080, {code_root: "sources"}                 # Elemente: nur das Nötigste, kein Code. Die ganze Datei steht nur im Reiter Sources.
serve 8080, {show_code: true}                      # nur der Code des route-Blocks statt der ganzen Datei
```
So sieht der Reiter „Elemente“ im Standard aus (kein `<!DOCTYPE html>`, kein `lang` an der Wurzel):
```
<fa-code>
  "# server.fa"
  "besucher = 0"
  " "
  "route "/""
  "    besucher += 1"
  "    page "Meine Seite""
  "        h1 "Hallo von Fa""
  …                                 (die ganze Datei, bis zur letzten Zeile)
  <title>Meine Seite</title>
  ▸ <fa-view> … </fa-view>
</fa-code>
```
und bei `"sources"`:
```
<fa-code>
  <title>Meine Seite</title>
  ▸ <fa-view> … </fa-view>
</fa-code>
```
Leere Zeilen deiner Datei erscheinen als eigene Zeile (`" "`). Die Anführungszeichen um jede Zeile setzt der Inspektor selbst, bei jedem Text. Das Wurzel-Element `<fa-code>` kann nicht fehlen, denn ein Dokument braucht eins.
Die Seite sieht pixelgenau gleich aus und funktioniert (Links, Zurück-Knopf des Browsers, Beschriftungen, Eingabefelder, Anker, Hintergrundfarbe, deine `.css`-Dateien).
Die Sprache der Seite (`lang`) steht dabei am `<body>` in der Seite, damit Screenreader sie weiter kennen.

**Achtung:** Standardmäßig bekommt jeder Besucher deine **ganze Datei**, mit allem, was darin steht (Passwörter, Schlüssel, Dateipfade). Fa warnt beim Start. Wer das nicht will, nimmt `show_code: true` (nur der `route`-Block) oder `code_root: false`.

Wie das geht: Die echte Seite liegt eine Ebene tiefer in `<fa-view>`, in einem geschlossenen Shadow-DOM (`#shadow-root (closed)`). Zwei Klicks auf die Pfeile machen sie sichtbar.
**Weg ist sie nicht**, denn der Browser braucht sie zum Anzeigen. Die Code-Zeilen im Elemente-Baum sind unsichtbar gemacht (`line-height:0`, durchsichtige Farbe).

Was in diesem Modus **nicht** geht:
- **Eigenes JavaScript** erreicht die Seite nicht (`document.getElementById` sieht nicht ins Shadow-DOM, und es gibt kein `document.body`). Fa schreibt dann einen Hinweis.
- **`@font-face`** (eigene Schriften aus deiner CSS) funktioniert im Shadow-DOM nicht.
- Auf **Handys** ist es nicht getestet (die `viewport`-Angabe der Seite fehlt).
- **Screenreader** lesen im Standard den unsichtbaren Code-Text mit vor (bei `"sources"` nicht).
- Ohne JavaScript im Browser bleibt die Seite leer.

### Code-Editor: im Browser und als Programm

Im Ordner `editor/` liegt ein Code-Editor, in zwei Ausführungen, beide mit Fa gebaut und mit denselben Dateien (`editor/workspace`):

```
fa editor/editor.fa        # im Browser:  http://localhost:8080
fa editor/app.fa           # als Programm mit eigenem Fenster (braucht sh install.sh gui)
```
Beide haben Dateiliste, Zeilennummern, Farben für Fa, Einrücken mit Tab / Shift+Tab, automatische Einrückung, Speichern (Strg+S),
Ausführen (Strg+Enter) mit der Ausgabe darunter, neue Dateien und Löschen.
Das Programm kann zusätzlich: Markieren mit Maus und Umschalt+Pfeilen, Doppelklick auf ein Wort, Kopieren / Ausschneiden / Einfügen,
Rückgängig / Wiederholen (Strg+Z / Strg+Y), Scrollen, ein Fenster, dessen Größe man ändern kann, und es speichert beim Schließen.
`editor.fa` ist der Server (Routen für Seite, Dateien, Speichern, Ausführen), `editor.js` und `editor.css` die Oberfläche im Browser;
`app.fa` ist das ganze Programm in Fa (rund 130 Zeilen).
Beide Server bzw. Fenster sind nur auf diesem Computer erreichbar, und Dateinamen dürfen nur aus Buchstaben, Ziffern, `_`, `-` und `.` bestehen.
„Ausführen“ startet die Datei mit Fa und stoppt sie nach 10 Sekunden (unter Windows ohne dieses Limit).

### Fenster

```
klicks = 0

window "Meine App"
    label "Bisher {klicks} Klicks"
    button "Klick mich"
        klicks += 1
    button "Zurücksetzen"
        klicks = 0
```
`label` zeigt Text, `button` hat darunter die Zeilen, die beim Klick laufen. Nach jedem Klick, Tastenkürzel oder jeder Änderung in einem Texteditor
wird das Fenster neu aufgebaut, es zeigt also immer die aktuellen Werte. Mit `for` und `if` kann man beliebig viele Zeilen erzeugen (`examples/aufgaben.fa`).

Für richtige Programme gibt es mehr Bausteine und Anordnung in Zeilen und Spalten:

| Baustein | Was er macht |
|---|---|
| `row` / `column` | ordnen die Bausteine darunter nebeneinander / untereinander an |
| `spacer` | leerer Raum, der die anderen auseinanderschiebt |
| `editor "name"` | ein Texteditor (Cursor, Markieren, Kopieren, Rückgängig, Farben), der seinen Text selbst behält |
| `textfield "name"` | eine einzelne Eingabezeile; darunter stehen die Zeilen, die bei Enter laufen |
| `textview text` | Text, der nicht geändert werden kann, mit Scrollen (z. B. eine Ausgabe) |
| `listbox liste` | eine Liste zum Anklicken; darunter stehen die Zeilen für einen Klick, dort ist `item` der gewählte Eintrag |
| `shortcut "ctrl+s"` | Tastenkürzel; darunter stehen die Zeilen, die laufen sollen |

Einstellungen stehen als Map hinter dem Baustein: `row {grow: 1, gap: 8, padding: 8, background: "#2d2d30"}`.
Es gibt `width`, `height`, `grow` (wie viel vom freien Platz), `gap`, `padding`, `align`, `background` und `color` (Farben als `"#rrggbb"`).
Dem Fenster selbst gibt man `width`, `height`, `background`, `color` und `accent`: `window "Titel", {width: 1100, height: 700}`.
Für `editor`: `language: "fa"` färbt Fa-Code ein. Für `listbox`: `selected: name` hebt einen Eintrag hervor.
Der Text eines Editors steht in Fa über Funktionen zur Verfügung:

```
editor_get "code"            # der Text
editor_set "code", text      # neuen Text hineinschreiben
editor_dirty "code"          # gibt es ungespeicherte Änderungen?
editor_clean "code"          # als gespeichert merken
editor_info "code"           # {line: 3, column: 7, lines: 20}
```
Der Programmtext zum Fenster beschreibt nur, was zu sehen ist; alles Wiederkehrende (Hover, Cursor, Markieren) macht Fa.
Fenster brauchen raylib: `sh install-raylib.sh`, dann `sh install.sh gui`.
Die Schrift ist die normale Systemschrift (mit Umlauten, Griechisch, Kyrillisch) und für den Editor die Festbreitenschrift des Systems.
Eigene wählst du mit `FA_FONT=/pfad/schrift.ttf` und `FA_MONO_FONT=/pfad/mono.ttf`.

## Aufbau des Projekts

```
src/main.cpp      Kommandozeile + Eingabe      (hier anfangen zu lesen)
src/lexer.h       Quelltext -> Tokens          src/parser.h    Tokens -> Syntaxbaum
src/interp.h      führt das Programm aus       src/methods.h   Text-, Listen-, Map-Methoden
src/stdlib.h      eingebaute Funktionen        src/server.h    Webserver (mit obfuscate.h: versteckt den Quelltext)
src/value.h ast.h objects.h text.h helpers.h html.h common.h    Bausteine
src/gui/          Fenster (raylib, optional)
editor/           der Code-Editor: im Browser (editor.fa, editor.js, editor.css) und als Programm (app.fa), workspace/ = die Dateien
examples/         hello.fa  server.fa  guess.fa  characters.fa  gui.fa  aufgaben.fa
```

## Was sich gegenüber v0.17 geändert hat

**Neuer Standard:** Ein einfaches `serve 8080` zeigt im Inspektor deine ganze `.fa`-Datei statt `html`, `head` und `body` (vorher: `{code_root: true, show_code: "file"}`).
Ausschalten mit `serve 8080, {code_root: false}`. Weil damit jeder Besucher die ganze Datei bekommt, schreibt Fa bei jedem Start eine Warnung.
Der Browser-Code-Editor (`editor/editor.fa`) hat eigenes JavaScript und nutzt darum `{code_root: false, obfuscate: "light"}`.

Davor (v0.15 bis v0.17): Dein Code steht als eigene Datei im Reiter „Sources“, im Modus `code_root` ohne `<!DOCTYPE html>` und `lang` an der Wurzel;
`code_root` zeigt die ganze Datei (vorher nur den `route`-Block); `code_root: "sources"` lässt die Textzeilen im Elemente-Baum weg.

## Später

Typen, Kompilieren zu einer eigenen ausführbaren Datei, größere Standardbibliothek, ein Modus für Betriebssystem-Code.
Grenzen von v0.18: ein Interpreter (langsamer als nativer Code), Rekursionstiefe 2000.
