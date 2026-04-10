import { describe, expect, test } from 'vitest'

async function loadHistoryPageModule() {
  try {
    return await import('./historyPage')
  } catch {
    return null
  }
}

describe('historyPage', () => {
  test('builds a daily detail summary from the selected day stats', async () => {
    const module = await loadHistoryPageModule()

    expect(module?.buildDetailSummaryText(true, {
      goodPostureTime: '21分钟',
      abnormalCount: 15,
      correctionRate: 89,
    })).toBe('良好 21分钟 · 异常 15 次')
  })

  test('builds a range summary when daily detail is hidden', async () => {
    const module = await loadHistoryPageModule()

    expect(module?.buildDetailSummaryText(false, {
      goodPostureTime: '0分钟',
      abnormalCount: 0,
      correctionRate: 100,
    })).toBe('最近 7 天姿态概览')
  })

  test('limits abnormal records to a preview subset before expansion', async () => {
    const module = await loadHistoryPageModule()
    const records = Array.from({ length: 8 }, (_, index) => ({ id: index + 1 }))

    expect(module?.getVisibleHistoryRecords(records, false, 6)).toEqual(records.slice(0, 6))
  })

  test('returns all abnormal records after expansion', async () => {
    const module = await loadHistoryPageModule()
    const records = Array.from({ length: 8 }, (_, index) => ({ id: index + 1 }))

    expect(module?.getVisibleHistoryRecords(records, true, 6)).toEqual(records)
  })

  test('reports whether the abnormal record list should show a toggle', async () => {
    const module = await loadHistoryPageModule()

    expect(module?.hasHiddenHistoryRecords(new Array(7).fill(null), 6)).toBe(true)
    expect(module?.hasHiddenHistoryRecords(new Array(6).fill(null), 6)).toBe(false)
  })

  test('builds timeline segments with percentage widths for the detail band', async () => {
    const module = await loadHistoryPageModule()

    expect(module?.buildDetailTimelineSegments([
      { startX: 24, endX: 224, state: 'normal' },
      { startX: 224, endX: 424, state: 'abnormal' },
      { startX: 424, endX: 696, state: 'mixed' },
    ], 24, 696)).toEqual([
      { leftPercent: 0, widthPercent: 29.76, state: 'normal' },
      { leftPercent: 29.76, widthPercent: 29.76, state: 'abnormal' },
      { leftPercent: 59.52, widthPercent: 40.48, state: 'mixed' },
    ])
  })

  test('clamps timeline segments into the visible detail range', async () => {
    const module = await loadHistoryPageModule()

    expect(module?.buildDetailTimelineSegments([
      { startX: 0, endX: 1000, state: 'normal' },
    ], 24, 696)).toEqual([
      { leftPercent: 0, widthPercent: 100, state: 'normal' },
    ])
  })

  test('builds compact abnormal markers from abnormal and mixed timeline segments', async () => {
    const module = await loadHistoryPageModule()

    expect(module?.buildAbnormalTimelineMarkers([
      { leftPercent: 10, widthPercent: 4, state: 'normal' },
      { leftPercent: 20, widthPercent: 3, state: 'abnormal' },
      { leftPercent: 55, widthPercent: 8, state: 'mixed' },
    ], ['06:50', '13:42', '20:35'])).toEqual([
      { leftPercent: 21.5, label: '09:45' },
      { leftPercent: 59, label: '14:25 - 15:30' },
    ])
  })

  test('merges nearby abnormal markers into one range label', async () => {
    const module = await loadHistoryPageModule()

    expect(module?.buildAbnormalTimelineMarkers([
      { leftPercent: 20, widthPercent: 2, state: 'abnormal' },
      { leftPercent: 24, widthPercent: 2, state: 'abnormal' },
    ], ['06:50', '13:42', '20:35'])).toEqual([
      { leftPercent: 23, label: '09:35 - 10:25' },
    ])
  })
})
