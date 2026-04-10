import { describe, expect, test } from 'vitest'

import { bucketDailyHistory, buildAdaptiveBucketChart, buildBucketChartPoints, buildDetailStatusSegments, buildSmoothPath, normalizePostureValue, scoreHistoryPoints, splitChartSegments } from './historyChart'

describe('historyChart', () => {
  test('scores postureType history with no_person excluded from denominator', () => {
    const points = [
      { time: '2026-04-10T09:00:00+08:00', value: 'normal' },
      { time: '2026-04-10T09:05:00+08:00', value: 'head_down' },
      { time: '2026-04-10T09:10:00+08:00', value: 'no_person' },
    ]

    expect(scoreHistoryPoints(points)).toBe(50)
  })

  test('maps numeric posture codes to active posture values', () => {
    expect(normalizePostureValue(0)).toBe('no_person')
    expect(normalizePostureValue(1)).toBe('normal')
    expect(normalizePostureValue(2)).toBe('head_down')
    expect(normalizePostureValue(3)).toBe('hunchback')
  })

  test('creates null score buckets for no_person-only intervals', () => {
    const points = [
      { time: '2026-04-10T09:00:00+08:00', value: 'no_person' },
    ]

    const buckets = bucketDailyHistory(points, 5)

    expect(buckets.some(bucket => bucket.score === null)).toBe(true)
  })

  test('splits chart segments on null score gaps', () => {
    const segments = splitChartSegments([
      { x: 0, y: 20 },
      { x: 50, y: 40 },
      { x: 100, y: null },
      { x: 150, y: 30 },
    ])

    expect(segments).toEqual([
      [
        { x: 0, y: 20 },
        { x: 50, y: 40 },
      ],
      [
        { x: 150, y: 30 },
      ],
    ])
  })

  test('builds a cubic bezier path from chart points', () => {
    const path = buildSmoothPath([
      { x: 0, y: 80 },
      { x: 100, y: 40 },
      { x: 200, y: 60 },
    ])

    expect(path).toContain('M 0 80')
    expect(path).toContain('C 50 80, 50 40, 100 40')
    expect(path).toContain('C 150 40, 150 60, 200 60')
  })

  test('builds finite chart points from bucketed daily history using a date key', () => {
    const points = buildBucketChartPoints(
      '2026-04-10',
      [
        { bucketStartMs: new Date(2026, 3, 10, 9, 0, 0, 0).getTime(), bucketEndMs: 0, score: 80, normalCount: 4, abnormalCount: 1 },
      ],
      24,
      672,
      (score) => score,
    )

    expect(points).toHaveLength(1)
    expect(points[0].x).toBeGreaterThanOrEqual(24)
    expect(Number.isFinite(points[0].x)).toBe(true)
    expect(points[0].y).toBe(80)
  })

  test('builds adaptive detail axis labels and spreads points across visible range', () => {
    const result = buildAdaptiveBucketChart(
      '2026-04-10',
      [
        { bucketStartMs: new Date(2026, 3, 10, 18, 20, 0, 0).getTime(), bucketEndMs: 0, score: 80, normalCount: 4, abnormalCount: 1 },
        { bucketStartMs: new Date(2026, 3, 10, 18, 35, 0, 0).getTime(), bucketEndMs: 0, score: 60, normalCount: 3, abnormalCount: 2 },
      ],
      24,
      672,
      (score) => score,
    )

    expect(result.axisLabels).toHaveLength(3)
    expect(result.axisLabels[0]).toMatch(/^18:/)
    expect(result.points[0].x).toBeGreaterThan(24)
    expect(result.points[0].x).toBeLessThan(696)
    expect(result.points[1].x).toBeGreaterThan(result.points[0].x)
  })

  test('merges adjacent daily buckets into visual status segments', () => {
    const result = buildDetailStatusSegments(
      [
        { bucketStartMs: new Date(2026, 3, 10, 18, 20).getTime(), bucketEndMs: new Date(2026, 3, 10, 18, 25).getTime(), score: 100, normalCount: 3, abnormalCount: 0 },
        { bucketStartMs: new Date(2026, 3, 10, 18, 25).getTime(), bucketEndMs: new Date(2026, 3, 10, 18, 30).getTime(), score: 100, normalCount: 2, abnormalCount: 0 },
        { bucketStartMs: new Date(2026, 3, 10, 18, 30).getTime(), bucketEndMs: new Date(2026, 3, 10, 18, 35).getTime(), score: 0, normalCount: 0, abnormalCount: 2 },
        { bucketStartMs: new Date(2026, 3, 10, 18, 35).getTime(), bucketEndMs: new Date(2026, 3, 10, 18, 40).getTime(), score: null, normalCount: 0, abnormalCount: 0 },
      ],
      '2026-04-10',
      24,
      672,
    )

    expect(result.axisLabels).toHaveLength(3)
    expect(result.segments).toHaveLength(2)
    expect(result.segments[0].state).toBe('normal')
    expect(result.segments[1].state).toBe('abnormal')
    expect(result.segments[0].endX).toBeGreaterThan(result.segments[0].startX)
  })
})
