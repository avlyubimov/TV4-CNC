# CI dependency lock verification

The firmware workflow previously completed compilation and binary validation, then failed
at `git diff --exit-code -- firmware/esp32-hmi/locks/` with Git exit code 129:
`Not a git repository`. See `evidence/build/ci-lock-check.txt` for the original log excerpt.

The workflow now snapshots the complete lock directory before building and compares
it with `diff -ru` afterwards. Changed, added and deleted files all fail the job.
The check is independent of Git repository discovery inside the SDK container;
it does not disable lock validation or change dependencies, firmware or buffering.

Acceptance: all four ESP-IDF 5.3.2/6.1.1 × headless/rev12 jobs must pass,
including binary configuration validation and unchanged dependency locks.
Host logic/render tests and documentation checks must also pass.
Hardware remains untested; no device access or flashing is part of this change.

Local regression checks accepted unchanged locks and rejected changed, added and
deleted files. All four CTest tests passed. Remote results are attached to
[PR #2](https://github.com/avlyubimov/TV4-CNC/pull/2).
Rollback: revert the CI fix commits; firmware binaries are unaffected.
