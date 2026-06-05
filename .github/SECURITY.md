# Security

Report vulnerabilities to dsls.dzc@gmail.com, or open an issue with the `security` label.

## Trust model

Only Core-0 is trusted. Everything else can crash and we don't treat it as a security bug.

| Layer | Runs | Trusted? |
|-------|------|----------|
| Core-0 | Kernel, capability system, PMM | Yes |
| Privileged-1 | Filesystem, drivers, services | No |
| Application-3 | User code | No |

Cross-domain operations require capability checks. Formal invariant checks run at boot.

## What's in scope

- Core-0 privilege escalation
- Capability forgery or unauthorized derivation
- Domain isolation bypass
- Unauthorized IPC access

Out of scope:
- Side channels (timing, cache)
- Physical attacks (DMA, JTAG, cold boot)
- Non-TCB layer bugs (but feel free to report them anyway)

## Response

Ack within 48h. Fix within ~2 weeks for serious issues. Public disclosure after merge to main.
