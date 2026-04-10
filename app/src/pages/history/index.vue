<template>
  <PageShell tone="cyber">
    <view class="container">
      <view class="page-header">
        <SectionHeader title="历史记录" subtitle="DATA HISTORY">
          <template #right>
            <view class="date-picker" @click="showDatePicker">
              <text class="picker-icon">◴</text>
              <text class="picker-text">{{ selectedDate }}</text>
              <text class="picker-arrow">▼</text>
            </view>
          </template>
        </SectionHeader>
      </view>

      <view class="stats-grid">
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
      </view>

      <view v-if="showDetailChart" class="detail-chart-section detail-chart-section--primary">
        <view class="section-label section-label--stacked">
          <view class="section-heading-group">
            <view class="section-heading-main">
              <text class="label-icon">◈</text>
              <text class="label-text">当天明细</text>
            </view>
            <text class="section-summary">{{ detailSummaryText }}</text>
          </view>

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
          <view class="panel-headline">
            <text class="panel-title">{{ selectedDate }}</text>
            <text class="panel-caption">日内姿态状态时间轴</text>
          </view>
          <view class="curve-hint">无人时显示为断线，不计入评分</view>
          <view class="detail-legend">
            <view class="legend-chip normal">
              <view class="legend-line"></view>
              <text>正常</text>
            </view>
            <view class="legend-chip abnormal">
              <view class="legend-line"></view>
              <text>异常</text>
            </view>
            <view class="legend-chip gap">
              <view class="legend-line"></view>
              <text>无人</text>
            </view>
          </view>

          <view v-if="hasDetailTrackedData" class="detail-timeline-wrap">
            <view class="detail-timeline">
              <view
                v-for="marker in detailAxisMarkers"
                :key="`${marker.label}-${marker.leftPercent}`"
                class="detail-marker"
                :style="{ left: `${marker.leftPercent}%` }"
              ></view>

              <view class="detail-track"></view>

              <view
                v-for="(segment, index) in detailTimelineSegments"
                :key="`${segment.state}-${index}`"
                class="detail-segment"
                :class="`detail-segment--${segment.state}`"
                :style="{ left: `${segment.leftPercent}%`, width: `${segment.widthPercent}%` }"
              ></view>
            </view>

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

            <view class="axis-row axis-row--timeline">
              <view
                v-for="marker in detailAxisMarkers"
                :key="marker.label"
                class="axis-tick"
                :style="{ left: `${marker.leftPercent}%` }"
              >
                <text>{{ marker.label }}</text>
              </view>
            </view>
          </view>

          <view v-else class="detail-empty">
            <text class="empty-title">当日暂无有效监测曲线</text>
            <text class="empty-subtitle">当全部为无人或没有历史数据时，将不会绘制曲线</text>
          </view>
        </view>
      </view>

      <view class="trend-section trend-section--secondary">
        <view class="section-label section-label--stacked section-label--compact">
          <view class="section-heading-group">
            <view class="section-heading-main">
              <text class="label-icon">◈</text>
              <text class="label-text">7天趋势</text>
            </view>
            <text class="section-summary">点击任一天可查看对应明细</text>
          </view>

          <view class="trend-meta">
            <text class="data-source">{{ hasRealHistory ? 'DATA: ONENET' : 'DATA: ESTIMATED' }}</text>
          </view>
        </view>

        <view class="curve-panel curve-panel--secondary">
          <view class="chart-legend">
            <view class="legend-item">
              <view class="legend-dot excellent"></view>
              <text>优秀</text>
            </view>
            <view class="legend-item">
              <view class="legend-dot good"></view>
              <text>良好</text>
            </view>
            <view class="legend-item">
              <view class="legend-dot poor"></view>
              <text>需改进</text>
            </view>
          </view>

          <canvas class="curve-canvas curve-canvas--week" canvas-id="weekTrendCanvas" id="weekTrendCanvas"></canvas>

          <view class="curve-footer week-footer week-footer--ticks">
            <view
              v-for="day in weekData"
              :key="day.date"
              class="week-tick"
              :class="{ active: activeWeekDateKey === day.date }"
              @click="selectDay(day)"
            >
              <text class="week-tick-label">{{ day.label }}</text>
              <text class="week-tick-score" :class="{ muted: day.score === null }">
                {{ day.score === null ? '--' : day.score }}
              </text>
            </view>
          </view>
        </view>
      </view>

      <view class="records-section">
        <view class="section-label">
          <text class="label-icon">◈</text>
          <text class="label-text">{{ recordsTitle }}</text>
          <view class="record-count">
            <text>{{ abnormalRecords.length }} 条记录</text>
          </view>
        </view>

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
      </view>
    </view>
  </PageShell>
