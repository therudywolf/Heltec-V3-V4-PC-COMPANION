# Security Policy

## Reporting

Report security issues privately via
[GitHub Security Advisories](https://github.com/therudywolf/Heltec-V3-V4-PC-COMPANION/security/advisories/new).
Do not open public issues for vulnerabilities.

## Secrets

- Wi‑Fi and PC connection settings: copy `include/secrets.h.example` to `include/secrets.h` locally only.
- Never commit `include/secrets.h` or real Wi‑Fi passwords.

## Scope

ESP32 firmware and companion tooling (PlatformIO, optional Flutter app, Python monitor).
