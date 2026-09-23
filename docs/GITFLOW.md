# Team & gitflow

## Team model

Three members, three parallel iterations. Each member builds their own
v0.1 (implant + controller approach) on their own branch; the team
compares results, picks the strongest base, and merges it into `develop`.
From then on, everyone contributes to the chosen codebase via feature
branches.

| Member | Branch | Role after merge |
|--------|--------|------------------|
| Hugo   | `Hugo`  | implant (C): stealth, persistence, syscalls |
| Lucas  | `lucas` | controller (Python): sessions, crypto, command routing |
| (3rd)  | `ghul`| tradecraft: extended commands, MITRE mapping, docs/demo |

Roles rotate on later milestones so everyone touches both sides.

## Branch structure

```
main ───────●──────────────────────────►  protected: demo-ready milestones only
             \
develop ──────●───●───●───●────────────►  integration: merged, working code
               \     \
feature/x ───── ●───●──┘                 short-lived, one capability each
Hugo / lucas / ghul                    personal v0.1 iterations
```

Rules:

- `main` is tagged at each milestone (`v0.1-brut`, `v0.2-tls`, ...).
- Nothing merges to `develop` without building and running in the lab.
- Feature branches: `feature/<short-name>` off `develop`, merged by PR
  with one teammate review.
- Every member commits at least twice (project requirement) - small,
  frequent, meaningful commits.
- Commit messages: imperative, scoped - e.g. `implant: add reconnect loop`,
  `controller: drain socket with select timeout`.

## Project management

GitHub Projects board (already chosen), columns:

`Backlog → In progress → In review → Done`

One card per capability from the subject (reverse shell, tunnel, creds,
persistence, crypto, log rollback, each extended command), each card
carrying its MITRE ID and its blue-team note when closed. Cards are
assigned to a member and linked to the PR that closes them.

## Milestones

| Milestone | Content | Date |
|-----------|---------|------|
| Follow-up 1 | roles, git, PM tool, architecture, v0.1 raw shell, README | tomorrow |
| v0.2 | TLS, framing, multi-session | TBD |
| v0.3 | persistence + resilience | TBD |
| Final | all core + chosen extended commands, slides, live demo | TBD |