</template>

<script setup lang="ts">
import { computed, nextTick, ref, watch } from 'vue'
import { onShow } from '@dcloudio/uni-app'
import store from '@/utils/store'
import PageShell from '@/components/ui/PageShell.vue'
import SectionHeader from '@/components/ui/SectionHeader.vue'
import StatCard from '@/components/ui/StatCard.vue'
import { queryPropertyHistory } from '@/utils/oneNetApi'
import type { HistoryDataPoint } from '@/utils/oneNetApi'
import { formatLocalDate } from '@/utils/date'
import { buildAbnormalTimelineMarkers, buildDetailSummaryText, buildDetailTimelineSegments, getVisibleHistoryRecords, hasHiddenHistoryRecords } from '@/utils/historyPage'
import type { DailyHistoryStats } from '@/utils/historyPage'
import { DEVICE_DEFAULTS, PROP_IDS, POSTURE_TYPES } from '@/utils/constants'
import {
  bucketDailyHistory,
  buildDetailStatusSegments,
  isAbnormalPosture,
  isHealthyPosture,
  normalizePostureValue,
  scoreHistoryPoints,
  splitChartSegments,
} from '@/utils/historyChart'

interface DayData {
  label: string
  score: number | null
  date: string
  source: 'history' | 'fallback'
}

interface AbnormalRecord {
  type: 'head_down' | 'hunchback'
  title: string
  description: string
  time: string
}

interface ChartDisplayPoint {
  key: string
  x: number
  y: number | null
  score: number | null
}

type AxisLabels = [string, string, string]

const bucketOptions = [5, 10, 15] as const
const defaultVisibleRecordCount = 6
const weekGridScores = [100, 70, 40] as const
const defaultDetailAxisLabels: AxisLabels = ['00:00', '12:00', '24:00']

const todayDate = formatLocalDate(new Date())
const selectedDate = ref(todayDate)
const selectedDateKey = ref<string>('today')
const selectedBucketMinutes = ref<(typeof bucketOptions)[number]>(5)
const showAllRecords = ref(false)

const dailyStats = ref<DailyHistoryStats>({
  goodPostureTime: '0分钟',
  abnormalCount: 0,
  correctionRate: 100,
})

const weekData = ref<DayData[]>([])
const hasRealHistory = ref(false)
const abnormalRecords = ref<AbnormalRecord[]>([])
const historyPoints = ref<HistoryDataPoint[]>([])

const recordsTitle = computed(() => selectedDateKey.value === 'range7' ? '最近7天异常记录' : '异常记录')
const emptyTitle = computed(() => selectedDateKey.value === 'range7' ? '最近7天暂无异常记录' : '当日暂无异常记录')
const showDetailChart = computed(() => selectedDateKey.value !== 'range7')
const detailSummaryText = computed(() => buildDetailSummaryText(showDetailChart.value, dailyStats.value))
const hasMoreRecords = computed(() => hasHiddenHistoryRecords(abnormalRecords.value, defaultVisibleRecordCount))
const visibleAbnormalRecords = computed(() => getVisibleHistoryRecords(abnormalRecords.value, showAllRecords.value, defaultVisibleRecordCount))
const recordToggleText = computed(() => showAllRecords.value ? '收起记录' : `查看全部 ${abnormalRecords.value.length} 条`)
const activeWeekDateKey = computed(() => {
  if (selectedDateKey.value === 'today') {
    return todayDate
  }

  if (selectedDateKey.value === 'yesterday') {
    const yesterday = new Date()
    yesterday.setDate(yesterday.getDate() - 1)
    return formatLocalDate(yesterday)
  }

  if (selectedDateKey.value === 'range7') {
    return ''
  }

  return selectedDateKey.value
})

const selectedDetailDate = computed(() => {
  if (!showDetailChart.value) {
    return null
  }
  return selectedDate.value
})

function pointDate(point: HistoryDataPoint): string {
  return formatLocalDate(point.time)
}

