/**
 * 全局响应式状态管理
 * 所有页面共享的设备数据、连接状态、统计信息
 * 通过轮询OneNET API获取最新数据
 */

import { reactive, computed } from 'vue'
import { queryDeviceProperty, queryDeviceStatus, restoreToken } from './oneNetApi'
import type { PropertyItem } from './oneNetApi'
import { formatLocalDate } from './date'
import { PROP_IDS, POLLING_INTERVALS as PROTO_INTERVALS, SYSTEM_MODE_LABELS, DEVICE_DEFAULTS } from './constants'
type PollingProfile = 'background' | 'normal' | 'realtime'

interface LocalStats {
  date: string
  abnormalCount: number
  goodMinutes: number
  totalMinutes: number
}

interface LocalSettings {
  currentMode?: string
  monitoringEnabled?: boolean
}

function getErrorMessage(error: unknown): string {
  if (error instanceof Error) return error.message
  return '未知错误'
}

function parseCurrentMode(value: unknown): string | null {
  if (value === 0 || value === '0' || value === 'posture') return 'posture'
  if (value === 1 || value === '1' || value === 'clock') return 'clock'
  if (value === 2 || value === '2' || value === 'timer') return 'timer'
  return null
}

function parsePropertyTimestamp(value: number | string): number {
  if (typeof value === 'number') return value
  const parsed = new Date(value).getTime()
  return Number.isNaN(parsed) ? 0 : parsed
}

function inferOnlineFromProperties(props: PropertyItem[] | null, now: number): boolean {
  if (!props || props.length === 0) return false

  let latest = 0
  for (const item of props) {
    const ts = parsePropertyTimestamp(item.time)
    if (ts > latest) latest = ts
  }

  if (latest <= 0) return false
  // 属性流最近 5 分钟内有新时间戳，就把设备视为在线。
  // 这个兜底用于覆盖 detail 接口偶发延迟更新的情况。
  return now - latest < DEVICE_DEFAULTS.ONLINE_TIMEOUT_MS
}

const POLLING_INTERVALS: Record<PollingProfile, number> = {
  background: PROTO_INTERVALS.BACKGROUND,
  normal: PROTO_INTERVALS.NORMAL,
  realtime: PROTO_INTERVALS.REALTIME,
}

// ===== 状态定义 =====
interface AppState {
  // 设备连接状态
  isOnline: boolean
  lastCheckTime: number

  // 姿势数据
  isPosture: boolean       // true=正常坐姿, false=异常坐姿
  lastPostureTime: number  // 最后姿势数据时间戳(毫秒)

  // 今日统计数据 (本地累计)
  todayAbnormalCount: number
  todayGoodMinutes: number
  todayTotalMinutes: number
  usageRemainderMs: number

  // 控制状态 (从设备同步或本地设置)
  monitoringEnabled: boolean
  currentMode: string       // 'posture' | 'clock' | 'timer'

  // 轮询控制
  pollingTimer: ReturnType<typeof setInterval> | null
  isPolling: boolean
  pollingProfile: PollingProfile
  pollingIntervalMs: number
  appVisible: boolean

  // 加载/错误状态
  isLoading: boolean
  lastError: string | null
}

// ===== 响应式状态 =====
const state = reactive<AppState>({
  isOnline: false,
  lastCheckTime: 0,

  isPosture: true,
  lastPostureTime: 0,

  todayAbnormalCount: 0,
  todayGoodMinutes: 0,
  todayTotalMinutes: 0,
  usageRemainderMs: 0,

  monitoringEnabled: true,
  currentMode: 'posture',

  pollingTimer: null,
  isPolling: false,
  pollingProfile: 'normal',
  pollingIntervalMs: POLLING_INTERVALS.normal,
  appVisible: true,

  isLoading: false,
  lastError: null,
})

// ===== 计算属性 =====

/** 当前姿势文本 */
const postureText = computed(() => state.isPosture ? '良好' : '异常')

/** 当前姿势类型CSS类名 */
const postureType = computed(() => state.isPosture ? 'normal' : 'abnormal')

