<template>
  <PageShell tone="cyber">
    <view class="container">
      <!-- 页面标题 -->
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

      <!-- 统计概览 -->
      <view class="stats-grid">
        <StatCard
          class="stat-card"
          variant="primary"
          icon="⏱"
          :value="dailyStats.goodPostureTime"
          label="良好时长"
        />
        
        <StatCard
          class="stat-card"
          variant="warning"
          icon="⚠️"
          :value="dailyStats.abnormalCount"
          label="异常次数"
        />
        
        <StatCard
          class="stat-card"
          variant="success"
          icon="★"
          :value="dailyStats.correctionRate + '%'"
          label="健康评分"
        />
      </view>

      <!-- 7天趋势图 -->
      <view class="trend-section">
        <view class="section-label">
          <text class="label-icon">◈</text>
          <text class="label-text">7天趋势</text>
          <text class="data-source">{{ hasRealHistory ? 'DATA: ONENET' : 'DATA: ESTIMATED' }}</text>
        </view>
        
        <view class="trend-chart">
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
          
          <view class="chart-bars">
            <view
              v-for="(day, index) in weekData"
              :key="index"
              class="bar-item"
              :class="{ active: selectedDateKey === day.date }"
              @click="selectDay(day)"
            >
              <view class="bar-track">
                <view
                  class="bar-fill"
                  :style="{ height: day.score + '%', backgroundColor: getScoreColor(day.score) }"
                ></view>
              </view>
              
              <text class="bar-label">{{ day.label }}</text>
              
              <text class="bar-value">{{ day.score }}</text>
            </view>
          </view>
        </view>
      </view>

      <!-- 异常记录列表 -->
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
            v-for="(record, index) in abnormalRecords"
            :key="index"
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
import { computed, ref } from 'vue'
import { onShow } from '@dcloudio/uni-app'
import store from '@/utils/store'
import PageShell from '@/components/ui/PageShell.vue'
import SectionHeader from '@/components/ui/SectionHeader.vue'
import StatCard from '@/components/ui/StatCard.vue'
import { queryPropertyHistory } from '@/utils/oneNetApi'
import type { HistoryDataPoint, PropertyValue } from '@/utils/oneNetApi'
import { formatLocalDate } from '@/utils/date'

/*
 * 页面职责：历史数据页。
 * - 展示当天/昨天/最近7天的健康趋势、统计摘要与异常记录。
 * - 优先使用 OneNET 历史属性流；无历史时回退到本地估算数据。
 * - 保证图表、统计、列表三者的数据口径一致。
 */

interface DayData {
  label: string
  score: number
  date: string
  source: 'history' | 'fallback'
}

interface AbnormalRecord {
  type: string
  title: string
  description: string
  time: string
}

const todayDate = formatLocalDate(new Date())
const selectedDate = ref(todayDate)
const selectedDateKey = ref<string>('today')