function pointTime(point: HistoryDataPoint): string {
  const date = typeof point.time === 'number' ? new Date(point.time) : new Date(String(point.time))
  if (Number.isNaN(date.getTime())) return '--:--:--'
  return date.toLocaleTimeString('zh-CN', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  })
}

function formatMinuteText(minutes: number): string {
  if (minutes <= 0) return '0分钟'
  const hours = Math.floor(minutes / 60)
  const mins = minutes % 60
  if (hours > 0) return `${hours}小时${mins}分`
  return `${mins}分钟`
}

function estimateMinutesFromPoints(pointCount: number): number {
  if (pointCount <= 0) {
    return 0
  }
  return Math.max(1, Math.round((pointCount * DEVICE_DEFAULTS.PUBLISH_INTERVAL_MS) / 60000))
}

function buildFallbackWeekData(): DayData[] {
  const days = ['日', '一', '二', '三', '四', '五', '六']
  const fallback: DayData[] = []
  const baseScore = store.healthScore

  for (let index = 6; index >= 0; index -= 1) {
    const date = new Date()
    date.setDate(date.getDate() - index)
    fallback.push({
      label: days[date.getDay()],
      score: index === 0 ? baseScore : Math.max(60, baseScore - index * 2),
      date: formatLocalDate(date),
      source: 'fallback',
    })
  }

  return fallback
}

function buildWeekDataFromHistory(points: HistoryDataPoint[]): DayData[] {
  const days = ['日', '一', '二', '三', '四', '五', '六']
  const dayMap = new Map<string, HistoryDataPoint[]>()

  for (const point of points) {
    const date = pointDate(point)
    const list = dayMap.get(date) ?? []
    list.push(point)
    dayMap.set(date, list)
  }

  const data: DayData[] = []
  for (let index = 6; index >= 0; index -= 1) {
    const date = new Date()
    date.setDate(date.getDate() - index)
    const dateKey = formatLocalDate(date)
    data.push({
      label: days[date.getDay()],
      score: dayMap.has(dateKey) ? scoreHistoryPoints(dayMap.get(dateKey) ?? []) : null,
      date: dateKey,
      source: 'history',
    })
  }

  return data
}

function buildAbnormalRecords(points: HistoryDataPoint[]): AbnormalRecord[] {
  return points
    .map((point) => ({ point, posture: normalizePostureValue(point.value) }))
    .filter(({ posture }) => posture === POSTURE_TYPES.HEAD_DOWN || posture === POSTURE_TYPES.HUNCHBACK)
    .map(({ point, posture }) => ({
      type: posture as 'head_down' | 'hunchback',
      title: posture === POSTURE_TYPES.HEAD_DOWN ? '检测到低头' : '检测到驼背',
      description: posture === POSTURE_TYPES.HEAD_DOWN
        ? '建议抬头放松颈部，保持视线平齐'
        : '建议挺直腰背，避免长时间弓背'
      ,
      time: pointTime(point),
    }))
}

function resolveScopedPoints(): HistoryDataPoint[] {
  const today = formatLocalDate(new Date())

  if (selectedDateKey.value === 'range7') {
    const dateSet = new Set(weekData.value.map(day => day.date))
    return historyPoints.value.filter(point => dateSet.has(pointDate(point)))
  }

  if (selectedDateKey.value === 'today') {
    return historyPoints.value.filter(point => pointDate(point) === today)
  }

  if (selectedDateKey.value === 'yesterday') {
    const yesterday = new Date()
    yesterday.setDate(yesterday.getDate() - 1)
    const yesterdayDate = formatLocalDate(yesterday)
    return historyPoints.value.filter(point => pointDate(point) === yesterdayDate)
  }

  return historyPoints.value.filter(point => pointDate(point) === selectedDateKey.value)
}

function applySelectionStats() {
  if (selectedDateKey.value === 'today' && historyPoints.value.length === 0) {
    dailyStats.value = {
      goodPostureTime: store.usageTimeText,
      abnormalCount: store.state.todayAbnormalCount,
      correctionRate: store.healthScore,
    }
    abnormalRecords.value = []
    return
  }

  const scopedPoints = resolveScopedPoints()
  const healthyCount = scopedPoints.filter(point => isHealthyPosture(normalizePostureValue(point.value))).length
  const abnormalPoints = scopedPoints.filter(point => isAbnormalPosture(normalizePostureValue(point.value)))
  const score = scoreHistoryPoints(scopedPoints)

  dailyStats.value = {
    goodPostureTime: formatMinuteText(estimateMinutesFromPoints(healthyCount)),
    abnormalCount: abnormalPoints.length,
    correctionRate: score ?? 100,
  }
  abnormalRecords.value = buildAbnormalRecords(scopedPoints)
}

