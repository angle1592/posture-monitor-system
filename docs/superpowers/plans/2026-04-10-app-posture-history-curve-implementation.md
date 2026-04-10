# App Posture Protocol Alignment and History Curve Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Align the App with the new `postureType/personPresent/fillLightOn` protocol and replace the history page bar chart with 7-day and daily Bezier curves.

**Architecture:** Move the App's posture semantics into `store.ts` and a small `historyChart.ts` utility module, so history scoring, aggregation, and SVG path preparation are shared instead of embedded inside the page. Keep page edits focused on rendering and interaction while preserving the existing uni-app page structure.

**Tech Stack:** uni-app, Vue 3 `<script setup>`, TypeScript strict mode, Vitest, native SVG rendering.

---

## File Map

- Modify: `app/src/utils/constants.ts`
  Purpose: switch active posture/property constants to the new protocol.
- Modify: `app/src/utils/store.ts`
  Purpose: fetch and derive posture state from `postureType/personPresent/fillLightOn/currentMode`, unify scoring semantics.
- Create: `app/src/utils/historyChart.ts`
  Purpose: normalize posture history, build day scores, bucket daily detail data, and prepare SVG point/path segments.
- Create: `app/src/utils/historyChart.test.ts`
  Purpose: verify scoring, bucketing, and no-person gap behavior.
- Modify: `app/src/pages/monitor/index.vue`
  Purpose: show four-state posture text and updated logs.
- Modify: `app/src/pages/history/index.vue`
  Purpose: switch to `postureType` history, render 7-day and daily curves, add bucket selector, keep no-person as gaps.
- Modify: `app/src/utils/constants.test.ts`
  Purpose: update constant expectations to the new active protocol surface.
- Modify: `app/src/utils/store.test.ts`
  Purpose: verify new posture parsing and state derivation behavior.

---

### Task 1: Update protocol constants to the new active App surface

**Files:**
- Modify: `app/src/utils/constants.ts`
- Modify: `app/src/utils/constants.test.ts`

- [ ] **Step 1: Write the failing constant expectations**

Add/update expectations so the test asserts the active surface contains:

```ts
expect(PROP_IDS.POSTURE_TYPE).toBe('postureType')
expect(PROP_IDS.PERSON_PRESENT).toBe('personPresent')
expect(PROP_IDS.FILL_LIGHT_ON).toBe('fillLightOn')
expect(Object.values(POSTURE_TYPES)).toEqual(
  expect.arrayContaining(['no_person', 'normal', 'head_down', 'hunchback', 'unknown'])
)
```

- [ ] **Step 2: Run the single test to verify it fails**

Run: `npx vitest run src/utils/constants.test.ts`

Expected: FAIL because the new property IDs and active posture set do not fully exist yet.

- [ ] **Step 3: Write the minimal constants update**

Update `constants.ts` so the active protocol includes:

```ts
export const POSTURE_TYPES = {
  NO_PERSON: 'no_person',
  NORMAL: 'normal',
  HEAD_DOWN: 'head_down',
  HUNCHBACK: 'hunchback',
  UNKNOWN: 'unknown',
} as const

export const PROP_IDS = {
  POSTURE_TYPE: 'postureType',
  PERSON_PRESENT: 'personPresent',
  FILL_LIGHT_ON: 'fillLightOn',
  MONITORING_ENABLED: 'monitoringEnabled',
  CURRENT_MODE: 'currentMode',
  ALERT_MODE_MASK: 'alertModeMask',
  COOLDOWN_MS: 'cooldownMs',
  TIMER_DURATION_SEC: 'timerDurationSec',
  TIMER_RUNNING: 'timerRunning',
  CFG_VERSION: 'cfgVersion',
  SELF_TEST: 'selfTest',
} as const
```

- [ ] **Step 4: Re-run the single test**

Run: `npx vitest run src/utils/constants.test.ts`

Expected: PASS.

- [ ] **Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 2: Build and test shared history aggregation utilities

