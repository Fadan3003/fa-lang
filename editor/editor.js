// Fa Editor - die Oberfläche im Browser. Der Server dazu ist in Fa geschrieben (editor.fa).
(() => {
  const $ = (id) => document.getElementById(id);
  const code = $('code'), hl = $('hl'), gutter = $('gutter'), output = $('output');
  const filesEl = $('files'), nameEl = $('name'), posEl = $('pos'), stateEl = $('state');
  const btnNew = $('new'), btnSave = $('save'), btnRun = $('run'), btnDelete = $('delete');

  let current = null;   // Name der geöffneten Datei
  let dirty = false;    // gibt es ungespeicherte Änderungen?
  let names = [];       // alle Dateien

  // ---------- Farben (Syntax-Highlighting für Fa) ----------
  const KEYWORDS = new Set('def return if else while for in break continue true false nothing and or not'.split(' '));
  const BUILTINS = new Set(('print ask len text number chr ord sqrt round floor abs min max random ' +
    'read_file write_file file_exists list_files delete_file run env wait now exit args ' +
    'route serve status content_type json page document h1 h2 h3 p div span ul ol li pre bold italic code ' +
    'link image input br hr tag raw window label button here interpreter os').split(' '));
  const TOKEN = /(#[^\n]*)|("(?:[^"\\\n]|\\.)*"?)|(\b\d+(?:\.\d+)?\b)|([A-Za-z_\u00C0-\uFFFF][\w\u00C0-\uFFFF]*)/g;

  const esc = (s) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  const span = (cls, s) => '<span class="' + cls + '">' + s + '</span>';

  function highlightFa(src) {
    let out = '', last = 0, m;
    TOKEN.lastIndex = 0;
    while ((m = TOKEN.exec(src)) !== null) {
      out += esc(src.slice(last, m.index));
      const t = m[0];
      if (m[1]) out += span('c', esc(t));
      else if (m[2]) out += span('s', esc(t).replace(/\{[^{}\n]*\}/g, (v) => span('i', v)));
      else if (m[3]) out += span('n', t);
      else if (KEYWORDS.has(t)) out += span('k', t);
      else if (BUILTINS.has(t)) out += span('f', t);
      else out += esc(t);
      last = m.index + t.length;
    }
    return out + esc(src.slice(last));
  }

  // ---------- Anzeige ----------
  function syncScroll() {
    hl.scrollTop = code.scrollTop; hl.scrollLeft = code.scrollLeft;
    gutter.scrollTop = code.scrollTop;
  }

  function showPos() {
    const before = code.value.slice(0, code.selectionStart);
    const line = before.split('\n').length;
    const col = before.length - before.lastIndexOf('\n');
    posEl.textContent = 'Zeile ' + line + ', Spalte ' + col;
  }

  function update() {
    const isFa = current !== null && current.endsWith('.fa');
    hl.innerHTML = (isFa ? highlightFa(code.value) : esc(code.value)) + '\n';
    let nums = '';
    const lines = code.value.split('\n').length;
    for (let i = 1; i <= lines; i++) nums += i + '\n';
    gutter.textContent = nums;
    syncScroll();
    showPos();
  }

  function setDirty(d) {
    dirty = d;
    stateEl.textContent = current === null ? '' : d ? 'Nicht gespeichert ●' : 'Gespeichert ✓';
  }

  function refreshButtons() {
    const none = current === null;
    code.disabled = none;
    btnSave.disabled = none;
    btnDelete.disabled = none;
    btnRun.disabled = none || !current.endsWith('.fa');
  }

  const onEdit = () => { setDirty(true); update(); };

  // ---------- Server ----------
  async function api(url, options) {
    const res = await fetch(url, options);
    const text = await res.text();
    if (!res.ok) throw new Error(text || res.statusText);
    return text;
  }
  const q = (name) => '?file=' + encodeURIComponent(name);

  async function loadFiles() {
    names = (await api('/files')).split('\n').filter(Boolean);
    filesEl.textContent = '';
    for (const n of names) {
      const li = document.createElement('li');
      li.textContent = n;
      if (n === current) li.className = 'active';
      li.onclick = () => openFile(n);
      filesEl.appendChild(li);
    }
  }

  async function openFile(name) {
    if (dirty && !confirm('Es gibt ungespeicherte Änderungen. Trotzdem wechseln?')) return;
    try {
      code.value = await api('/open' + q(name));
      current = name;
      nameEl.textContent = name;
      setDirty(false);
      refreshButtons();
      update();
      await loadFiles();
      code.focus();
      code.setSelectionRange(0, 0);
    } catch (e) { alert(e.message); }
  }

  async function save() {
    if (current === null) return;
    try {
      await api('/save' + q(current), { method: 'POST', body: code.value });
      setDirty(false);
    } catch (e) { alert('Speichern fehlgeschlagen: ' + e.message); }
  }

  async function run() {
    if (current === null || !current.endsWith('.fa')) return;
    await save();
    output.textContent = 'läuft …';
    try { output.textContent = await api('/run' + q(current)); }
    catch (e) { output.textContent = e.message; }
  }

  async function newFile() {
    const name = (prompt('Name der neuen Datei (z. B. test.fa):') || '').trim();
    if (!name) return;
    if (!/^[A-Za-z0-9_-][A-Za-z0-9_.-]*$/.test(name) || name.includes('..') || name.length > 60) {
      alert('Erlaubt sind nur Buchstaben, Ziffern, _ - und . (nicht am Anfang).');
      return;
    }
    try {
      if (!names.includes(name)) await api('/save' + q(name), { method: 'POST', body: '' });
      await openFile(name);
    } catch (e) { alert(e.message); }
  }

  async function remove() {
    if (current === null || !confirm('„' + current + '“ wirklich löschen?')) return;
    try {
      await api('/delete' + q(current));
      current = null; dirty = false;
      code.value = ''; nameEl.textContent = 'keine Datei';
      setDirty(false); refreshButtons(); update();
      await loadFiles();
    } catch (e) { alert(e.message); }
  }

  // ---------- Tastatur ----------
  function replaceRange(start, end, text, selStart, selEnd) {
    code.focus();
    code.setSelectionRange(start, end);
    if (!document.execCommand || !document.execCommand('insertText', false, text)) {
      code.setRangeText(text, start, end, 'end');       // ohne Rückgängig-Verlauf, aber sicher
    }
    code.setSelectionRange(selStart, selEnd);
    onEdit();
  }

  code.addEventListener('keydown', (e) => {
    const v = code.value, s = code.selectionStart, t = code.selectionEnd;
    const strg = e.ctrlKey || e.metaKey;

    if (strg && e.key.toLowerCase() === 's') { e.preventDefault(); save(); return; }
    if (strg && e.key === 'Enter') { e.preventDefault(); run(); return; }

    if (e.key === 'Tab') {                               // Tab: einrücken, Shift+Tab: ausrücken
      e.preventDefault();
      if (s === t && !e.shiftKey) { replaceRange(s, t, '    ', s + 4, s + 4); return; }
      const end = (t > s && v[t - 1] === '\n') ? t - 1 : t;
      const lineStart = v.lastIndexOf('\n', s - 1) + 1;
      let lineEnd = v.indexOf('\n', end);
      if (lineEnd === -1) lineEnd = v.length;
      const lines = v.slice(lineStart, lineEnd).split('\n');
      const out = lines.map((l) => e.shiftKey ? l.replace(/^( {1,4}|\t)/, '') : '    ' + l);
      const text = out.join('\n');
      const delta = out[0].length - lines[0].length;
      replaceRange(lineStart, lineEnd, text, Math.max(lineStart, s + delta), lineStart + text.length);
      return;
    }

    if (e.key === 'Enter' && !e.shiftKey && !e.altKey && !strg) {   // Enter: Einrückung übernehmen
      e.preventDefault();
      const lineStart = v.lastIndexOf('\n', s - 1) + 1;
      const line = v.slice(lineStart, s);
      let indent = line.match(/^ */)[0];
      const trimmed = line.trim();
      if (/^(def|if|else|while|for)\b/.test(trimmed) || trimmed.endsWith(':')) indent += '    ';
      const pos = s + 1 + indent.length;
      replaceRange(s, t, '\n' + indent, pos, pos);
    }
  });

  code.addEventListener('input', onEdit);
  code.addEventListener('scroll', syncScroll);
  ['keyup', 'click', 'select'].forEach((ev) => code.addEventListener(ev, showPos));
  btnNew.onclick = newFile;
  btnSave.onclick = save;
  btnRun.onclick = run;
  btnDelete.onclick = remove;
  window.addEventListener('beforeunload', (e) => { if (dirty) { e.preventDefault(); e.returnValue = ''; } });

  // ---------- Start ----------
  refreshButtons();
  update();
  loadFiles().then(() => { if (names.length) openFile(names[0]); }).catch((e) => { output.textContent = e.message; });
})();
