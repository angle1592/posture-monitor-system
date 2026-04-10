# App History Detail Status Band Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refine the daily detail status band so it reads as a clean timeline instead of a thick progress bar.

**Architecture:** Keep the existing status-band data model and only polish the rendering layer inside `history/index.vue`: thinner track, clearer segment colors, one axis row, and a small legend.

**Tech Stack:** Vue 3, uni-app canvas, scoped SCSS.

---

## File Map

- Modify: `app/src/pages/history/index.vue`

---

### Task 1: Polish the daily detail status-band rendering

**Files:**
- Modify: `app/src/pages/history/index.vue`

- [ ] **Step 1: Remove duplicate axis text**
- [ ] **Step 2: Replace thick fill blocks with a thin rounded track**
- [ ] **Step 3: Add a simple normal/abnormal/no-person legend**
- [ ] **Step 4: Run `npm test` and `npm run type-check`**
