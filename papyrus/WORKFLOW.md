> ⚠️ **Legacy / archived.** This documents the old Papyrus implementation of the mod
> (tag `papyrus-v1.0`), which has been superseded by the native SKSE C++ version. Kept
> for reference only.

# Development Workflow — Isekai Hero

**Important:** these rules apply to ALL development sessions.

---

## 🔄 Git workflow (mandatory!)

### Before developing (always!)

```bash
# 1. Get the current state
git pull origin main

# 2. Check that everything is fine
git status
```

**Why:** avoids merge conflicts and makes sure we work on the current state.

---

### After changes (always!)

```bash
# 1. Stage all changes
git add -A

# 2. Commit with a descriptive commit message
git commit -m "type: short description

- Detailed change 1
- Detailed change 2
- Detailed change 3"

# 3. Push to GitHub
git push origin main
```

**Commit-message format:**
- `feat:` new features
- `fix:` bugfixes
- `docs:` documentation
- `refactor:` code improvements without a behavior change
- `chore:` maintenance, setup, etc.

---

## 📋 Checklist before every commit

- [ ] Ran `git pull`?
- [ ] All files saved?
- [ ] Scripts syntactically correct?
- [ ] Commit message describes the changes?
- [ ] Tested (if possible)?

---

## 🚫 Never forget!

**Always commit and push after:**
- Every significant change
- Bugfixes
- New features
- Documentation updates

**Never forget before developing:**
- `git pull origin main`

---

## 📝 For the assistant

**Every session:**
1. Run `git pull origin main` first
2. Then make changes
3. Commit and push at the end
4. Give a summary of the changes

**This file lives at:** `WORKFLOW.md`