**Files:**
- Create: `app/src/utils/historyChart.ts`
- Create: `app/src/utils/historyChart.test.ts`

- [ ] **Step 1: Write the failing tests for scoring and gaps**

Create tests that prove:

```ts
test('scores a day from postureType points with no_person excluded', () => {
  const points = [
    { time: '2026-04-10T09:00:00+08:00', value: 'normal' },
    { time: '2026-04-10T09:05:00+08:00', value: 'head_down' },
    { time: '2026-04-10T09:10:00+08:00', value: 'no_person' },
  ]
  expect(scoreHistoryPoints(points)).toBe(50)
})

test('creates null bucket scores for no_person-only buckets', () => {
  const points = [
    { time: '2026-04-10T09:00:00+08:00', value: 'no_person' },
  ]
  const buckets = bucketDailyHistory(points, 5)
  expect(buckets.some(bucket => bucket.score === null)).toBe(true)
})
```

- [ ] **Step 2: Run the single test file to verify it fails**

Run: `npx vitest run src/utils/historyChart.test.ts`

Expected: FAIL because the module does not exist yet.

- [ ] **Step 3: Write the minimal utility module**

Implement functions like:

```ts
export function normalizePostureValue(value: PropertyValue): PostureValue
export function isHealthyPosture(value: PostureValue): boolean
export function isTrackedPosture(value: PostureValue): boolean
export function scoreHistoryPoints(points: HistoryDataPoint[]): number | null
export function bucketDailyHistory(points: HistoryDataPoint[], bucketMinutes: number): BucketPoint[]
export function splitSegments(points: Array<{ x: number; y: number | null }>): Array<Array<{ x: number; y: number }>>
```

Keep `no_person` outside the denominator and return `null` for empty/no-person-only buckets.

- [ ] **Step 4: Re-run the single utility test file**

Run: `npx vitest run src/utils/historyChart.test.ts`

Expected: PASS.

- [ ] **Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 3: Migrate store state to the new posture protocol

**Files:**
- Modify: `app/src/utils/store.ts`
- Modify: `app/src/utils/store.test.ts`

- [ ] **Step 1: Write the failing store expectations**

Add tests that prove `fetchLatest()` derives the new posture state correctly:

```ts
expect(store.state.postureType).toBe('head_down')
expect(store.state.personPresent).toBe(true)
expect(store.state.fillLightOn).toBe(false)
expect(store.postureText).toBe('低头')
expect(store.state.isPosture).toBe(false)
```

- [ ] **Step 2: Run the single store test file to verify it fails**

Run: `npx vitest run src/utils/store.test.ts`

Expected: FAIL because the state still depends on `isPosture` as the source field.

- [ ] **Step 3: Write the minimal store migration**

Update the state and derivations so `fetchLatest()` reads:

```ts
const postureTypeProp = props.find((p) => p.identifier === PROP_IDS.POSTURE_TYPE)
const personPresentProp = props.find((p) => p.identifier === PROP_IDS.PERSON_PRESENT)
const fillLightProp = props.find((p) => p.identifier === PROP_IDS.FILL_LIGHT_ON)
```

Then derive:

```ts
state.postureType = normalizePostureValue(postureTypeProp?.value)
state.personPresent = personPresentProp?.value === true || personPresentProp?.value === 'true'
state.fillLightOn = fillLightProp?.value === true || fillLightProp?.value === 'true'
state.isPosture = state.postureType === 'normal'
```

Also update local statistic accumulation so:

- `normal` adds good + total
- `head_down/hunchback` adds total only
- `no_person` adds neither

- [ ] **Step 4: Re-run the store test file**

Run: `npx vitest run src/utils/store.test.ts`

Expected: PASS.

- [ ] **Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 4: Update the realtime monitor page to four-state posture semantics

**Files:**
- Modify: `app/src/pages/monitor/index.vue`

- [ ] **Step 1: Write the red checklist for the page**

Confirm the page still has these old behaviors:

