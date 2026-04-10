# App History Detail Abnormal Markers Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add compact abnormal-time markers below the daily detail timeline so users can immediately see when abnormal posture clusters happened.

**Architecture:** Keep the existing daily detail segmented timeline intact and add a small display-only derivation layer that turns abnormal or mixed timeline segments into marker labels and positions. Render those markers in `history/index.vue` between the status band and the base axis, with lightweight merging to prevent label collisions on dense ranges.

**Tech Stack:** Vue 3 `<script setup>`, TypeScript, scoped SCSS, uni-app, Vitest.

---

## File Map

- Modify: `app/src/utils/historyPage.ts`
  - Add a helper that converts visible timeline segments plus axis labels into abnormal marker display data.
  - Add light collision-merging so close abnormal segments collapse into one readable label.

- Modify: `app/src/utils/historyPage.test.ts`
  - Add failing tests for abnormal marker generation, range labeling, and collision merging.

- Modify: `app/src/pages/history/index.vue`
  - Compute abnormal marker display data from the current detail timeline.
  - Render marker ticks and labels below the segmented timeline.
  - Add scoped styles for desktop and mobile readability.

---

### Task 1: Add the abnormal-marker helper with test-first coverage

**Files:**
- Modify: `app/src/utils/historyPage.ts`
- Modify: `app/src/utils/historyPage.test.ts`

- [ ] **Step 1: Write the failing tests for abnormal marker generation**

Append these tests to `app/src/utils/historyPage.test.ts`:

```ts
  test('builds compact abnormal markers from abnormal and mixed timeline segments', async () => {
    const module = await loadHistoryPageModule()

    expect(module?.buildAbnormalTimelineMarkers([
      { leftPercent: 10, widthPercent: 4, state: 'normal' },
      { leftPercent: 20, widthPercent: 3, state: 'abnormal' },
      { leftPercent: 55, widthPercent: 8, state: 'mixed' },
    ], ['06:50', '13:42', '20:35'])).toEqual([
      { leftPercent: 21.5, label: '09:40' },
      { leftPercent: 59, label: '15:00 - 15:55' },
    ])
  })

  test('merges nearby abnormal markers into one range label', async () => {
    const module = await loadHistoryPageModule()

    expect(module?.buildAbnormalTimelineMarkers([
      { leftPercent: 20, widthPercent: 2, state: 'abnormal' },
      { leftPercent: 24, widthPercent: 2, state: 'abnormal' },
    ], ['06:50', '13:42', '20:35'])).toEqual([
      { leftPercent: 23, label: '09:30 - 09:55' },
    ])
  })
```

- [ ] **Step 2: Run the helper test file and verify it fails for the right reason**

Run: `npx vitest run src/utils/historyPage.test.ts`

Expected: FAIL because `buildAbnormalTimelineMarkers` does not exist yet.

- [ ] **Step 3: Add the minimal helper implementation**

Add these types and helpers to `app/src/utils/historyPage.ts` below the existing timeline display types:

```ts
export interface AbnormalTimelineMarker {
  leftPercent: number
  label: string
}

function parseAxisLabelMinutes(label: string): number {
  const [hh = '0', mm = '0'] = label.split(':')
  return Number(hh) * 60 + Number(mm)
}

function formatMinutesLabel(totalMinutes: number): string {
  const normalized = Math.max(0, Math.round(totalMinutes))
  const hh = String(Math.floor(normalized / 60)).padStart(2, '0')
  const mm = String(normalized % 60).padStart(2, '0')
  return `${hh}:${mm}`
}

function percentToMinutes(percent: number, axisLabels: [string, string, string]): number {
  const start = parseAxisLabelMinutes(axisLabels[0])
  const end = parseAxisLabelMinutes(axisLabels[2])
  const span = Math.max(1, end - start)
  return start + (percent / 100) * span
}

export function buildAbnormalTimelineMarkers(
  segments: DetailTimelineDisplaySegment[],
  axisLabels: [string, string, string],
): AbnormalTimelineMarker[] {
  const rawMarkers = segments
    .filter(segment => segment.state === 'abnormal' || segment.state === 'mixed')
    .map((segment) => {
      const startMinutes = percentToMinutes(segment.leftPercent, axisLabels)
      const endMinutes = percentToMinutes(segment.leftPercent + segment.widthPercent, axisLabels)

      return {
        startMinutes,
        endMinutes,
        leftPercent: Number((segment.leftPercent + segment.widthPercent / 2).toFixed(2)),
      }
    })

  const merged: Array<{ startMinutes: number; endMinutes: number; leftPercent: number }> = []
  for (const marker of rawMarkers) {
    const previous = merged[merged.length - 1]
    if (previous && marker.leftPercent - previous.leftPercent < 6) {
      previous.endMinutes = marker.endMinutes
      previous.leftPercent = Number(((previous.leftPercent + marker.leftPercent) / 2).toFixed(2))
      continue
    }

    merged.push({ ...marker })
  }

  return merged.map((marker) => {
    const duration = marker.endMinutes - marker.startMinutes
    const label = duration >= 30
      ? `${formatMinutesLabel(marker.startMinutes)} - ${formatMinutesLabel(marker.endMinutes)}`
      : formatMinutesLabel((marker.startMinutes + marker.endMinutes) / 2)

    return {
      leftPercent: marker.leftPercent,
      label,
    }
  })
}
```