async function fetchHistoryData() {
  try {
    const history = await queryPropertyHistory(PROP_IDS.POSTURE_TYPE, 7)
    historyPoints.value = history
      .slice()
      .sort((left, right) => {
        const leftTime = typeof left.time === 'number' ? left.time : new Date(left.time).getTime()
        const rightTime = typeof right.time === 'number' ? right.time : new Date(right.time).getTime()
        return leftTime - rightTime
      })

    if (historyPoints.value.length > 0) {
      weekData.value = buildWeekDataFromHistory(historyPoints.value)
      hasRealHistory.value = true
    } else {
      weekData.value = buildFallbackWeekData()
      hasRealHistory.value = false
    }
  } catch (error) {
    weekData.value = buildFallbackWeekData()
    hasRealHistory.value = false
    historyPoints.value = []
    console.warn('[History] 获取历史数据失败:', error)
  }

  applySelectionStats()
}

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

function getRecordIcon(type: string): string {
  if (type === 'head_down') return '↓'
  if (type === 'hunchback') return '⌒'
  return '!'
}

function weekChartY(score: number): number {
  const top = 24
  const height = 150
  return top + ((100 - score) / 100) * height
}

const weekChartPoints = computed(() => {
  const left = 24
  const width = 652
  const step = weekData.value.length > 1 ? width / (weekData.value.length - 1) : 0

  return weekData.value.map((day, index) => ({
    ...day,
    x: left + step * index,
    y: day.score === null ? null : weekChartY(day.score),
  }))
})

const detailBuckets = computed(() => {
  if (!selectedDetailDate.value) {
    return []
  }

  const points = historyPoints.value.filter(point => pointDate(point) === selectedDetailDate.value)
  return bucketDailyHistory(points, selectedBucketMinutes.value)
})

const detailBandData = computed(() => {
  if (!selectedDetailDate.value) {
    return {
      segments: [],
      axisLabels: defaultDetailAxisLabels,
    }
  }

  return buildDetailStatusSegments(
    detailBuckets.value,
    selectedDetailDate.value,
    24,
    672,
  )
})

const detailAxisLabels = computed<AxisLabels>(() => detailBandData.value.axisLabels)

const detailTimelineSegments = computed(() => buildDetailTimelineSegments(detailBandData.value.segments, 24, 696))

const abnormalTimelineMarkers = computed(() => buildAbnormalTimelineMarkers(detailTimelineSegments.value, detailAxisLabels.value))

const detailAxisMarkers = computed(() => ([
  { label: detailAxisLabels.value[0], leftPercent: 0 },
  { label: detailAxisLabels.value[1], leftPercent: 50 },
  { label: detailAxisLabels.value[2], leftPercent: 100 },
]))

const hasDetailTrackedData = computed(() => detailTimelineSegments.value.length > 0)

function drawChartCanvas(
  canvasId: string,
  chartPoints: ChartDisplayPoint[],
  gridScores: readonly number[],
  lineColor: string,
  nodeRadius: number,
) {
  const context = uni.createCanvasContext(canvasId)
  const width = 720
  const height = 220

  context.clearRect(0, 0, width, height)

  context.setStrokeStyle('rgba(255, 255, 255, 0.08)')
  context.setLineWidth(1)
  gridScores.forEach((score) => {
    const y = score === 100 ? 24 : score === 70 ? 69 : 114
    context.beginPath()
    context.moveTo(24, y)
    context.lineTo(width - 24, y)
    context.stroke()
  })

  const segments = splitChartSegments(chartPoints)
  context.setStrokeStyle(lineColor)
  context.setLineWidth(4)

  segments.forEach((segment) => {
    if (segment.length === 0) return
    context.beginPath()
    context.moveTo(segment[0].x, segment[0].y)
    for (let index = 1; index < segment.length; index += 1) {
      const previous = segment[index - 1]
      const current = segment[index]
      const midX = (previous.x + current.x) / 2
      context.bezierCurveTo(midX, previous.y, midX, current.y, current.x, current.y)
    }
    context.stroke()
  })

  chartPoints.forEach((point) => {
    if (point.y === null) return
    context.beginPath()
    context.setFillStyle('#0f172a')
    context.arc(point.x, point.y, nodeRadius, 0, 2 * Math.PI)
    context.fill()
    context.setStrokeStyle(point.score !== null && point.score >= 85 ? '#00e676' : point.score !== null && point.score >= 70 ? '#ffab00' : '#ff5252')
    context.setLineWidth(3)
    context.stroke()
  })

  context.draw()
}

