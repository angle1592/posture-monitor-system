# App History Canvas Render Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the history page's SVG trend rendering with canvas rendering while keeping the existing data preparation and scoring logic intact.

**Architecture:** Reuse the current chart-point preparation in `history/index.vue` and only swap the presentation layer to `uni.createCanvasContext` drawing. Keep the canvas drawing helpers local to the page to avoid premature abstraction.

**Tech Stack:** uni-app canvas, Vue 3, existing historyChart utilities.

---

## File Map

- Modify: `app/src/pages/history/index.vue`
  Purpose: replace SVG markup and rendering path with canvas-based trend rendering.

---

### Task 1: Replace the history SVG layers with canvas rendering

**Files:**
- Modify: `app/src/pages/history/index.vue`

- [ ] **Step 1: Write the red checklist**

Confirm the current history page still renders both trends with inline SVG.

- [ ] **Step 2: Run the red check**

Read `src/pages/history/index.vue` and confirm the SVG structure is still present.

Expected: RED.

- [ ] **Step 3: Replace week trend SVG with canvas**

Use a canvas node for the 7-day trend and draw the prepared point segments with `uni.createCanvasContext`.

- [ ] **Step 4: Replace daily detail SVG with canvas**

Use a second canvas node for the daily detail trend and draw only tracked segments, preserving no-person gaps.

- [ ] **Step 5: Re-run verification**

Run:

```bash
npm test
npm run type-check
```

Expected: PASS.