/** 今日健康评分 (0-100) */
const healthScore = computed(() => {
  if (state.todayTotalMinutes === 0) return 100
  return Math.round((state.todayGoodMinutes / state.todayTotalMinutes) * 100)
})

/** 最后更新时间文本 */
const lastUpdateTimeText = computed(() => {
  if (state.lastPostureTime === 0) return '暂无数据'
  const d = new Date(state.lastPostureTime)
  return d.toLocaleTimeString('zh-CN', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  })
})

/** 今日使用时长文本 */
const usageTimeText = computed(() => {
  const m = state.todayTotalMinutes
  if (m === 0) return '0分钟'
  const hours = Math.floor(m / 60)
  const mins = m % 60
  if (hours > 0) return `${hours}小时${mins}分`
  return `${mins}分钟`
})

/** 当前模式文本 */
const modeText = computed(() => {
  return SYSTEM_MODE_LABELS[state.currentMode] || state.currentMode
})

// ===== 操作方法 =====

/** 初始化: 恢复token和本地统计 */
function init() {
  restoreToken()
  loadLocalStats()
  loadLocalSettings()
  console.log('[Store] 初始化完成')
}

function accumulateUsage(elapsedMs: number) {
  if (!state.monitoringEnabled) return
  if (elapsedMs <= 0) return

  // 用余数累计避免“每轮询不足 1 分钟”时丢失时长。
  const totalMs = state.usageRemainderMs + elapsedMs
  const addMinutes = Math.floor(totalMs / 60000)
  state.usageRemainderMs = totalMs % 60000

  if (addMinutes <= 0) return

  state.todayTotalMinutes += addMinutes
  if (state.isPosture) {
    state.todayGoodMinutes += addMinutes
  }
  saveLocalStats()
}

/** 从OneNET获取最新设备数据 */
async function fetchLatest() {
  state.isLoading = true
  state.lastError = null
  const now = Date.now()
  const elapsedMs = state.lastCheckTime > 0 ? now - state.lastCheckTime : 0
  try {
    const [props, status] = await Promise.all([
      queryDeviceProperty(),
      queryDeviceStatus(),
    ])

    if (props && props.length > 0) {
      // 从属性列表中查找 isPosture
      const postureProp = props.find((p: PropertyItem) => p.identifier === PROP_IDS.IS_POSTURE)
      if (postureProp) {
        const oldPosture = state.isPosture
        state.isPosture = postureProp.value === true || postureProp.value === 'true'
        state.lastPostureTime = typeof postureProp.time === 'number'
          ? postureProp.time
          : new Date(postureProp.time).getTime() || Date.now()

        // 如果从正常变为异常，增加异常计数
        if (oldPosture && !state.isPosture) {
          state.todayAbnormalCount++
          saveLocalStats()
        }
      }

      // 解析 monitoringEnabled 属性（如果存在）
      const monProp = props.find((p: PropertyItem) => p.identifier === PROP_IDS.MONITORING_ENABLED)
      if (monProp) {
        state.monitoringEnabled = monProp.value === true || monProp.value === 'true'
      }

      const modeProp = props.find((p: PropertyItem) => p.identifier === PROP_IDS.CURRENT_MODE)
      if (modeProp) {
        const parsedMode = parseCurrentMode(modeProp.value)
        if (parsedMode) {
          state.currentMode = parsedMode
        }
      }
    }

    const onlineByPropertyStream = inferOnlineFromProperties(props, now)
    state.isOnline = status || onlineByPropertyStream
    accumulateUsage(elapsedMs)
    state.lastCheckTime = now
  } catch (e: unknown) {
    state.lastError = getErrorMessage(e)
    state.isOnline = false
    console.error('[Store] 获取数据失败:', e)
  } finally {
    state.isLoading = false
  }
}

function getEffectivePollingProfile(): PollingProfile {
  if (!state.appVisible) return 'background'
  return state.pollingProfile
}

