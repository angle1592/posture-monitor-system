# App StatCard Layout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Re-layout `StatCard` so all cards render icon+label first and the large value second.

**Architecture:** Keep the change isolated to `app/src/components/ui/StatCard.vue` so every page benefits without API churn. Reorder the template structure and adjust the internal flex layout while preserving existing props, variants, and glow styles.

**Tech Stack:** Vue 3 SFC, TypeScript, scoped SCSS, existing App theme tokens.

---

## File Map

- Modify: `app/src/components/ui/StatCard.vue`
  Purpose: reorder label/value layout and adapt internal spacing.

---

### Task 1: Re-layout the shared StatCard component

**Files:**
- Modify: `app/src/components/ui/StatCard.vue`

- [ ] **Step 1: Write the red checklist**

Confirm the current component still behaves like this:

```text
1. icon sits in its own block above content
2. value renders above label
3. label visually reads like a footer instead of a title row
```

- [ ] **Step 2: Run the red check**

Read `src/components/ui/StatCard.vue` and confirm all three points are true.

Expected: RED.

- [ ] **Step 3: Implement the smallest layout change**

Reorder the template to:

```vue
<view class="content">
  <view class="meta-row">
    <view v-if="icon" class="icon-wrap">
      <text class="icon">{{ icon }}</text>
    </view>
    <text class="label">{{ label }}</text>
  </view>
  <text class="value" :class="valueClass">{{ value }}</text>
</view>
```

Update SCSS so:

- `.content` becomes a compact vertical stack
- `.meta-row` aligns icon + label horizontally
- `.value` stays the visual focus

- [ ] **Step 4: Run type-check**

Run: `npm run type-check`

Expected: PASS.

- [ ] **Step 5: Run the full App test suite**

Run: `npm test`

Expected: PASS.

- [ ] **Step 6: Commit**

Do not commit unless the user explicitly asks.