- [ ] **Step 4: Re-run the helper test file and verify it passes**

Run: `npx vitest run src/utils/historyPage.test.ts`

Expected: PASS.

---

### Task 2: Render the abnormal markers under the daily timeline

**Files:**
- Modify: `app/src/pages/history/index.vue`

- [ ] **Step 1: Compute marker data from the existing daily timeline segments**

Update the import and computed section in `app/src/pages/history/index.vue`:

```ts
import {
  buildAbnormalTimelineMarkers,
  buildDetailSummaryText,
  buildDetailTimelineSegments,
  getVisibleHistoryRecords,
  hasHiddenHistoryRecords,
} from '@/utils/historyPage'

const abnormalTimelineMarkers = computed(() => buildAbnormalTimelineMarkers(
  detailTimelineSegments.value,
  detailAxisLabels.value,
))
```

- [ ] **Step 2: Insert the abnormal marker row between the band and the base axis**

Inside the `v-if="hasDetailTrackedData"` block, insert this row after `.detail-timeline` and before `.axis-row--timeline`:

```vue
            <view v-if="abnormalTimelineMarkers.length > 0" class="abnormal-marker-row">
              <view
                v-for="marker in abnormalTimelineMarkers"
                :key="`${marker.leftPercent}-${marker.label}`"
                class="abnormal-marker"
                :style="{ left: `${marker.leftPercent}%` }"
              >
                <view class="abnormal-marker-tick"></view>
                <text class="abnormal-marker-label">{{ marker.label }}</text>
              </view>
            </view>
```

- [ ] **Step 3: Add compact scoped styles for the marker row**

Append these styles near the existing timeline styles:

```scss
.abnormal-marker-row {
  position: relative;
  height: 58rpx;
  margin-top: 8rpx;
}

.abnormal-marker {
  position: absolute;
  top: 0;
  transform: translateX(-50%);
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8rpx;
}

.abnormal-marker-tick {
  width: 2rpx;
  height: 14rpx;
  background: rgba(255, 122, 89, 0.75);
}

.abnormal-marker-label {
  padding: 4rpx 12rpx;
  border-radius: 999rpx;
  background: rgba(255, 122, 89, 0.1);
  border: 1rpx solid rgba(255, 122, 89, 0.22);
  color: #ffb29d;
  font-size: 20rpx;
  line-height: 1;
  white-space: nowrap;
}
```

- [ ] **Step 4: Add the mobile refinement for narrow screens**

Extend the existing mobile media block with:

```scss
  .abnormal-marker-row {
    height: 64rpx;
  }

  .abnormal-marker-label {
    font-size: 18rpx;
    padding: 4rpx 10rpx;
  }
```

- [ ] **Step 5: Run type-check after the page integration**

Run: `npm run type-check`

Expected: PASS.

---

### Task 3: Verify the abnormal markers in the real page

**Files:**
- Modify: `app/src/pages/history/index.vue`

- [ ] **Step 1: Run the full automated test suite**

Run: `npm test`

Expected: PASS.

- [ ] **Step 2: Open the history page in H5 and inspect the marker row**

Open: `http://localhost:5173/#/pages/history/index`

Confirm all of the following:

```text
1. 异常段下方出现短刻度和时间提示
2. 正常段下方没有大面积无关标签
3. 相邻异常段不会出现严重文字重叠
4. 切换 5/10/15 分钟粒度后，提示位置和文本会更新
```

- [ ] **Step 3: Re-run type-check after any visual micro-fixes**

Run: `npm run type-check`

Expected: PASS.
