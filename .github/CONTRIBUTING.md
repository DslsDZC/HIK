# Contributing

PRs welcome. Keep it simple.

## Before submitting

- Run `make` and make sure it compiles
- Run `make img-run` and check `qemu_serial.log` ends with `[MOD_MGR] Entering idle loop`
- Check `qemu_debug.log` has no `check_exception` or `Triple fault`
- For tests: `make -f Makefile.test test`
- Commit messages follow Conventional Commits

## What I probably won't merge

- Cosmetic-only changes (reformatting, renaming)
- Adding deps just for one helper function
- PRs that touch 20+ files without a clear reason
