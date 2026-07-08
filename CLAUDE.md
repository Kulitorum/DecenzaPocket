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

## Release

- Version is set once: `project(DecenzaPocket VERSION X.Y.Z ...)` in `CMakeLists.txt`
  (iOS `MARKETING_VERSION` and Android `ANDROID_VERSION_NAME` derive from it; QML reads
  it via the `AppVersion` context property). Build number lives in `versioncode.txt`,
  auto-bumped by CI on each `v*` tag.
- Cut a release: bump the version → commit → `git pull --rebase` → tag `vX.Y.Z` → push
  the tag. CI (`.github/workflows/{ios,android}-release.yml`) builds and uploads to
  App Store Connect / GitHub Releases.
- **iOS CI needs the iOS 26 SDK (Xcode 26+):** `ios-release.yml` runs on `macos-26`,
  pins Xcode 26.4.1, and shims clang to unset `*_DEPLOYMENT_TARGET` env vars (mirrors
  Decenza's workflow — see its comments). `PRIVACY.md` + `ios/PrivacyInfo.xcprivacy`
  are required for App Store review.
- App Store screenshots: run the Windows build with `--ss WxH[@S]`; in the window,
  F4–F8 snap to device sizes and F12 saves a PNG (RGB, no alpha) — see `src/main.cpp`.

## Conventions

Same as Decenza:
- Classes: `PascalCase`, Methods/variables: `camelCase`, Members: `m_` prefix
- QML files: `PascalCase.qml`, IDs/properties: `camelCase`
- Never use timers as guards/workarounds
- Never run I/O on the main thread
