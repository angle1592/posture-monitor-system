# App History Detail Status Band Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the daily detail chart's 0/100 line with a merged status timeline band while keeping the weekly trend curve intact.

**Architecture:** Reuse the existing bucketed history data and adaptive axis, but add a utility that maps buckets into merged visual status segments. Keep the weekly canvas untouched and only replace the detail canvas drawing routine.

**Tech Stack:** TypeScript, Vue 3, uni-app canvas, existing historyChart utilities.

---

## File Map

- Modify: `app/src/utils/historyChart.ts`
- Modify: `app/src/utils/historyChart.test.ts`
- Modify: `app/src/pages/history/index.vue`

---

### Task 1: Add failing tests for status-band segment generation

**Files:**
- Modify: `app/src/utils/historyChart.test.ts`

- [ ] **Step 1: Write the failing test**

Add a test proving that adjacent buckets with the same semantic state merge into one segment, and that `score === null` becomes a gap.

- [ ] **Step 2: Run the targeted test file**

Run: `npx vitest run src/utils/historyChart.test.ts`

Expected: FAIL because the segment helper does not exist yet.

### Task 2: Implement merged status-band helpers

**Files:**
- Modify: `app/src/utils/historyChart.ts`

- [ ] **Step 1: Add bucket-to-band-state mapping**

Implement helpers that classify buckets into `normal/abnormal/mixed/gap`.

- [ ] **Step 2: Add merged visual segment generation**

Return merged drawable segments with start/end X coordinates.

- [ ] **Step 3: Re-run the targeted test file**

Run: `npx vitest run src/utils/historyChart.test.ts`

Expected: PASS.

### Task 3: Replace daily detail canvas drawing with status-band rendering

**Files:**
- Modify: `app/src/pages/history/index.vue`

- [ ] **Step 1: Keep weekly curve rendering unchanged**

Do not touch the 7-day trend canvas path drawing.

- [ ] **Step 2: Replace detail draw routine**

Draw horizontal rounded bands for `normal/abnormal/mixed`, and leave `gap` empty.

- [ ] **Step 3: Verify**

Run:

```bash
npm test
npm run type-check
```

Expected: PASS.