function renderHistoryCanvases() {
  nextTick(() => {
    drawChartCanvas('weekTrendCanvas', weekChartPoints.value.map(point => ({ key: point.date, x: point.x, y: point.y, score: point.score })), weekGridScores, '#00f0ff', 6)
  })
}

watch([weekChartPoints, selectedBucketMinutes], () => {
  renderHistoryCanvases()
})

onShow(() => {
  showAllRecords.value = false
  selectedDateKey.value = 'today'
  selectedDate.value = todayDate
  selectedBucketMinutes.value = 5
  weekData.value = buildFallbackWeekData()
  applySelectionStats()
  fetchHistoryData()
  renderHistoryCanvases()
})
</script>

<style lang="scss" scoped>
.container {
  padding: 40rpx 30rpx 60rpx;
  min-height: 100vh;
}

.page-header {
  margin-bottom: 40rpx;
  animation: slide-up var(--duration-slow) var(--ease-out) both;
}

.date-picker {
  display: flex;
  align-items: center;
  gap: 12rpx;
  padding: 16rpx 24rpx;
  background: var(--bg-tertiary);
  border-radius: var(--radius-pill);
  border: 1rpx solid var(--border-subtle);

  .picker-icon {
    font-size: 24rpx;
    color: var(--neon-cyan);
  }

  .picker-text {
    font-size: 26rpx;
    color: var(--text-primary);
    font-weight: 600;
  }

  .picker-arrow {
    font-size: 20rpx;
    color: var(--text-tertiary);
  }

  &:active {
    border-color: var(--neon-cyan);
    box-shadow: var(--neon-cyan-glow);
  }
}

.stats-grid {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 20rpx;
  margin-bottom: 40rpx;
  animation: slide-up var(--duration-slow) var(--ease-out) 100ms both;

  .stat-card {
    min-height: 220rpx;
    animation: card-enter var(--duration-slow) var(--ease-out) both;

    &:nth-child(1) { animation-delay: 100ms; }
    &:nth-child(2) { animation-delay: 150ms; }
    &:nth-child(3) { animation-delay: 200ms; }

    :deep(.stat-card-cyber) {
      height: 100%;
      padding: 30rpx 26rpx;
      border-color: rgba(148, 163, 184, 0.16);
      box-shadow: 0 18rpx 40rpx rgba(15, 23, 42, 0.24);
    }

    :deep(.content) {
      min-height: 100%;
      justify-content: space-between;
      gap: 24rpx;
    }

    :deep(.meta-row) {
      align-items: flex-start;
    }

    :deep(.label) {
      font-size: 22rpx;
      letter-spacing: 1.5rpx;
    }

    :deep(.value) {
      font-size: 58rpx;
      line-height: 1;
    }
  }

  .stat-card-score {
    :deep(.stat-card-cyber) {
      background: linear-gradient(180deg, rgba(15, 50, 40, 0.95) 0%, rgba(11, 24, 31, 0.98) 100%);
      box-shadow: 0 24rpx 48rpx rgba(0, 35, 28, 0.28);
    }

    :deep(.value) {
      font-size: 64rpx;
    }
  }
}

