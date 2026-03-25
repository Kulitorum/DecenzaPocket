# DecenzaPocket

Companion phone app for Decenza espresso machine controller. iOS + Android only.

## Development Environment

- **Qt version**: 6.10.2
- **Qt path (Windows)**: `C:/Qt/6.10.2/msvc2022_64`
- **C++ standard**: C++17
- **Related projects**:
  - Decenza (tablet app): `C:\CODE\Decenza`
  - AWS backend: `C:\CODE\decenza.coffee`
- **IMPORTANT**: Use relative paths (e.g., `src/main.cpp`) to avoid edit errors

## Build

Don't build automatically - let the user build in Qt Creator.

## Conventions

Same as Decenza:
- Classes: `PascalCase`, Methods/variables: `camelCase`, Members: `m_` prefix
- QML files: `PascalCase.qml`, IDs/properties: `camelCase`
- Never use timers as guards/workarounds
- Never run I/O on the main thread
