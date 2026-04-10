import type { HistoryDataPoint, PropertyValue } from './oneNetApi'
import { POSTURE_TYPES } from './constants'
import { parseLocalDateStartMs } from './date'

export type PostureValue = (typeof POSTURE_TYPES)[keyof typeof POSTURE_TYPES]

export interface BucketPoint {
  bucketStartMs: number
  bucketEndMs: number
  score: number | null
  normalCount: number
  abnormalCount: number
}

export interface ChartPoint {
  x: number
  y: number | null
}

export type DetailBandState = 'normal' | 'abnormal' | 'mixed'

export interface DetailBandSegment {
  state: DetailBandState
  startX: number
  endX: number
}

function parseHistoryTime(value: number | string): number {
  if (typeof value === 'number') return value
  const parsed = new Date(value).getTime()
  return Number.isNaN(parsed) ? 0 : parsed
}

export function normalizePostureValue(value: PropertyValue): PostureValue {
  if (value === 0 || value === '0') return POSTURE_TYPES.NO_PERSON
  if (value === 1 || value === '1') return POSTURE_TYPES.NORMAL
  if (value === 2 || value === '2') return POSTURE_TYPES.HEAD_DOWN
  if (value === 3 || value === '3') return POSTURE_TYPES.HUNCHBACK

  switch (value) {
    case POSTURE_TYPES.NO_PERSON:
    case POSTURE_TYPES.NORMAL:
    case POSTURE_TYPES.HEAD_DOWN:
    case POSTURE_TYPES.HUNCHBACK:
      return value
    default:
      return POSTURE_TYPES.UNKNOWN
  }
}

export function isHealthyPosture(value: PostureValue): boolean {
  return value === POSTURE_TYPES.NORMAL
}

export function isAbnormalPosture(value: PostureValue): boolean {
  return value === POSTURE_TYPES.HEAD_DOWN || value === POSTURE_TYPES.HUNCHBACK
}

export function isTrackedPosture(value: PostureValue): boolean {
  return isHealthyPosture(value) || isAbnormalPosture(value)
}

export function scoreHistoryPoints(points: HistoryDataPoint[]): number | null {
  let normalCount = 0
  let abnormalCount = 0

  for (const point of points) {
    const posture = normalizePostureValue(point.value)
    if (isHealthyPosture(posture)) {
      normalCount += 1
    } else if (isAbnormalPosture(posture)) {
      abnormalCount += 1
    }
  }

  const trackedCount = normalCount + abnormalCount
  if (trackedCount === 0) {
    return null
  }

  return Math.round((normalCount / trackedCount) * 100)
}

export function bucketDailyHistory(points: HistoryDataPoint[], bucketMinutes: number): BucketPoint[] {
  const bucketMs = bucketMinutes * 60 * 1000
  const byBucket = new Map<number, BucketPoint>()

  for (const point of points) {
    const pointMs = parseHistoryTime(point.time)
    if (pointMs <= 0) {
      continue
    }

    const bucketStartMs = Math.floor(pointMs / bucketMs) * bucketMs
    const existing = byBucket.get(bucketStartMs) ?? {
      bucketStartMs,
      bucketEndMs: bucketStartMs + bucketMs,
      score: null,
      normalCount: 0,
      abnormalCount: 0,
    }

    const posture = normalizePostureValue(point.value)
    if (isHealthyPosture(posture)) {
      existing.normalCount += 1
    } else if (isAbnormalPosture(posture)) {
      existing.abnormalCount += 1
    }

    byBucket.set(bucketStartMs, existing)
  }

  return Array.from(byBucket.values())
    .sort((a, b) => a.bucketStartMs - b.bucketStartMs)
    .map((bucket) => {
      const trackedCount = bucket.normalCount + bucket.abnormalCount
      return {
        ...bucket,
        score: trackedCount > 0 ? Math.round((bucket.normalCount / trackedCount) * 100) : null,
      }
    })
}

export function splitChartSegments(points: ChartPoint[]): Array<Array<{ x: number; y: number }>> {
  const segments: Array<Array<{ x: number; y: number }>> = []
  let currentSegment: Array<{ x: number; y: number }> = []

  for (const point of points) {
    if (point.y === null) {
      if (currentSegment.length > 0) {
        segments.push(currentSegment)
        currentSegment = []
      }
      continue
    }

    currentSegment.push({ x: point.x, y: point.y })
  }

  if (currentSegment.length > 0) {
    segments.push(currentSegment)
  }

  return segments
}