.section-label {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 18rpx;
  margin-bottom: 20rpx;

  &--stacked {
    align-items: flex-start;
  }

  &--compact {
    margin-bottom: 18rpx;
  }

  .section-heading-group {
    display: flex;
    flex-direction: column;
    gap: 10rpx;
    min-width: 0;
  }

  .section-heading-main {
    display: flex;
    align-items: center;
    gap: 12rpx;
  }

  .label-icon {
    font-size: 24rpx;
    color: var(--neon-cyan);
    text-shadow: 0 0 10rpx rgba(0, 240, 255, 0.35);
  }

  .label-text {
    font-size: 28rpx;
    font-weight: 700;
    color: var(--text-primary);
    text-transform: uppercase;
    letter-spacing: 2rpx;
  }

  .section-summary {
    font-size: 22rpx;
    color: var(--text-tertiary);
    line-height: 1.4;
  }

  .data-source {
    font-size: 20rpx;
    color: var(--text-muted);
    font-family: var(--font-mono);
  }

  .record-count {
    margin-left: auto;
    padding: 8rpx 20rpx;
    background: var(--bg-tertiary);
    border-radius: var(--radius-pill);
    border: 1rpx solid var(--border-subtle);

    text {
      font-size: 24rpx;
      color: var(--text-tertiary);
    }
  }
}

.trend-section,
.detail-chart-section {
  margin-bottom: 40rpx;
  animation: slide-up var(--duration-slow) var(--ease-out) 200ms both;
}

.detail-chart-section--primary {
  .curve-panel {
    padding: 32rpx 28rpx 26rpx;
    border-radius: 28rpx;
    background: linear-gradient(180deg, rgba(20, 27, 46, 0.96) 0%, rgba(13, 18, 34, 0.98) 100%);
    box-shadow: 0 28rpx 56rpx rgba(6, 12, 24, 0.3);
  }
}

.trend-section--secondary {
  .curve-panel {
    padding: 24rpx 24rpx 20rpx;
  }
}

.curve-panel {
  background: linear-gradient(180deg, rgba(19, 27, 45, 0.92) 0%, rgba(12, 18, 33, 0.96) 100%);
  border-radius: var(--radius-xl);
  padding: 28rpx;
  border: 1rpx solid rgba(148, 163, 184, 0.14);
  box-shadow: 0 20rpx 44rpx rgba(6, 12, 24, 0.2);
}

.panel-headline {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 16rpx;
  margin-bottom: 12rpx;

  .panel-title {
    font-size: 30rpx;
    font-weight: 700;
    color: var(--text-primary);
  }

  .panel-caption {
    font-size: 20rpx;
    color: var(--text-muted);
    letter-spacing: 1rpx;
  }
}

.trend-meta {
  flex-shrink: 0;
}

.chart-legend {
  display: flex;
  justify-content: flex-end;
  gap: 24rpx;
  margin-bottom: 20rpx;

  .legend-item {
    display: flex;
    align-items: center;
    gap: 10rpx;

    .legend-dot {
      width: 12rpx;
      height: 12rpx;
      border-radius: 50%;

      &.excellent {
        background: #00e676;
        box-shadow: 0 0 10rpx #00e676;
      }

      &.good {
        background: #ffab00;
        box-shadow: 0 0 10rpx #ffab00;
      }

      &.poor {
        background: #ff5252;
        box-shadow: 0 0 10rpx #ff5252;
      }
    }

    text {
      font-size: 22rpx;
      color: var(--text-tertiary);
    }
  }
}

.curve-canvas {
  width: 100%;
  height: 260rpx;
}

.curve-canvas--week {
  height: 216rpx;
}

.curve-footer {
  display: flex;
  gap: 12rpx;
  margin-top: 20rpx;
}

.week-footer {
  justify-content: space-between;
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
  padding: 14rpx 0 4rpx;
  border-top: 2rpx solid rgba(148, 163, 184, 0.14);
  transition: all var(--duration-base);

  &.active {
    border-top-color: rgba(0, 240, 255, 0.92);
  }
}

.week-tick-label {
  font-size: 22rpx;
  color: var(--text-tertiary);
}

.week-tick-score {
  font-size: 24rpx;
  font-weight: 700;
  color: var(--text-primary);
  font-family: var(--font-mono);

  &.muted {
    color: var(--text-muted);
  }
}

.bucket-switcher {
  display: flex;
  flex-wrap: wrap;
  justify-content: flex-end;
  gap: 12rpx;
}

.bucket-chip {
  padding: 10rpx 18rpx;
  border-radius: var(--radius-pill);
  background: var(--bg-tertiary);
  border: 1rpx solid var(--border-subtle);

  text {
    font-size: 22rpx;
    color: var(--text-tertiary);
  }

  &.active {
    border-color: var(--neon-cyan);
    background: var(--neon-cyan-dim);

    text {
      color: var(--neon-cyan);
    }
  }
}

