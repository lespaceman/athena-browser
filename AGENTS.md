# Repository Guidelines

## Project Structure & Module Organization
- app/: C++17 CEF host app (CMake target `athena-browser`).
- frontend/: React + Vite + TypeScript UI (`src/`, `index.html`).
- resources/web/: Production web assets copied from `frontend/dist`.
- scripts/: Developer helpers (`build.sh`, `dev.sh`, `build.ps1`).
- cmake/: CEF configuration helper; `CMakePresets.json` drives builds.
- third_party/: Place unpacked CEF at `third_party/cef_binary_${CEF_VERSION}_${CEF_PLATFORM}`.

## Build, Test, and Development Commands
- Build (Release): `./scripts/build.sh` — builds frontend, configures CMake preset `release`, copies web assets.
- Dev (HMR): `./scripts/dev.sh` — starts Vite and launches native app pointing at `DEV_URL=http://localhost:5173`.
- Manual CMake: `cmake --preset debug|release` then `cmake --build --preset debug|release -j`.
- Frontend only: `cd frontend && npm install && npm run dev|build|preview`.

## Coding Style & Naming Conventions
- C++: C++17, warnings enabled (`-Wall -Wextra -Wpedantic`). Indent 2 spaces; no tabs.
  - Classes: PascalCase (e.g., `AppHandler`). Methods: PascalCase; private members end with `_` (e.g., `browser_`). Constants: ALL_CAPS.
- TypeScript/React: 2-space indent. Components PascalCase (`App.tsx`), variables/methods camelCase. Prefer function components and hooks.
- Paths: Use forward slashes in logs/docs; keep includes local and minimal.

## Testing Guidelines
- No tests are set up yet. Recommended:
  - C++: GoogleTest with a `tests/` target in CMake; name files `*_test.cc`.
  - Frontend: Vitest + Testing Library; co-locate as `src/**/*.test.tsx`.
- Aim for smoke tests (app boot, scheme handler) and UI render tests. Add CI later.

## Commit & Pull Request Guidelines
- Commits: concise imperative subject (≤72 chars), body for rationale. Prefer Conventional Commits (e.g., `feat: add custom app:// scheme`).
- PRs: clear description, scope, and testing notes; link issues; include screenshots/gifs for UI changes; note platform impacts (Win/macOS/Linux).

## Security & Configuration Tips
- CEF: Place locales and ICU files next to the binary; ensure `resources_dir_path` and `locales_dir_path` resolve at runtime.
- CSP: `app://` scheme serves `resources/web` with a strict CSP in prod; keep dev allowances limited to Vite HMR.
- Config: Set `DEV_URL` for dev runs; do not hardcode secrets. Keep `third_party/` contents pinned to `CEF_VERSION` in `CMakeLists.txt`.

