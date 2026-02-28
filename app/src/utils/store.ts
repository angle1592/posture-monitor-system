/*
 * 模块职责：
 * 1) 作为应用级状态中心，维护设备在线状态、姿势数据、统计数据与控制状态。
 * 2) 统一封装页面可直接消费的计算属性，避免页面重复实现业务换算逻辑。
 * 3) 负责 OneNET 轮询调度与本地存储同步（统计与控制设置）。
 *
 * 与其他模块的关系与数据流：
 * - 输入：通过 oneNetApi.ts 拉取设备属性/在线状态；通过 uni 本地存储恢复历史统计与设置。
 * - 规则：通过 constants.ts 读取属性标识、轮询间隔、在线超时等协议常量。
 * - 工具：通过 date.ts 统一日期键格式，保证本地统计按“自然日”归档。
 * - 输出：页面（index/monitor/control/history）直接消费 state、computed 与 action。
 */

import { reactive, computed } from 'vue'
import { queryDeviceProperty, queryDeviceStatus, restoreToken } from './oneNetApi'
import type { PropertyItem } from './oneNetApi'
import { formatLocalDate } from './date'
import { PROP_IDS, POLLING_INTERVALS as PROTO_INTERVALS, SYSTEM_MODE_LABELS, DEVICE_DEFAULTS } from './constants'
// 轮询档位：由页面可见性和业务场景共同决定请求频率。
type PollingProfile = 'background' | 'normal' | 'realtime'

interface LocalStats {
  // 本地统计归档日期（yyyy-MM-dd），用于判断是否与“今天”同一天。
  date: string
  // 当天异常姿势累计次数（由姿势从正常切换到异常时递增）。
  abnormalCount: number
  // 当天良好姿势累计分钟数（按轮询间隔折算）。
  goodMinutes: number
  // 当天总监测分钟数（监控开启时累计）。
  totalMinutes: number
}

interface LocalSettings {
  // 最近一次选择的系统模式（posture/clock/timer）。
  currentMode?: string
  // 最近一次监控开关状态。
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
  // 后台档：页面不可见时降低请求频率，减少电量与流量消耗。
  background: PROTO_INTERVALS.BACKGROUND,
  // 常规档：普通页面展示所用默认频率。
  normal: PROTO_INTERVALS.NORMAL,
  // 实时档：监控/控制页面使用，优先保证实时反馈。
  realtime: PROTO_INTERVALS.REALTIME,
}

// ===== 状态定义 =====
interface AppState {
  // 设备连接状态
  isOnline: boolean           // 当前设备在线判定（detail接口 + 属性流时间戳双通道兜底）
  lastCheckTime: number       // 最近一次完成拉取的本地时间戳（毫秒）

  // 姿势数据
  isPosture: boolean           // true=正常坐姿, false=异常坐姿；来自 OneNET 的 isPosture 属性
  lastPostureTime: number      // 最后一次姿势属性时间戳（毫秒），用于“最近更新时间”展示

  // 今日统计数据 (本地累计)
  todayAbnormalCount: number   // 今日异常次数（姿势边沿触发计数）
  todayGoodMinutes: number     // 今日良好分钟数（按轮询增量累计）
  todayTotalMinutes: number    // 今日总监测分钟数（监控开启时累计）
  usageRemainderMs: number     // 分钟换算余数（毫秒），避免小于1分钟的采样损失

  // 控制状态 (从设备同步或本地设置)
  monitoringEnabled: boolean   // 监控开关，优先由云端属性同步；失败时使用本地回显
  currentMode: string           // 当前模式：'posture' | 'clock' | 'timer'

  // 轮询控制
  pollingTimer: ReturnType<typeof setInterval> | null // 轮询定时器句柄
  isPolling: boolean          // 是否处于轮询中
  pollingProfile: PollingProfile // 当前业务请求档位（页面可主动切换）
  pollingIntervalMs: number   // 当前实际生效间隔（毫秒）
  appVisible: boolean         // 应用/页面可见性（用于后台降频）

  // 加载/错误状态
  isLoading: boolean           // 当前是否正在拉取设备数据
  lastError: string | null     // 最近一次请求错误信息（用于页面提示或调试）
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
  // 先恢复鉴权信息，确保后续请求可直接访问 OneNET。
  restoreToken()
  // 恢复“当天统计”与“控制设置”，用于首屏快速回显。
  loadLocalStats()
  loadLocalSettings()
  console.log('[Store] 初始化完成')
}

function accumulateUsage(elapsedMs: number) {
  // 仅在“监控开启”时累计使用时长，避免暂停期间误记。
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
  // 数据流：queryDeviceProperty/queryDeviceStatus -> 归一化状态 -> 本地统计落盘。
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
      // 属性 ID 映射说明：
      // - PROP_IDS.IS_POSTURE -> isPosture（坐姿是否正常）
      // - PROP_IDS.MONITORING_ENABLED -> monitoringEnabled（监控开关）
      // - PROP_IDS.CURRENT_MODE -> currentMode（系统模式：0/1/2）
      // 其余如 alertModeMask/cooldownMs/timerDurationSec 等主要在控制页下发。
      // 通过 PROP_IDS.IS_POSTURE 映射云端属性，统一姿势状态来源。
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

      // 通过 PROP_IDS.MONITORING_ENABLED 同步设备端监控开关。
      const monProp = props.find((p: PropertyItem) => p.identifier === PROP_IDS.MONITORING_ENABLED)
      if (monProp) {
        state.monitoringEnabled = monProp.value === true || monProp.value === 'true'
      }

      // 通过 PROP_IDS.CURRENT_MODE 同步系统模式（posture/clock/timer）。
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
  // 重建前先清理旧 timer，避免多重轮询并发。
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
  // 生效规则：页面不可见强制 background，否则按当前 pollingProfile。
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
  // 外部显式指定间隔时，直接按该间隔启动（常用于应用启动阶段）。
  state.pollingIntervalMs = intervalMs
  schedulePollingTimer(intervalMs, true)
  console.log('[Store] 开始轮询, 间隔:', intervalMs, 'ms')
}

/** 停止轮询 */
function stopPolling() {
  // 副作用：清理 timer 并将 isPolling 标记为 false。
  if (state.pollingTimer) {
    clearInterval(state.pollingTimer)
    state.pollingTimer = null
  }
  state.isPolling = false
  console.log('[Store] 停止轮询')
}

function setPollingProfile(profile: PollingProfile, fetchImmediately: boolean = true) {
  // 页面可按场景切档：monitor/control 常用 realtime，离开后恢复 normal。
  state.pollingProfile = profile
  applyPollingPolicy(fetchImmediately)
}

function setAppVisibility(visible: boolean) {
  // 可见性变化会触发策略重算：后台降频、前台恢复。
  state.appVisible = visible
  applyPollingPolicy(visible)
}

/** 保存今日统计到本地存储 */
function saveLocalStats() {
  // 存储键 postureStats：按自然日保存，跨天后由 loadLocalStats 自动归零。
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
      // 日期不匹配说明已跨天，保持默认值 0 并等待新一天重新累计。
    }
  } catch (e) {
    console.error('[Store] 加载本地统计失败:', e)
  }
}

/** 保存控制设置到本地 */
function saveLocalSettings() {
  // 存储键 controlSettings：用于控制页和首页重启后的体验一致性。
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
  // 仅清空当天累计，不影响实时姿势和在线状态。
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
