# App History Timeline Layout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild the history page into a timeline-first layout where the daily detail band becomes the primary chart, the weekly curve becomes a lighter secondary chart, and the abnormal record list no longer drags the whole page down.

**Architecture:** Keep all existing OneNET history queries, scoring logic, and canvas data preparation in place. Concentrate the work in `app/src/pages/history/index.vue` by adding a few local UI-only computed values and then reordering the template and SCSS to create a new visual hierarchy. Reuse the existing `StatCard` API instead of introducing new shared abstractions.

**Tech Stack:** Vue 3 `<script setup>`, uni-app, TypeScript, scoped SCSS, uni-app canvas, Vitest regression suite.

---

## File Map

- Modify: `app/src/pages/history/index.vue`
  - Add local UI state for abnormal-record expansion.
  - Add lightweight computed values for visible records and daily summary text.
  - Reorder the template so `当天明细` appears before `7天趋势`.
  - Replace the heavy week-day chips with lighter date/score ticks.
  - Add a compact expand/collapse control for the abnormal record list.
  - Rework scoped SCSS so the page reads as `摘要卡 -> 当天明细 -> 7天趋势 -> 异常记录`.

- Reuse without modification unless implementation proves otherwise: `app/src/components/ui/StatCard.vue`
  - Use existing `variant` and `valueClass` props from the page instead of widening the shared component API.

---

### Task 1: Add page-local UI state for the timeline-first layout

**Files:**
- Modify: `app/src/pages/history/index.vue`

- [ ] **Step 1: Add local constants and refs for record-list expansion**

Insert these declarations near the existing top-level refs so record-list behavior stays page-local:

```ts
const defaultVisibleRecordCount = 6

const showAllRecords = ref(false)
```

- [ ] **Step 2: Add computed values for daily summary and visible records**

Place these computed values near `recordsTitle`, `emptyTitle`, and `showDetailChart` so all page-level display state stays grouped together:

```ts
const detailSummaryText = computed(() => {
  if (!showDetailChart.value) {
    return '最近 7 天姿态概览'
  }

  return `良好 ${dailyStats.value.goodPostureTime} · 异常 ${dailyStats.value.abnormalCount} 次`
})

const hasMoreRecords = computed(() => abnormalRecords.value.length > defaultVisibleRecordCount)

const visibleAbnormalRecords = computed(() => {
  if (showAllRecords.value || !hasMoreRecords.value) {
    return abnormalRecords.value
  }

  return abnormalRecords.value.slice(0, defaultVisibleRecordCount)
})

const recordToggleText = computed(() => showAllRecords.value ? '收起记录' : '查看全部')
```

- [ ] **Step 3: Reset the expanded record state whenever the selected range changes**

Update both selection flows so switching date range always collapses the list back to the preview state:

```ts
function showDatePicker() {
  uni.showActionSheet({
    itemList: ['今天', '昨天', '最近7天'],
    success: (result) => {
      showAllRecords.value = false
      const now = new Date()
      if (result.tapIndex === 0) {
        selectedDateKey.value = 'today'
        selectedDate.value = formatLocalDate(now)
      } else if (result.tapIndex === 1) {
        const yesterday = new Date(now.getTime() - 24 * 60 * 60 * 1000)
        selectedDateKey.value = 'yesterday'
        selectedDate.value = formatLocalDate(yesterday)
      } else {
        const start = new Date(now)
        start.setDate(start.getDate() - 6)
        selectedDateKey.value = 'range7'
        selectedDate.value = `${formatLocalDate(start)} ~ ${formatLocalDate(now)}`
      }
      applySelectionStats()
    }
  })
}

function selectDay(day: DayData) {
  showAllRecords.value = false
  selectedDateKey.value = day.date
  selectedDate.value = day.date
  applySelectionStats()
}
```

- [ ] **Step 4: Run type-check after the new page-local state is in place**

Run: `npm run type-check`

Expected: PASS with no new TypeScript errors.

---

### Task 2: Restructure the template so the daily timeline becomes the primary chart

**Files:**
- Modify: `app/src/pages/history/index.vue`

- [ ] **Step 1: Strengthen the three summary cards using existing `valueClass` hooks**

Update the three `StatCard` calls so the most important values get stronger visual hooks without changing the shared component API:

