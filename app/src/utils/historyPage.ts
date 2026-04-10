export interface DailyHistoryStats {
  goodPostureTime: string
  abnormalCount: number
  correctionRate: number
}

export interface DetailTimelineSegment {
  startX: number
  endX: number
  state: 'normal' | 'abnormal' | 'mixed'
}

export interface DetailTimelineDisplaySegment {
  leftPercent: number
  widthPercent: number
  state: DetailTimelineSegment['state']
}

export interface AbnormalTimelineMarker {
  leftPercent: number
  label: string
}

export function buildDetailSummaryText(showDetailChart: boolean, stats: DailyHistoryStats): string {
  if (!showDetailChart) {
    return '最近 7 天姿态概览'
  }

  return `良好 ${stats.goodPostureTime} · 异常 ${stats.abnormalCount} 次`
}

export function getVisibleHistoryRecords<T>(records: T[], showAllRecords: boolean, limit: number): T[] {
  if (showAllRecords || records.length <= limit) {
    return records
  }

  return records.slice(0, limit)
}

export function hasHiddenHistoryRecords<T>(records: T[], limit: number): boolean {
  return records.length > limit
}

export function buildDetailTimelineSegments(
  segments: DetailTimelineSegment[],
  startX: number,
  endX: number,
): DetailTimelineDisplaySegment[] {
  const totalWidth = Math.max(1, endX - startX)

  return segments.map((segment) => {
    const visibleStart = Math.min(endX, Math.max(startX, segment.startX))
    const visibleEnd = Math.min(endX, Math.max(startX, segment.endX))
    const leftPercent = Number((((visibleStart - startX) / totalWidth) * 100).toFixed(2))
    const widthPercent = Number((((Math.max(0, visibleEnd - visibleStart)) / totalWidth) * 100).toFixed(2))

    return {
      leftPercent,
      widthPercent,
      state: segment.state,
    }
  })
}

function parseAxisLabelMinutes(label: string): number {
  const [hh = '0', mm = '0'] = label.split(':')
  return Number(hh) * 60 + Number(mm)
}

function formatMinutesLabel(totalMinutes: number): string {
  const normalized = Math.max(0, Math.round(totalMinutes / 5) * 5)
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
    .map((segment) => ({
      startMinutes: percentToMinutes(segment.leftPercent, axisLabels),
      endMinutes: percentToMinutes(segment.leftPercent + segment.widthPercent, axisLabels),
      leftPercent: Number((segment.leftPercent + segment.widthPercent / 2).toFixed(2)),
    }))

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