export function buildSmoothPath(points: Array<{ x: number; y: number }>): string {
  if (points.length === 0) {
    return ''
  }

  if (points.length === 1) {
    return `M ${points[0].x} ${points[0].y}`
  }

  let path = `M ${points[0].x} ${points[0].y}`
  for (let index = 1; index < points.length; index += 1) {
    const previous = points[index - 1]
    const current = points[index]
    const midX = (previous.x + current.x) / 2
    path += ` C ${midX} ${previous.y}, ${midX} ${current.y}, ${current.x} ${current.y}`
  }

  return path
}

export function buildBucketChartPoints(
  dateKey: string,
  buckets: BucketPoint[],
  plotLeft: number,
  plotWidth: number,
  yMapper: (score: number) => number,
): Array<{ key: string; x: number; y: number | null; score: number | null }> {
  const dayStart = parseLocalDateStartMs(dateKey)

  return buckets.map((bucket) => {
    const minutesFromStart = (bucket.bucketStartMs - dayStart) / 60000
    const x = plotLeft + (minutesFromStart / (24 * 60)) * plotWidth

    return {
      key: String(bucket.bucketStartMs),
      x,
      y: bucket.score === null ? null : yMapper(bucket.score),
      score: bucket.score,
    }
  })
}

function formatAxisTimeLabel(timestampMs: number): string {
  const date = new Date(timestampMs)
  const hh = String(date.getHours()).padStart(2, '0')
  const mm = String(date.getMinutes()).padStart(2, '0')
  return `${hh}:${mm}`
}

export function buildAdaptiveBucketChart(
  dateKey: string,
  buckets: BucketPoint[],
  plotLeft: number,
  plotWidth: number,
  yMapper: (score: number) => number,
): {
  points: Array<{ key: string; x: number; y: number | null; score: number | null }>
  axisLabels: [string, string, string]
} {
  const trackedBuckets = buckets.filter(bucket => bucket.score !== null)
  if (trackedBuckets.length === 0) {
    return {
      points: buildBucketChartPoints(dateKey, buckets, plotLeft, plotWidth, yMapper),
      axisLabels: ['00:00', '12:00', '24:00'],
    }
  }

  const rawStart = Math.min(...trackedBuckets.map(bucket => bucket.bucketStartMs))
  const rawEnd = Math.max(...trackedBuckets.map(bucket => bucket.bucketStartMs))
  const minimumWindowMs = 30 * 60 * 1000
  const paddingMs = 10 * 60 * 1000

  let axisStart = rawStart - paddingMs
  let axisEnd = rawEnd + paddingMs

  if (axisEnd - axisStart < minimumWindowMs) {
    const center = (rawStart + rawEnd) / 2
    axisStart = center - minimumWindowMs / 2
    axisEnd = center + minimumWindowMs / 2
  }

  const points = buckets.map((bucket) => ({
    key: String(bucket.bucketStartMs),
    x: plotLeft + ((bucket.bucketStartMs - axisStart) / (axisEnd - axisStart)) * plotWidth,
    y: bucket.score === null ? null : yMapper(bucket.score),
    score: bucket.score,
  }))

  const axisMid = axisStart + (axisEnd - axisStart) / 2
  return {
    points,
    axisLabels: [
      formatAxisTimeLabel(axisStart),
      formatAxisTimeLabel(axisMid),
      formatAxisTimeLabel(axisEnd),
    ],
  }
}

function resolveBucketBandState(bucket: BucketPoint): DetailBandState | null {
  if (bucket.score === null) return null
  if (bucket.normalCount > 0 && bucket.abnormalCount === 0) return 'normal'
  if (bucket.abnormalCount > 0 && bucket.normalCount === 0) return 'abnormal'
  return 'mixed'
}

export function buildDetailStatusSegments(
  buckets: BucketPoint[],
  dateKey: string,
  plotLeft: number,
  plotWidth: number,
): {
  segments: DetailBandSegment[]
  axisLabels: [string, string, string]
} {
  const adaptive = buildAdaptiveBucketChart(dateKey, buckets, plotLeft, plotWidth, (score) => score)
  const segments: DetailBandSegment[] = []

  for (let index = 0; index < buckets.length; index += 1) {
    const bucket = buckets[index]
    const state = resolveBucketBandState(bucket)
    if (state === null) {
      continue
    }

    const startX = adaptive.points[index]?.x ?? plotLeft
    const nextPoint = adaptive.points[index + 1]
    const endX = nextPoint ? nextPoint.x : startX + 18

    const lastSegment = segments[segments.length - 1]
    if (lastSegment && lastSegment.state === state && Math.abs(lastSegment.endX - startX) < 2) {
      lastSegment.endX = endX
      continue
    }

    segments.push({ state, startX, endX })
  }

  return {
    segments,
    axisLabels: adaptive.axisLabels,
  }
}
