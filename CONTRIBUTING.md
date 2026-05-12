# Contributing to Nocturne OS

Thank you for your interest in contributing! This document provides guidelines for participating in the project.

## License

All contributions must comply with **GNU AGPL v3.0**. By submitting code, you agree to license your contributions under AGPL-3.0.

## Development Setup

### Prerequisites

- PlatformIO CLI or VS Code extension
- Heltec WiFi LoRa 32 V4 board (for hardware testing)
- Git

### Local Development

1. **Clone and setup:**
   ```bash
   git clone https://github.com/therudywolf/Heltec-V3-V4-PC-COMPANION.git
   cd Heltec-V3-V4-PC-COMPANION
   cp include/secrets.h.example include/secrets.h
   ```

2. **Choose a build profile:**
   ```bash
   pio run -e bmw_only -t upload      # BMW I-Bus only
   pio run -e pc_companion -t upload  # PC monitoring + BMW + Forza
   pio run -e full -t upload          # All features
   ```

## Code Style

- **C++**: camelCase functions, PascalCase classes, explicit types
- **Dart**: Use `flutter format`
- **Python**: PEP 8 compliant

## Pull Request Process

1. Test all three build profiles compile cleanly
2. Use conventional commits: `feat:`, `fix:`, `docs:`, `refactor:`
3. No personal data, tokens, or hardcoded credentials
4. Update documentation if needed
5. All PRs require code review before merge

## License

GNU AGPL v3.0 - See LICENSE file for details.