```vue
<StatCard
  class="stat-card stat-card-duration"
  variant="primary"
  icon="⏱"
  :value="dailyStats.goodPostureTime"
  value-class="heading"
  label="良好时长"
/>

<StatCard
  class="stat-card stat-card-alert"
  variant="warning"
  icon="⚠️"
  :value="dailyStats.abnormalCount"
  value-class="warning"
  label="异常次数"
/>

<StatCard
  class="stat-card stat-card-score"
  variant="success"
  icon="★"
  :value="dailyStats.correctionRate + '%'"
  value-class="success"
  label="健康评分"
/>
```

- [ ] **Step 2: Move the daily-detail section above the weekly-trend section**

Place the `当天明细` block before `7天趋势`, and update the section header so the title row includes the new summary text and the bucket switcher:

```vue
<view v-if="showDetailChart" class="detail-chart-section detail-chart-section--primary">
  <view class="section-label section-label--stacked">
    <view class="section-heading-group">
      <text class="label-icon">◈</text>
      <text class="label-text">当天明细</text>
    </view>
    <text class="section-summary">{{ detailSummaryText }}</text>
    <view class="bucket-switcher">
      <view
        v-for="option in bucketOptions"
        :key="option"
        class="bucket-chip"
        :class="{ active: selectedBucketMinutes === option }"
        @click="selectedBucketMinutes = option"
      >
        <text>{{ option }}分钟</text>
      </view>
    </view>
  </view>

  <view class="curve-panel">
    <view class="curve-hint">无人时显示为断线，不计入评分</view>
    <view class="detail-legend">
      <view class="legend-chip normal"><view class="legend-line"></view><text>正常</text></view>
      <view class="legend-chip abnormal"><view class="legend-line"></view><text>异常</text></view>
      <view class="legend-chip gap"><view class="legend-line"></view><text>无人</text></view>
    </view>

    <view v-if="hasDetailTrackedData" class="detail-curve-wrap">
      <canvas class="curve-canvas detail-canvas" canvas-id="detailTrendCanvas" id="detailTrendCanvas"></canvas>
      <view class="axis-row">
        <text>{{ detailAxisLabels[0] }}</text>
        <text>{{ detailAxisLabels[1] }}</text>
        <text>{{ detailAxisLabels[2] }}</text>
      </view>
    </view>

    <view v-else class="detail-empty">
      <text class="empty-title">当日暂无有效监测曲线</text>
      <text class="empty-subtitle">当全部为无人或没有历史数据时，将不会绘制曲线</text>
    </view>
  </view>
</view>
```

- [ ] **Step 3: Replace the heavy week footer chips with a lighter tick-style footer**

Inside the `7天趋势` panel, replace the current `day-chip` card layout with a flatter row that still supports date selection:

```vue
<view class="curve-footer week-footer week-footer--ticks">
  <view
    v-for="day in weekData"
    :key="day.date"
    class="week-tick"
    :class="{ active: selectedDateKey === day.date }"
    @click="selectDay(day)"
  >
    <text class="week-tick-label">{{ day.label }}</text>
    <text class="week-tick-score" :class="{ muted: day.score === null }">
      {{ day.score === null ? '--' : day.score }}
    </text>
  </view>
</view>
```

- [ ] **Step 4: Make the abnormal-record list render from `visibleAbnormalRecords` and add the toggle button**

Update the list and append a compact footer action only when there are more than six records:

```vue
<view class="records-list">
  <view
    v-for="(record, index) in visibleAbnormalRecords"
    :key="`${record.time}-${index}`"
    class="record-card"
  >
    <view class="record-glow"></view>

    <view class="record-icon" :class="record.type">
      <text>{{ getRecordIcon(record.type) }}</text>
    </view>

    <view class="record-info">
      <text class="record-title">{{ record.title }}</text>
      <text class="record-desc">{{ record.description }}</text>
    </view>

    <view class="record-time">
      <text class="time-code">{{ record.time }}</text>
    </view>
  </view>

  <view v-if="hasMoreRecords" class="records-toggle" @click="showAllRecords = !showAllRecords">
    <text>{{ recordToggleText }}</text>
  </view>

  <view v-if="abnormalRecords.length === 0" class="empty-state">
    <view class="empty-icon">◉</view>
    <text class="empty-title">{{ emptyTitle }}</text>
    <text class="empty-subtitle">保持良好的坐姿习惯，系统为您守护健康</text>
  </view>
</view>
```

- [ ] **Step 5: Run type-check after the template restructure**

Run: `npm run type-check`

Expected: PASS with no template typing errors.

---

