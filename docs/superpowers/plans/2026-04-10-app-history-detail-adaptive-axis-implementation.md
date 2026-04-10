# App History Detail Adaptive Axis Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the daily detail chart auto-fit the actual tracked time range instead of squeezing all points into a full-day 00:00-24:00 axis.

**Architecture:** Keep the adaptation inside the chart-point preparation layer and expose axis labels to the page. This preserves the existing history scoring logic while improving readability of the daily detail view.

**Tech Stack:** TypeScript, existing historyChart utility module, Vue page computed state.

---

## File Map

- Modify: `app/src/utils/historyChart.ts`
- Modify: `app/src/utils/historyChart.test.ts`
- Modify: `app/src/pages/history/index.vue`

---

### Task 1: Add failing tests for adaptive axis range

**Files:**
- Modify: `app/src/utils/historyChart.test.ts`

- [ ] **Step 1: Write the failing tests**

Add tests proving:

```ts
const result = buildAdaptiveBucketChart(...)
expect(result.axisLabels).toHaveLength(3)
expect(result.points[0].x).toBeGreaterThan(24)
expect(result.points[0].x).toBeLessThan(696)
```

- [ ] **Step 2: Run the targeted test file**

Run: `npx vitest run src/utils/historyChart.test.ts`

Expected: FAIL because adaptive axis helpers do not exist yet.

### Task 2: Implement adaptive daily axis helpers

**Files:**
- Modify: `app/src/utils/historyChart.ts`

- [ ] **Step 1: Add adaptive range + label helpers**

Compute:

- axis start/end from tracked bucket points
- a minimum visible window when there are too few points
- 3 formatted axis labels

- [ ] **Step 2: Re-run the targeted test file**

Run: `npx vitest run src/utils/historyChart.test.ts`

Expected: PASS.

### Task 3: Wire the history page to the adaptive axis

**Files:**
- Modify: `app/src/pages/history/index.vue`

- [ ] **Step 1: Replace fixed detail axis labels**

Use dynamic labels from the adaptive helper instead of `00:00 / 12:00 / 24:00`.

- [ ] **Step 2: Use adaptive chart points for detail canvas rendering**

Keep the week chart unchanged; only the detail chart becomes adaptive.

- [ ] **Step 3: Verify**

Run:

```bash
npm test
npm run type-check
```

Expected: PASS.