.curve-hint {
  font-size: 22rpx;
  color: var(--text-muted);
  margin-bottom: 16rpx;
  line-height: 1.5;
}

.detail-legend {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 24rpx;
  margin-bottom: 18rpx;
}

.legend-chip {
  display: flex;
  align-items: center;
  gap: 10rpx;

  text {
    font-size: 22rpx;
    color: var(--text-tertiary);
  }

  .legend-line {
    width: 28rpx;
    height: 8rpx;
    border-radius: 999rpx;
    background: rgba(148, 163, 184, 0.25);
  }

  &.normal .legend-line {
    background: rgba(0, 230, 118, 0.95);
    box-shadow: 0 0 12rpx rgba(0, 230, 118, 0.35);
  }

  &.abnormal .legend-line {
    background: rgba(255, 122, 89, 0.95);
    box-shadow: 0 0 12rpx rgba(255, 122, 89, 0.35);
  }

  &.gap .legend-line {
    background: rgba(148, 163, 184, 0.22);
    border: 1rpx dashed rgba(148, 163, 184, 0.4);
  }
}

.axis-row {
  margin-top: 18rpx;
  position: relative;
  height: 36rpx;

  text {
    font-size: 22rpx;
    color: var(--text-muted);
    font-family: var(--font-mono);
  }
}

.axis-row--timeline {
  margin-top: 22rpx;
}

.axis-tick {
  position: absolute;
  top: 0;
  transform: translateX(-50%);

  &:first-child {
    transform: translateX(0);
  }

  &:last-child {
    transform: translateX(-100%);
  }
}

.detail-timeline-wrap {
  margin-top: 12rpx;
}

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

.detail-timeline {
  position: relative;
  height: 180rpx;
  padding: 24rpx 24rpx 0;
}

.detail-track {
  position: absolute;
  left: 24rpx;
  right: 24rpx;
  top: 92rpx;
  height: 28rpx;
  border-radius: 999rpx;
  background: rgba(98, 113, 142, 0.16);
  box-shadow: inset 0 0 0 1rpx rgba(148, 163, 184, 0.12);
}

.detail-segment {
  position: absolute;
  top: 92rpx;
  height: 28rpx;
  min-width: 12rpx;
  border-radius: 999rpx;
  box-shadow: 0 0 0 2rpx rgba(11, 17, 30, 0.72);

  &--normal {
    background: linear-gradient(90deg, rgba(0, 230, 118, 0.92) 0%, rgba(24, 255, 155, 0.88) 100%);
  }

  &--abnormal {
    background: linear-gradient(90deg, rgba(255, 122, 89, 0.95) 0%, rgba(255, 92, 92, 0.92) 100%);
  }

  &--mixed {
    background: linear-gradient(90deg, rgba(0, 230, 118, 0.92) 0%, rgba(0, 230, 118, 0.92) 48%, rgba(255, 122, 89, 0.95) 52%, rgba(255, 122, 89, 0.95) 100%);
  }
}

.detail-marker {
  position: absolute;
  top: 24rpx;
  bottom: 24rpx;
  width: 2rpx;
  background: linear-gradient(180deg, rgba(148, 163, 184, 0.04) 0%, rgba(148, 163, 184, 0.2) 45%, rgba(148, 163, 184, 0.04) 100%);

  &:first-child {
    left: 24rpx !important;
  }

  &:last-child {
    left: calc(100% - 24rpx) !important;
  }
}

.detail-empty {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 70rpx 0 56rpx;

  .empty-title {
    font-size: 30rpx;
    color: var(--text-primary);
    margin-bottom: 12rpx;
  }

  .empty-subtitle {
    font-size: 24rpx;
    color: var(--text-tertiary);
    text-align: center;
  }
}