### Task 3: Rework the scoped styles so the page reads top-down in the new hierarchy

**Files:**
- Modify: `app/src/pages/history/index.vue`

- [ ] **Step 1: Increase summary-card presence and reduce all-over glow**

Replace the current `.stats-grid` and `.stat-card` page-level overrides with taller cards and stronger number emphasis:

```scss
.stats-grid {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 20rpx;
  margin-bottom: 40rpx;

  .stat-card {
    min-height: 220rpx;

    :deep(.stat-card-cyber) {
      height: 100%;
      padding: 30rpx 26rpx;
      border-color: rgba(148, 163, 184, 0.16);
      box-shadow: 0 18rpx 40rpx rgba(15, 23, 42, 0.24);
    }

    :deep(.content) {
      min-height: 100%;
      justify-content: space-between;
    }

    :deep(.value) {
      font-size: 58rpx;
      line-height: 1;
    }
  }
}
```

- [ ] **Step 2: Make the daily-detail panel the largest, calmest chart container**

Add a primary modifier for the detail section and use it to create more breathing room for the main timeline panel:

```scss
.detail-chart-section--primary {
  margin-bottom: 40rpx;

  .curve-panel {
    padding: 32rpx 28rpx 26rpx;
    border-radius: 28rpx;
    background: linear-gradient(180deg, rgba(20, 27, 46, 0.96) 0%, rgba(13, 18, 34, 0.98) 100%);
  }

  .detail-canvas {
    height: 320rpx;
  }
}
```

- [ ] **Step 3: Flatten the weekly footer and compress the weekly chart panel**

Replace the old `.day-chip` treatment with lighter tick styling and shrink the weekly chart height so it reads as secondary content:

```scss
.trend-section .curve-canvas {
  height: 220rpx;
}

.week-footer--ticks {
  gap: 10rpx;
}

.week-tick {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8rpx;
  padding: 12rpx 0 6rpx;
  border-top: 1rpx solid rgba(148, 163, 184, 0.16);

  &.active {
    border-top-color: rgba(0, 240, 255, 0.9);
  }
}
```

- [ ] **Step 4: Tighten abnormal-record cards and style the expand/collapse action**

Adjust the record section so the list is denser and the footer control feels intentional instead of improvised:

```scss
.records-list {
  gap: 16rpx;
}

.record-card {
  padding: 24rpx;

  .record-icon {
    width: 68rpx;
    height: 68rpx;
  }

  .record-title {
    font-size: 28rpx;
  }

  .record-desc {
    font-size: 22rpx;
  }
}

.records-toggle {
  display: flex;
  justify-content: center;
  align-items: center;
  padding: 22rpx 0 8rpx;

  text {
    font-size: 24rpx;
    color: var(--neon-cyan);
  }
}
```

- [ ] **Step 5: Update the mobile breakpoint so the new hierarchy survives narrow screens**

Extend the existing mobile media block so the summary cards stack into two rows cleanly and the detail panel keeps its presence:

```scss
@media screen and (max-width: 430px) {
  .stats-grid {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }

  .stat-card-score {
    grid-column: span 2;
  }

  .detail-chart-section--primary .detail-canvas {
    height: 280rpx;
  }
}
```

- [ ] **Step 6: Run type-check after the SCSS rewrite**

Run: `npm run type-check`

Expected: PASS.

---

### Task 4: Verify behavior and visual result in H5

**Files:**
- Modify: `app/src/pages/history/index.vue`

- [ ] **Step 1: Run the automated regression checks**

Run: `npm test`

Expected: PASS with the existing Vitest suite.

- [ ] **Step 2: Start the H5 dev server and inspect the rebuilt history page**

Run: `npm run dev:h5`

Expected: Vite dev server starts successfully and serves the app on the local H5 port.

- [ ] **Step 3: Manually validate the new hierarchy in both desktop and mobile views**

Open: `http://localhost:5173/#/pages/history/index`

Confirm all of the following:

```text
1. 页面顺序为：摘要卡 -> 当天明细 -> 7天趋势 -> 异常记录
2. 当天明细比 7 天趋势更大、更醒目
3. 7 天趋势底部不再是厚重大卡片式日期区
4. 异常记录默认只显示前 6 条，并且可展开/收起
5. 选择不同日期后，当天明细与异常记录同步刷新
```

- [ ] **Step 4: Re-run type-check after any last-mile visual fixes**

Run: `npm run type-check`

Expected: PASS.
