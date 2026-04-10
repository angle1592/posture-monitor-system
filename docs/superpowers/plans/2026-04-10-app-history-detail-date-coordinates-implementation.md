# App History Detail Date Coordinates Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the daily detail Bezier curve by replacing fragile date-string parsing with stable local date helpers and a shared chart-point builder.

**Architecture:** Keep the fix narrow and layered: `date.ts` owns local date start parsing, `historyChart.ts` owns bucket-to-chart-point conversion, and `history/index.vue` consumes those helpers.

**Tech Stack:** TypeScript, Vue 3, Vitest, existing historyChart/date utility modules.

---

## File Map

- Modify: `app/src/utils/date.ts`
- Modify: `app/src/utils/date.test.ts`
- Modify: `app/src/utils/historyChart.ts`
- Modify: `app/src/utils/historyChart.test.ts`
- Modify: `app/src/pages/history/index.vue`

---

### Task 1: Add failing tests for stable date parsing and detail chart point generation

**Files:**
- Modify: `app/src/utils/date.test.ts`
- Modify: `app/src/utils/historyChart.test.ts`

- [ ] **Step 1: Write the failing tests**

Add tests proving:

```ts
expect(parseLocalDateStartMs('2026-04-10')).toBe(new Date(2026, 3, 10, 0, 0, 0, 0).getTime())
```

and:

```ts
const points = buildBucketChartPoints('2026-04-10', buckets, 24, 672, (score) => score)
expect(points[0].x).toBeGreaterThanOrEqual(24)
expect(Number.isFinite(points[0].x)).toBe(true)
```

- [ ] **Step 2: Run the targeted tests to verify they fail**

Run:

```bash
npx vitest run src/utils/date.test.ts src/utils/historyChart.test.ts
```

Expected: FAIL because the helpers do not exist yet.

---

### Task 2: Implement the shared helpers

**Files:**
- Modify: `app/src/utils/date.ts`
- Modify: `app/src/utils/historyChart.ts`

- [ ] **Step 1: Add `parseLocalDateStartMs`**

Implement a helper that safely parses `YYYY-MM-DD` into local midnight milliseconds.

- [ ] **Step 2: Add `buildBucketChartPoints`**

Implement a helper that converts bucketed history into chart points using the parsed local-day start.

- [ ] **Step 3: Re-run the targeted tests**

Run:

```bash
npx vitest run src/utils/date.test.ts src/utils/historyChart.test.ts
```

Expected: PASS.

---

### Task 3: Switch the history page to the shared chart-point helper

**Files:**
- Modify: `app/src/pages/history/index.vue`

- [ ] **Step 1: Replace inline `dayStart` coordinate math**

Use `buildBucketChartPoints(...)` instead of computing `new Date(`${selectedDetailDate}T00:00:00`)` in the page.

- [ ] **Step 2: Run App verification**

Run:

```bash
npm test
npm run type-check
```

Expected: PASS.
