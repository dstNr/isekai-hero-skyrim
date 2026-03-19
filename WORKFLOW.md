# Development Workflow — Isekai Hero

**Wichtig:** Diese Regeln gelten für ALLE Entwicklungssessions.

---

## 🔄 Git Workflow (Pflicht!)

### Vor dem Entwickeln (immer!)

```bash
# 1. Aktuellen Stand holen
git pull origin main

# 2. Prüfen ob alles passt
git status
```

**Warum:** Vermeidet Merge-Konflikte und stellt sicher, dass wir auf dem aktuellen Stand arbeiten.

---

### Nach Änderungen (immer!)

```bash
# 1. Alle Änderungen stagen
git add -A

# 2. Mit beschreibendem Commit-Message committen
git commit -m "type: Kurzbeschreibung

- Detaillierte Änderung 1
- Detaillierte Änderung 2
- Detaillierte Änderung 3"

# 3. Auf GitHub pushen
git push origin main
```

**Commit-Message Format:**
- `feat:` Neue Features
- `fix:` Bugfixes
- `docs:` Dokumentation
- `refactor:` Code-Verbesserungen ohne Funktionsänderung
- `chore:` Wartung, Setup, etc.

---

## 📋 Checkliste vor jedem Commit

- [ ] `git pull` ausgeführt?
- [ ] Alle Dateien gespeichert?
- [ ] Scripts syntaktisch korrekt?
- [ ] Commit-Message beschreibt die Änderungen?
- [ ] Getestet (wenn möglich)?

---

## 🚫 Nie vergessen!

**Immer committen und pushen nach:**
- Jeder signifikanten Änderung
- Bugfixes
- Neuen Features
- Dokumentations-Updates

**Nie vergessen vor dem Entwickeln:**
- `git pull origin main`

---

## 📝 Für Alfred (KI-Assistent)

**Bei jeder Session:**
1. Erst `git pull origin main` ausführen
2. Dann Änderungen vornehmen
3. Am Ende committen und pushen
4. Zusammenfassung der Änderungen geben

**Diese Datei liegt unter:** `WORKFLOW.md`