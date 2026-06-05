# Governance

## PR workflow

All changes go through PRs:

1. **Automated tests must pass**
   - CI runs: build (x86_64 + ARM64 + STM32) → unit tests → QEMU boot test → malware scan
   - If any fails, the PR is blocked from review

2. **Code review**
   - A maintainer reviews the code

3. **Merge**
   - Maintainer merges into main

## Becoming a maintainer

- Submit good PRs, participate in reviews
- Proven contributors get added to CODEOWNERS
- Current sole maintainer: DslsDZC