.records-section {
  animation: slide-up var(--duration-slow) var(--ease-out) 300ms both;

  .records-list {
    display: flex;
    flex-direction: column;
    gap: 16rpx;

    .record-card {
      position: relative;
      display: flex;
      align-items: center;
      gap: 20rpx;
      padding: 24rpx;
      background: linear-gradient(180deg, rgba(19, 27, 45, 0.9) 0%, rgba(12, 18, 33, 0.94) 100%);
      border-radius: var(--radius-lg);
      border: 1rpx solid rgba(148, 163, 184, 0.14);
      overflow: hidden;
      animation: card-enter var(--duration-slow) var(--ease-out) both;

      .record-glow {
        position: absolute;
        inset: 0;
        background: linear-gradient(90deg, transparent, rgba(255, 82, 82, 0.1), transparent);
        opacity: 0;
        transition: opacity var(--duration-base);
      }

      &:hover .record-glow {
        opacity: 1;
      }

      .record-icon {
        width: 68rpx;
        height: 68rpx;
        border-radius: var(--radius-md);
        background: rgba(255, 82, 82, 0.15);
        border: 1rpx solid rgba(255, 82, 82, 0.3);
        display: flex;
        align-items: center;
        justify-content: center;
        flex-shrink: 0;

        text {
          font-size: 36rpx;
          color: var(--state-danger);
          font-weight: 700;
        }
      }

      .record-info {
        flex: 1;

        .record-title {
          display: block;
          font-size: 28rpx;
          font-weight: 700;
          color: var(--text-primary);
          margin-bottom: 6rpx;
        }

        .record-desc {
          display: block;
          font-size: 22rpx;
          color: var(--text-tertiary);
          line-height: 1.5;
        }
      }

      .record-time {
        align-self: flex-start;
        padding: 10rpx 18rpx;
        background: var(--bg-elevated);
        border-radius: var(--radius-pill);
        border: 1rpx solid var(--border-subtle);

        .time-code {
          font-size: 24rpx;
          color: var(--neon-cyan);
          font-family: var(--font-mono);
        }
      }
    }

    .records-toggle {
      display: flex;
      justify-content: center;
      align-items: center;
      padding: 14rpx 0 4rpx;

      text {
        padding: 14rpx 26rpx;
        border-radius: 999rpx;
        background: rgba(0, 240, 255, 0.08);
        border: 1rpx solid rgba(0, 240, 255, 0.22);
        color: var(--neon-cyan);
        font-size: 24rpx;
      }
    }

    .empty-state {
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 80rpx 0;

      .empty-icon {
        width: 120rpx;
        height: 120rpx;
        border-radius: 50%;
        background: var(--neon-cyan-dim);
        border: 2rpx solid rgba(0, 240, 255, 0.3);
        display: flex;
        align-items: center;
        justify-content: center;
        font-size: 60rpx;
        color: var(--neon-cyan);
        margin-bottom: 32rpx;
        box-shadow: 0 0 40rpx rgba(0, 240, 255, 0.2);
      }

      .empty-title {
        font-size: 32rpx;
        font-weight: 700;
        color: var(--text-primary);
        margin-bottom: 12rpx;
      }

      .empty-subtitle {
        font-size: 26rpx;
        color: var(--text-tertiary);
      }
    }
  }
}

@keyframes slide-up {
  from {
    opacity: 0;
    transform: translateY(40rpx);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}

@keyframes card-enter {
  from {
    opacity: 0;
    transform: translateY(30rpx) scale(0.95);
  }
  to {
    opacity: 1;
    transform: translateY(0) scale(1);
  }
}

@media screen and (max-width: 430px) {
  .container {
    padding: 30rpx 20rpx 50rpx;
  }

  .stats-grid {
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 16rpx;
  }

   .stat-card-score {
    grid-column: span 2;
  }

  .curve-panel {
    padding: 22rpx;
  }

  .curve-footer {
    gap: 8rpx;
  }

  .week-tick {
    padding-top: 12rpx;
  }

  .bucket-switcher {
    gap: 8rpx;
    justify-content: flex-start;
  }

  .section-label {
    flex-direction: column;
    align-items: flex-start;
  }

  .panel-headline {
    flex-direction: column;
    align-items: flex-start;
  }

  .detail-timeline {
    height: 168rpx;
    padding: 20rpx 18rpx 0;
  }

  .abnormal-marker-row {
    height: 64rpx;
  }

  .abnormal-marker-label {
    font-size: 18rpx;
    padding: 4rpx 10rpx;
  }

  .detail-track,
  .detail-segment {
    top: 86rpx;
  }

  .axis-row--timeline {
    margin-top: 18rpx;
  }

  .record-card {
    align-items: flex-start;
  }

  .record-time {
    margin-left: auto;
  }
}
</style>