function schedulePollingTimer(intervalMs: number, fetchImmediately: boolean) {
  if (state.pollingTimer) {
    clearInterval(state.pollingTimer)
    state.pollingTimer = null
  }

  if (fetchImmediately) {
    fetchLatest()
  }

  state.pollingTimer = setInterval(() => {
    fetchLatest()
  }, intervalMs)

  state.isPolling = true
  state.pollingIntervalMs = intervalMs
}

function applyPollingPolicy(fetchImmediately: boolean = false) {
  const effectiveProfile = getEffectivePollingProfile()
  const nextInterval = POLLING_INTERVALS[effectiveProfile]
  const needsReschedule = !state.isPolling || state.pollingIntervalMs !== nextInterval

  // 轮询间隔没变化就不重建 timer，避免不必要抖动。
  if (!needsReschedule) return

  schedulePollingTimer(nextInterval, fetchImmediately)
  console.log('[Store] 轮询策略切换:', effectiveProfile, `${nextInterval}ms`)
}

/** 开始定时轮询 */
function startPolling(intervalMs: number = 5000) {
  state.pollingIntervalMs = intervalMs
  schedulePollingTimer(intervalMs, true)
  console.log('[Store] 开始轮询, 间隔:', intervalMs, 'ms')
}

/** 停止轮询 */
function stopPolling() {
  if (state.pollingTimer) {
    clearInterval(state.pollingTimer)
    state.pollingTimer = null
  }
  state.isPolling = false
  console.log('[Store] 停止轮询')
}

function setPollingProfile(profile: PollingProfile, fetchImmediately: boolean = true) {
  state.pollingProfile = profile
  applyPollingPolicy(fetchImmediately)
}

function setAppVisibility(visible: boolean) {
  state.appVisible = visible
  applyPollingPolicy(visible)
}

/** 保存今日统计到本地存储 */
function saveLocalStats() {
  const today = formatLocalDate(new Date())
  uni.setStorageSync('postureStats', {
    date: today,
    abnormalCount: state.todayAbnormalCount,
    goodMinutes: state.todayGoodMinutes,
    totalMinutes: state.todayTotalMinutes,
  })
}

/** 从本地存储恢复今日统计 */
function loadLocalStats() {
  try {
    const saved = uni.getStorageSync('postureStats') as LocalStats | undefined
    if (saved) {
      const today = formatLocalDate(new Date())
      if (saved.date === today) {
        state.todayAbnormalCount = saved.abnormalCount || 0
        state.todayGoodMinutes = saved.goodMinutes || 0
        state.todayTotalMinutes = saved.totalMinutes || 0
      }
      // 如果日期不匹配，保持默认值0
    }
  } catch (e) {
    console.error('[Store] 加载本地统计失败:', e)
  }
}

/** 保存控制设置到本地 */
function saveLocalSettings() {
  uni.setStorageSync('controlSettings', {
    currentMode: state.currentMode,
    monitoringEnabled: state.monitoringEnabled,
  })
}

/** 从本地恢复控制设置 */
function loadLocalSettings() {
  try {
    const saved = uni.getStorageSync('controlSettings') as LocalSettings | undefined
    if (saved) {
      state.currentMode = saved.currentMode || 'posture'
      state.monitoringEnabled = saved.monitoringEnabled ?? true
    }
  } catch (e) {
    console.error('[Store] 加载本地设置失败:', e)
  }
}

/** 重置今日统计 */
function resetTodayStats() {
  state.todayAbnormalCount = 0
  state.todayGoodMinutes = 0
  state.todayTotalMinutes = 0
  state.usageRemainderMs = 0
  saveLocalStats()
}

// ===== 导出 =====
export const store = reactive({
  // 状态
  state,

  // 计算属性
  postureText,
  postureType,
  healthScore,
  lastUpdateTimeText,
  usageTimeText,
  modeText,

  // 方法
  init,
  fetchLatest,
  startPolling,
  stopPolling,
  setPollingProfile,
  setAppVisibility,
  saveLocalStats,
  saveLocalSettings,
  resetTodayStats,
})

export default store
