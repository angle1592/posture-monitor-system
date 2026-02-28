# uni-vue3-ts

Mobile-side control and monitoring app built with uni-app + Vue 3 + TypeScript. The app presents device status, history views, and remote control interactions for the posture monitoring system.

## Stack

- uni-app
- Vue 3
- TypeScript
- Vite

## Directory Overview

```text
uni-vue3-ts/
├── src/
│   ├── pages/
│   │   ├── index/
│   │   ├── monitor/
│   │   ├── history/
│   │   └── control/
│   ├── utils/
│   │   ├── oneNetApi.ts
│   │   ├── store.ts
│   │   └── mqtt.js
│   ├── App.vue
│   ├── main.ts
│   ├── pages.json
│   └── manifest.json
├── package.json
├── tsconfig.json
└── vite.config.ts
```

## Requirements

- Node.js 20 LTS (recommended to match CI)
- npm

## Install

```bash
npm ci
```

If lockfile install is not available:

```bash
npm install
```

## Common Commands

```bash
npm run dev:h5
npm run dev:app-plus
npm run dev:mp-weixin
npm run build:h5
npm run build:app-plus
npm run build:mp-weixin
npm run lint
npm run test
npm run type-check
```

## Notes

- Lint command: `npm run lint`
- Test command: `npm run test` (currently supports empty test sets via `--passWithNoTests`)
- Keep credentials and tokens out of repository files.
- Prefer type-safe changes (`strict` TypeScript project).

## API Docs

- OneNET API interface document: `docs/api/oneNet-api.md`
