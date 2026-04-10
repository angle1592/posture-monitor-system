# App Mock History and Realtime Data Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an environment-variable-controlled mock data source so the App can preview rich realtime and history UI without waiting for real OneNET data.

**Architecture:** Keep mock logic out of pages and route it through `oneNetApi.ts`. A dedicated `mockData.ts` module will generate repeatable realtime properties and 7-day posture history, while `store` and pages continue consuming the same API surface.

**Tech Stack:** uni-app, Vue 3, TypeScript, Vitest, environment-variable feature flags.

---

## File Map

- Create: `app/src/utils/mockData.ts`
  Purpose: generate mock realtime properties, device online state, and 7-day posture history.
- Modify: `app/src/utils/oneNetApi.ts`
  Purpose: branch the data source when `VITE_USE_MOCK_HISTORY=true`.
- Modify: `app/src/utils/oneNetApi.test.ts`
  Purpose: verify the mock branch returns synthetic realtime and history data.
- Modify: `app/.env.example`
  Purpose: document the mock feature flag for local development.

---

### Task 1: Add failing tests for mock mode routing

**Files:**
- Modify: `app/src/utils/oneNetApi.test.ts`

- [ ] **Step 1: Write the failing mock-mode tests**

Add tests that assert:

```ts
it('returns mock realtime properties when VITE_USE_MOCK_HISTORY=true', async () => {
  // expect postureType/personPresent/fillLightOn/currentMode properties
})

it('returns mock posture history when VITE_USE_MOCK_HISTORY=true', async () => {
  // expect multiple postureType history points including no_person and abnormal values
})
```

- [ ] **Step 2: Run the single test file to verify it fails**

Run: `npx vitest run src/utils/oneNetApi.test.ts`

Expected: FAIL because no mock branch exists yet.

- [ ] **Step 3: Commit**

Do not commit unless the user explicitly asks.

---

### Task 2: Implement the repeatable mock data provider

**Files:**
- Create: `app/src/utils/mockData.ts`

- [ ] **Step 1: Write the minimal generator module**

Create a focused module exporting functions like:

```ts
export function getMockDeviceStatus(): boolean
export function getMockRealtimeProperties(now?: number): PropertyItem[]
export function getMockPropertyHistory(identifier: string, days: number): HistoryDataPoint[]
```

Requirements:

- realtime properties include `postureType`, `personPresent`, `fillLightOn`, `monitoringEnabled`, `currentMode`
- posture history covers the last 7 days
- generated values include `normal/head_down/hunchback/no_person`
- data is repeatable, not random per call

- [ ] **Step 2: Re-read the new module**

Confirm the module does not import pages or store, and only focuses on mock generation.

- [ ] **Step 3: Commit**

Do not commit unless the user explicitly asks.

---

### Task 3: Wire the environment flag into oneNetApi

**Files:**
- Modify: `app/src/utils/oneNetApi.ts`
- Modify: `app/src/utils/oneNetApi.test.ts`
- Modify: `app/.env.example`

- [ ] **Step 1: Implement the smallest environment switch**

Add a flag like:

```ts
const USE_MOCK_HISTORY = import.meta.env.VITE_USE_MOCK_HISTORY === 'true'
```

Then branch:

```ts
if (USE_MOCK_HISTORY) {
  return getMockRealtimeProperties()
}
```

for realtime properties, device status, and history.

- [ ] **Step 2: Document the flag in `.env.example`**

Add:

```env
VITE_USE_MOCK_HISTORY=false
```

with a short comment that enabling it routes realtime and history pages to local mock data.

- [ ] **Step 3: Re-run the mock API test file**

Run: `npx vitest run src/utils/oneNetApi.test.ts`

Expected: PASS.

- [ ] **Step 4: Commit**

Do not commit unless the user explicitly asks.

---

### Task 4: Verify App behavior with mock mode enabled

**Files:**
- No additional source edits required unless verification reveals issues

- [ ] **Step 1: Run the full App test suite**

Run:

```bash
npm test
```

Expected: PASS.

- [ ] **Step 2: Run type-check**

Run:

```bash
npm run type-check
```

Expected: PASS.

- [ ] **Step 3: Record manual verification still required**

Note the manual checks:

- realtime page cycles through normal/head_down/hunchback/no_person in mock mode
- history page shows 7-day curve and daily detail curve
- daily detail gaps appear for no_person periods
- switching 5/10/15 minute buckets changes the detail curve density

- [ ] **Step 4: Commit**

Do not commit unless the user explicitly asks.
