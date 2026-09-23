# Team & gitflow

## Team model

Three members. Work is split by responsibility area, not by parallel rewrites: everyone contributes to the same codebase through feature branches, reviewed by a teammate.

| Member | Primary area |
|--------|--------------|
| Hugo   | implant (C): stealth, persistence, syscalls |
| Lucas  | controller (Python): sessions, crypto, command routing |
| (3rd)  | tradecraft: extended commands, MITRE mapping, docs/demo |

Roles rotate on later milestones so everyone touches both sides.

## Branch structure

```
main        <-------------------------  protected: demo-ready milestones only
  ^                                     only develop may merge in (PR + 1 review)
develop   <---------------------        protected: integration, always builds and runs in the lab
  ^                                     only testing may merge in (PR + 1 review)
testing   <-------------------          validation: feature work is demoed in the lab here
  ^                                     feature branches merge in (PR + 1 review)
feature/x ---< per capability >---      short-lived, one capability each, off testing
```

Flow: `feature/<name>` -> PR to `testing` -> validate in the lab -> PR to `develop` -> PR to `main` at milestones.

## Rules

- Nothing is pushed directly to `main`, `develop`, or `testing` - everything goes through a pull request.
- Every PR requires 1 approving review from a teammate (not the author) before merging.
- `main` only accepts PRs from `develop`; `develop` only accepts PRs from `testing`. Enforced by the `pr-gate` workflow + required status check (see below).
- `main` is tagged at each milestone (`v0.1-brut`, `v0.2-tls`, ...).
- Nothing merges to `develop` without having run in the lab on `testing` first.
- Every member commits at least twice (project requirement) - small, frequent, meaningful commits.
- Commit messages: imperative, scoped - e.g. `implant: add reconnect loop`, `controller: drain socket with select timeout`.

## Enforcement setup (GitHub)

Branch protection / rulesets on `main`, `develop`, `testing`:

- Require a pull request before merging
- Require 1 approval
- Require status check `pr-source-gate` to pass (blocks feature branches from skipping a level)
- Restrict who can push: nobody (admins keep bypass for emergencies)

The `pr-source-gate` check lives in `.github/workflows/pr-gate.yml`.

## Project management

GitHub Projects board, columns: `Backlog -> In progress -> In review -> Done`. One card per capability from the subject, each carrying its MITRE ID and blue-team note when closed. Cards are assigned to a member and linked to the PR that closes them. Issues auto-add to the board via the project's Auto-add workflow.

## Milestones

| Milestone | Content | Date |
|-----------|---------|------|
| Follow-up 1 | roles, git, PM tool, architecture, v0.1 raw shell, README | done |
| v0.2 | TLS, framing, multi-session | TBD |
| v0.3 | persistence + resilience | TBD |
| Final | all core + chosen extended commands, slides, live demo | TBD |
