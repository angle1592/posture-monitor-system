# Docs Structure Normalization Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a unified product documentation entry point while keeping docs stored by domain, and normalize a small set of active document names safely.

**Architecture:** Keep documentation physically distributed by scope: workspace-level material at the root, product-level navigation in `refactored/docs/`, and subsystem-specific documents inside each active subtree's `docs/` directory. Introduce an index page and update existing entry points so readers navigate from one place instead of moving all files into one folder.

**Tech Stack:** Markdown documentation, draw.io/PNG assets, repository README and AGENTS guidance

---

### Task 1: Create product docs entry point

**Files:**
- Create: `refactored/docs/index.md`
- Modify: `refactored/README.md`

**Step 1: Write the new index page**
Create a product-level index that links architecture, protocol, app docs, ESP32 docs, K230 docs, and hardware diagrams.

**Step 2: Link the index from the product README**
Add a short documentation section to `refactored/README.md` that points to the new index.

**Step 3: Verify links visually**
Check all referenced paths exist and the wording matches current repository structure.

### Task 2: Normalize active doc filenames with low blast radius

**Files:**
- Move: `refactored/posture_monitor/docs/hardware-wiring-and-controls.md` -> `refactored/posture_monitor/docs/hardware-wiring.md`
- Move: `refactored/k230/docs/k230-引脚信息整理.md` -> `refactored/k230/docs/k230-pinout.md`
- Modify: all README/index references to those files

**Step 1: Rename active documentation files**
Use clearer, normalized filenames in lowercase English with hyphens.

**Step 2: Update every active reference**
Update references in product and subtree README files plus the new index.

**Step 3: Avoid risky global diagram renames**
Do not rename thesis-facing diagram assets in this pass.

### Task 3: Strengthen workspace-level navigation and guidance

**Files:**
- Modify: `README.md`
- Modify: `AGENTS.md`
- Modify: `refactored/posture_monitor/README.md`
- Modify: `refactored/k230/README.md`
- Modify: `refactored/app/README.md`

**Step 1: Update root navigation**
Add a clear pointer from the workspace root to the product docs index.

**Step 2: Update subtree README docs sections**
Ensure each active subtree README points to its own docs and, where useful, to the shared product index.

**Step 3: Update agent guidance**
Add concise rules describing the docs layout: by-domain storage, `refactored/docs/index.md` as product entry point, and naming conventions for new product docs.

### Task 4: Verify consistency

**Files:**
- Review only

**Step 1: Search for stale references**
Search for the old filenames to ensure active references were updated.

**Step 2: Summarize remaining intentional exceptions**
Note any legacy or thesis-side filenames intentionally left unchanged.
