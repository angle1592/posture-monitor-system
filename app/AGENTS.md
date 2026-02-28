# AGENTS.md
Operating guide for coding agents in this repository.
Source of truth is current files in this repo (not assumptions from other Vue projects).

## Project Snapshot
- Stack: `uni-app` + `Vue 3` + `TypeScript` + `Vite`
- Package manager: `npm` (`package-lock.json` exists)
- Main app code: `src/`
- TS path alias: `@/*` -> `src/*` (`tsconfig.json`)
- CI checks type safety on Node 20 (`.github/workflows/typecheck-node20.yml`)

## Rules Files (Cursor/Copilot)
- `.cursorrules`: not present
- `.cursor/rules/`: not present
- `.github/copilot-instructions.md`: not present
- If these files are added later, treat them as higher-priority instructions.

## Environment
Use Node 20 LTS to match CI.

```bash
npm ci
```

Fallback if lockfile install is not possible locally:

```bash
npm install
```

## Commands
All listed commands are verified from `package.json`.

### Development
- H5: `npm run dev:h5`
- App Plus: `npm run dev:app-plus`
- Weixin Mini Program: `npm run dev:mp-weixin`
- Additional targets are available as `dev:*` scripts.

### Build
- H5: `npm run build:h5`
- App Plus: `npm run build:app-plus`
- Weixin Mini Program: `npm run build:mp-weixin`
- Additional targets are available as `build:*` scripts.

### Type Check
- Main: `npm run type-check`
- Underlying command: `vue-tsc --noEmit`
- CI runs: `npm ci` + `npm run type-check`

### Lint
- Main: `npm run lint`
- Auto-fix: `npm run lint:fix`
- Config: `.eslintrc.cjs` + `.eslintignore`

### Test
- Main: `npm run test`
- Watch: `npm run test:watch`
- Runner: Vitest (`vitest.config.ts`)

#### Single Test Execution (Current State)
- Vitest single file: `npx vitest run src/path/to/file.test.ts`
- Vitest by test name: `npx vitest run -t "test name pattern"`

#### Single Test Execution (Future Fallback Patterns)
If Jest is later added:

```bash
npx jest path/to/file.test.ts
npx jest -t "test name pattern"
```

Jest commands above are fallback patterns only, not current repo defaults.

## Code Style and Conventions
### TypeScript
- `strict: true` is enabled; keep new code strict-type-safe.
- `moduleResolution: "Bundler"`, `target: "ESNext"`.
- Prefer explicit interfaces/types for payloads and state.
- Avoid new `any`; use unions, generics, or narrowed types.

### Vue / uni-app Patterns
- Use SFCs with `<script setup lang="ts">`.
- Prefer Composition API (`ref`, `reactive`, `computed`).
- Keep page logic in page files under `src/pages/**` unless shared.
- Use uni lifecycle hooks where appropriate (`onLaunch`, `onShow`, `onHide`).

### Imports
- Existing code uses both relative imports and `@/` alias imports.
- Prefer `@/` for cross-folder imports.
- Keep same-folder imports relative when clearer.
- Use `import type` for type-only imports.
- Preserve surrounding file style to keep diffs small.

### Naming
- Variables/functions: `camelCase`
- Interfaces/types: `PascalCase`
- Keep state names explicit (`monitoringEnabled`, `currentMode`, etc.)
- Prefer descriptive API function names (`queryDeviceProperty`, `setDeviceProperty`)

### State and API Layer
- Shared reactive store lives in `src/utils/store.ts`.
- Pattern: reactive state + computed derivations + exported actions.
- OneNET transport and request helpers live in `src/utils/oneNetApi.ts`.
- Keep IO/network/storage side effects centralized in store/api helpers.

### Error Handling and Logging
- Wrap async IO/network/storage with `try/catch`.
- Log with context prefixes (for example `[Store]`, `[OneNET]`).
- Use `uni.showToast` for user-actionable feedback in UI flows.
- Return safe fallback values where pattern already exists (`null`, `false`, `[]`).
- Do not silently swallow errors.

### Formatting and Styles
- Formatting is mixed (single quotes and double quotes both exist).
- Match local file style; avoid style-only churn.
- Keep indentation/spacing consistent with touched files.
- SCSS is used in SFCs (`<style lang="scss" scoped>`).
- `rpx` is the dominant unit for mobile-oriented layouts.

## Security Notes
- Never commit real secrets/tokens.
- `src/utils/oneNetApi.ts` currently contains a token-like value; treat as sensitive debt.
- Do not duplicate that pattern in new files.

## Agent Workflow Checklist
Before editing:
1. Read nearby files and follow existing conventions.
2. Confirm platform target when running `dev:*` / `build:*` scripts.
3. Make minimal, surgical changes.

After editing TS/Vue logic:
1. Run `npm run type-check`.
2. Update docs/scripts if you introduced tooling.
3. Summarize behavior changes and any risk/follow-up.

## Do Not Assume
- Do not assume a single deployment target; this is multi-platform `uni-app`.
- Do not assume a formatter is enforcing style; preserve local patterns intentionally.

## If You Add Lint/Test Tooling
Update all of these together:
1. `package.json` scripts
2. Tool config files (Vitest/Jest/ESLint/Prettier, etc.)
3. CI workflow updates under `.github/workflows/`
4. This `AGENTS.md` (including single-test instructions)