const dailyStats = ref({
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

function isGoodPosture(value: PropertyValue): boolean {
  return value === true || value === 'true' || value === 1
}

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

function buildAbnormalRecords(points: HistoryDataPoint[]): AbnormalRecord[] {
  return points
    .filter(point => !isGoodPosture(point.value))
    .map(point => ({
      type: 'abnormal',
      title: '检测到异常姿势',
      description: '建议及时调整坐姿，保持腰背挺直',
      time: pointTime(point),
    }))
}

function applySelectionStats() {
  const today = formatLocalDate(new Date())

  if (selectedDateKey.value === 'today' && historyPoints.value.length === 0) {
    // 当天历史尚未回填时，优先用 store 实时累计值，避免页面初开显示全 0。
    dailyStats.value = {
      goodPostureTime: store.usageTimeText,
      abnormalCount: store.state.todayAbnormalCount,
      correctionRate: store.healthScore,
    }
    abnormalRecords.value = []
    return
  }

  // 先根据当前筛选范围裁剪点位，再统一计算 good/abnormal/score，避免多套统计逻辑分叉。
  let scopedPoints: HistoryDataPoint[] = []
  if (selectedDateKey.value === 'range7') {
    // 7天视图用 weekData 的日期集合做裁剪，保证图表和列表口径一致。
    const dateSet = new Set(weekData.value.map(day => day.date))
    scopedPoints = historyPoints.value.filter(point => dateSet.has(pointDate(point)))
  } else if (selectedDateKey.value === 'today') {
    scopedPoints = historyPoints.value.filter(point => pointDate(point) === today)
  } else if (selectedDateKey.value === 'yesterday') {
    const yesterday = new Date()
    yesterday.setDate(yesterday.getDate() - 1)
    const yesterdayDate = formatLocalDate(yesterday)
    scopedPoints = historyPoints.value.filter(point => pointDate(point) === yesterdayDate)
  } else {
    scopedPoints = historyPoints.value.filter(point => pointDate(point) === selectedDateKey.value)
  }

  const total = scopedPoints.length
  const good = scopedPoints.filter(point => isGoodPosture(point.value)).length
  const abnormal = total - good
  const score = total > 0 ? Math.round((good / total) * 100) : 100

  dailyStats.value = {
    goodPostureTime: formatMinuteText(good),
    abnormalCount: Math.max(0, abnormal),
    correctionRate: score,
  }
  abnormalRecords.value = buildAbnormalRecords(scopedPoints)
}

function initWeekData() {
  const days = ['日', '一', '二', '三', '四', '五', '六']
  const data: DayData[] = []
  const baseScore = store.healthScore
  for (let i = 6; i >= 0; i--) {
    const d = new Date()
    d.setDate(d.getDate() - i)
    const dayOfWeek = d.getDay()
    const score = i === 0 ? baseScore : Math.max(60, baseScore - i * 2)
    data.push({
      label: days[dayOfWeek],
      score,
      date: formatLocalDate(d),
      source: 'fallback',
    })
  }
  weekData.value = data
}

async function fetchHistoryData() {
  try {
    // 拉取最近 7 天 isPosture 历史流，并按天聚合分数。
    const history = await queryPropertyHistory('isPosture', 7)
    historyPoints.value = history
    if (history.length > 0) {
      const dayScores: Record<string, { good: number; total: number }> = {}
      for (const point of history) {
        const date = pointDate(point)
        if (!dayScores[date]) {
          dayScores[date] = { good: 0, total: 0 }
        }
        dayScores[date].total++
        if (isGoodPosture(point.value)) {
          dayScores[date].good++
        }
      }
      // 仅覆盖有真实数据的天，未命中日期继续保留 fallback 分值，避免断档。
      weekData.value = weekData.value.map(day => {
        const ds = dayScores[day.date]
        if (ds && ds.total > 0) {
          return {
            ...day,
            score: Math.round((ds.good / ds.total) * 100),
            source: 'history',
          }
        }
        return day
      })
      hasRealHistory.value = weekData.value.some(day => day.source === 'history')
    } else {
      hasRealHistory.value = false
    }
  } catch (e) {
    hasRealHistory.value = false
    historyPoints.value = []
    console.warn('[History] 获取历史数据失败:', e)
  }
  applySelectionStats()
}

function showDatePicker() {
  // 日期筛选仅改变“视图口径”，不会触发额外网络请求。
  uni.showActionSheet({
    itemList: ['今天', '昨天', '最近7天'],
    success: (res) => {
      const now = new Date()
      if (res.tapIndex === 0) {
        selectedDateKey.value = 'today'
        selectedDate.value = formatLocalDate(now)
      } else if (res.tapIndex === 1) {
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
  selectedDateKey.value = day.date
  selectedDate.value = day.date
  applySelectionStats()
}

function getScoreColor(score: number): string {
  if (score >= 85) return '#00e676'
  if (score >= 70) return '#ffab00'
  return '#ff5252'
}

function getRecordIcon(type: string): string {
  const map: Record<string, string> = {
    head_down: '↓',
    hunchback: '⌒',
    tilt: '↗',
    abnormal: '!',
  }
  return map[type] || '!'
}

onShow(() => {
  initWeekData()
  applySelectionStats()
  fetchHistoryData()
})
</script>

<style lang="scss" scoped>
.container {
  padding: 40rpx 30rpx 60rpx;
  min-height: 100vh;
}

/* 页面头部 */
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

/* 统计网格 */
.stats-grid {
  display: flex;
  gap: 20rpx;
  margin-bottom: 40rpx;
  animation: slide-up var(--duration-slow) var(--ease-out) 100ms both;
  
  .stat-card {
    flex: 1;
    animation: card-enter var(--duration-slow) var(--ease-out) both;
    
    &:nth-child(1) { animation-delay: 100ms; }
    &:nth-child(2) { animation-delay: 150ms; }
    &:nth-child(3) { animation-delay: 200ms; }
  }
}

/* 区块标签 */
.section-label {
  display: flex;
  align-items: center;
  gap: 12rpx;
  margin-bottom: 20rpx;
  
  .label-icon {
    font-size: 24rpx;
    color: var(--neon-cyan);
    text-shadow: 0 0 10rpx var(--neon-cyan);
  }
  
  .label-text {
    font-size: 28rpx;
    font-weight: 700;
    color: var(--text-secondary);
    text-transform: uppercase;
    letter-spacing: 2rpx;
  }
  
  .data-source {
    margin-left: auto;
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

/* 趋势图表 */
.trend-section {
  margin-bottom: 40rpx;
  animation: slide-up var(--duration-slow) var(--ease-out) 200ms both;
  
  .trend-chart {
    background: linear-gradient(135deg, var(--bg-tertiary) 0%, var(--bg-secondary) 100%);
    border-radius: var(--radius-xl);
    padding: 32rpx;
    border: 1rpx solid var(--border-subtle);
  }
  
  .chart-legend {
    display: flex;
    justify-content: flex-end;
    gap: 24rpx;
    margin-bottom: 24rpx;
    
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
  
  .chart-bars {
    display: flex;
    justify-content: space-between;
    align-items: flex-end;
    height: 300rpx;
    padding: 20rpx 0;
    
    .bar-item {
      flex: 1;
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 12rpx;
      padding: 16rpx 8rpx;
      border-radius: var(--radius-md);
      transition: all var(--duration-base);
      
      &.active {
        background: var(--neon-cyan-dim);
        box-shadow: inset 0 0 0 2rpx var(--neon-cyan);
      }
      
      .bar-track {
        width: 100%;
        height: 200rpx;
        background: rgba(255, 255, 255, 0.05);
        border-radius: var(--radius-sm);
        position: relative;
        overflow: hidden;
        
        .bar-fill {
          position: absolute;
          bottom: 0;
          left: 0;
          right: 0;
          border-radius: var(--radius-sm);
          transition: height 0.8s var(--ease-spring);
          box-shadow: 0 0 20rpx currentColor;
        }
      }
      
      .bar-label {
        font-size: 22rpx;
        color: var(--text-tertiary);
      }
      
      .bar-value {
        font-size: 26rpx;
        font-weight: 700;
        color: var(--text-primary);
        font-family: var(--font-mono);
      }
    }
  }
}

/* 记录列表 */
.records-section {
  animation: slide-up var(--duration-slow) var(--ease-out) 300ms both;
  
  .records-list {
    display: flex;
    flex-direction: column;
    gap: 20rpx;
    
    .record-card {
      position: relative;
      display: flex;
      align-items: center;
      gap: 24rpx;
      padding: 28rpx;
      background: linear-gradient(135deg, var(--bg-tertiary) 0%, var(--bg-secondary) 100%);
      border-radius: var(--radius-lg);
      border: 1rpx solid var(--border-subtle);
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
        width: 80rpx;
        height: 80rpx;
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
          font-size: 30rpx;
          font-weight: 700;
          color: var(--text-primary);
          margin-bottom: 8rpx;
        }
        
        .record-desc {
          display: block;
          font-size: 24rpx;
          color: var(--text-tertiary);
        }
      }
      
      .record-time {
        padding: 10rpx 20rpx;
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

/* 动画 */
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

/* 小屏幕适配 */
@media screen and (max-width: 375px) {
  .container {
    padding: 30rpx 20rpx 50rpx;
  }
  
  .stats-grid {
    gap: 16rpx;
  }
  
  .trend-section {
    .trend-chart {
      padding: 24rpx;
      
      .chart-bars {
        height: 260rpx;
        
        .bar-item {
          .bar-track {
            height: 160rpx;
          }
        }
      }
    }
  }
  
  .records-section {
    .records-list {
      .record-card {
        padding: 20rpx;
        gap: 16rpx;
        
        .record-icon {
          width: 64rpx;
          height: 64rpx;
          
          text {
            font-size: 28rpx;
          }
        }
        
        .record-info {
          .record-title {
            font-size: 26rpx;
          }
          
          .record-desc {
            font-size: 22rpx;
          }
        }
      }
    }
  }
}
</style>