```text
1. top status only shows normal/abnormal
2. detail card still says 坐姿状态 instead of 当前姿态
3. logs only distinguish normal/abnormal edges
```

- [ ] **Step 2: Run the red check**

Read `src/pages/monitor/index.vue` and confirm all three statements are still true.

Expected: RED.

- [ ] **Step 3: Write the minimal monitor-page update**

Switch rendering from `store.state.isPosture` to `store.state.postureType` and `store.postureText`, for example:

```ts
const getPostureStatusClass = computed(() => {
  if (!store.state.isOnline) return 'offline'
  if (store.state.postureType === 'normal') return 'normal'
  if (store.state.postureType === 'no_person') return 'idle'
  return 'danger'
})
```

Update logs to emit `低头/驼背/无人/恢复正常` instead of plain normal/abnormal.

- [ ] **Step 4: Re-read the page for consistency**

Check that the card label, badge text, and logs all use the same four-state language.

- [ ] **Step 5: Commit**

Do not commit unless the user explicitly asks.

---

### Task 5: Replace the history bar chart with 7-day and daily SVG Bezier curves

**Files:**
- Modify: `app/src/pages/history/index.vue`

- [ ] **Step 1: Write the red checklist for the history page**

Confirm the current page is still wrong in these ways:

```text
1. still requests isPosture history
2. still renders a bar chart instead of a curve
3. has no daily detail chart
4. has no bucket selector for 5/10/15 minutes
```

- [ ] **Step 2: Run the red check**

Read `src/pages/history/index.vue` and confirm all four points are still true.

Expected: RED.

- [ ] **Step 3: Switch the page data source to postureType history**

Update the fetch path to use:

```ts
const history = await queryPropertyHistory(PROP_IDS.POSTURE_TYPE, 7)
```

Then build:

- 7-day score points from `scoreHistoryPoints`
- daily bucket data from `bucketDailyHistory`

- [ ] **Step 4: Add the bucket selector for daily detail**

Use a local selection state like:

```ts
const bucketOptions = [5, 10, 15] as const
const selectedBucketMinutes = ref<5 | 10 | 15>(5)
```

The selector only refreshes the daily detail chart, not the summary cards.

- [ ] **Step 5: Render the SVG curves with gaps**

Replace the existing bar DOM with:

```html
<svg class="curve-svg" viewBox="0 0 700 260" preserveAspectRatio="none">
  <path v-for="segment in weekCurveSegments" :d="segment.path" class="curve-line" />
  <circle v-for="point in weekPoints" :cx="point.x" :cy="point.y" r="6" class="curve-node" />
</svg>
```

Do the same for daily detail, but skip or split any `null` bucket score so `no_person` appears as a line break.

- [ ] **Step 6: Keep range7 from showing the daily detail curve**

When `selectedDateKey === 'range7'`, render only the 7-day trend and 7-day abnormal list.

- [ ] **Step 7: Re-read the page for consistency**

Check that:

- all history queries use `postureType`
- the daily curve defaults to 5-minute buckets
- `no_person` is rendered as a gap
- range7 no longer shows the daily detail curve

- [ ] **Step 8: Commit**

Do not commit unless the user explicitly asks.

---

### Task 6: Run App verification

**Files:**
- No source edits required

- [ ] **Step 1: Run targeted Vitest files**

Run:

```bash
npx vitest run src/utils/constants.test.ts src/utils/store.test.ts src/utils/historyChart.test.ts
```

Expected: PASS.

- [ ] **Step 2: Run type-check**

Run:

```bash
npm run type-check
```

Expected: PASS with no TypeScript errors.

- [ ] **Step 3: Record manual verification still required**

Document the remaining manual checks:

- monitor page shows 无人/正常/低头/驼背 correctly
- 7-day SVG curve renders and node selection works
- daily curve bucket selector switches between 5/10/15 minutes
- no_person intervals appear as gaps instead of 0-value drops

- [ ] **Step 4: Commit**

Do not commit unless the user explicitly asks.
