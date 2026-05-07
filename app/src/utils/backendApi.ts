/**
 * 模块职责：FastAPI 后端 API 客户端。
 *
 * 作为 oneNetApi.ts 的替代数据源，提供相同语义的接口。
 * 部署时通过 VITE_API_BASE_URL 指向后端地址。
 */

const API_BASE = import.meta.env.VITE_API_BASE_URL || 'http://localhost:8000'

// ===== 类型定义 =====

/** 与 oneNetApi.PropertyItem 兼容的属性条目 */
export interface PropertyItem {
  identifier: string
  time: number | string
  value: PropertyValue
  data_type?: string
  access_mode?: string
  name?: string
  description?: string
}

export type PropertyValue = boolean | number | string | null

export interface HistoryDataPoint {
  time: number | string
  value: PropertyValue
}

/** 后端 /api/posture/latest 返回的记录 */
interface PostureRecordResponse {
  id: number
  device_id: string
  posture_type: string
  person_present: boolean
  ambient_lux: number | null
  fill_light_on: boolean
  onenet_time: string
  created_at: string
}

/** 后端 /api/posture/stats/daily 返回 */
export interface DailyStats {
  good_posture_minutes: number
  abnormal_count: number
  health_score: number
}

/** 后端 /api/posture/stats/weekly 返回 */
export interface WeekTrendDay {
  date: string
  score: number | null
}

/** 后端 /api/device/status 返回 */
export interface DeviceStatus {
  online: boolean
  last_time: string | null
}

// ===== 通用请求 =====

async function request<T>(path: string): Promise<T> {
  const resp = await fetch(`${API_BASE}${path}`)
  if (!resp.ok) {
    throw new Error(`HTTP ${resp.status}`)
  }
  return resp.json()
}

// ===== 姿态名 → 数字映射（与 OneNET 物模型一致）=====
const POSTURE_TO_NUMBER: Record<string, number> = {
  normal: 0,
  head_down: 1,
  hunchback: 2,
  no_person: 3,
  unknown: 4,
}

// ===== API =====

/**
 * 获取最新设备属性（兼容 oneNetApi.queryDeviceProperty 接口）。
 *
 * 后端返回单条记录，转换为 PropertyItem[] 格式供 store.ts 消费。
 */
export async function queryDeviceProperty(): Promise<PropertyItem[] | null> {
  try {
    const record = await request<PostureRecordResponse>('/api/posture/latest')
    const ts = new Date(record.onenet_time).getTime()
    return [
      { identifier: 'postureType', value: POSTURE_TO_NUMBER[record.posture_type] ?? 4, time: ts },
      { identifier: 'personPresent', value: record.person_present, time: ts },
      { identifier: 'ambientLux', value: record.ambient_lux, time: ts },
      { identifier: 'fillLightOn', value: record.fill_light_on, time: ts },
      // 以下字段后端不提供，给默认值保持 App 不报错
      { identifier: 'monitoringEnabled', value: true, time: ts },
      { identifier: 'currentMode', value: 0, time: ts },
      { identifier: 'isPosture', value: record.posture_type === 'normal', time: ts },
    ]
  } catch (e) {
    console.error('[BackendAPI] queryDeviceProperty failed:', e)
    return null
  }
}

/**
 * 获取设备在线状态（兼容 oneNetApi.queryDeviceStatus 接口）。
 */
export async function queryDeviceStatus(): Promise<boolean> {
  try {
    const data = await request<DeviceStatus>('/api/device/status')
    return data.online
  } catch (e) {
    console.error('[BackendAPI] queryDeviceStatus failed:', e)
    return false
  }
}

/**
 * 获取历史数据（兼容 oneNetApi.queryPropertyHistory 接口）。
 */
export async function queryPropertyHistory(
  _identifier: string,
  days: number = 7
): Promise<HistoryDataPoint[]> {
  try {
    const records = await request<PostureRecordResponse[]>(
      `/api/posture/history?days=${days}`
    )
    return records.map((r) => ({
      time: new Date(r.onenet_time).getTime(),
      value: POSTURE_TO_NUMBER[r.posture_type] ?? 4,
    }))
  } catch (e) {
    console.error('[BackendAPI] queryPropertyHistory failed:', e)
    return []
  }
}

/**
 * 获取当日统计（新增接口，替代本地计算）。
 */
export async function getDailyStats(date?: string): Promise<DailyStats> {
  try {
    const qs = date ? `?date=${date}` : ''
    return await request<DailyStats>(`/api/posture/stats/daily${qs}`)
  } catch (e) {
    console.error('[BackendAPI] getDailyStats failed:', e)
    return { good_posture_minutes: 0, abnormal_count: 0, health_score: 100 }
  }
}

/**
 * 获取 7 天趋势（新增接口）。
 */
export async function getWeeklyTrend(): Promise<WeekTrendDay[]> {
  try {
    return await request<WeekTrendDay[]>('/api/posture/stats/weekly')
  } catch (e) {
    console.error('[BackendAPI] getWeeklyTrend failed:', e)
    return []
  }
}

/**
 * 设置设备属性（透传到 OneNET，通过后端转发）。
 * TODO: 后端尚未实现此接口，暂不可用
 */
export async function setDeviceProperty(
  _params: Record<string, PropertyValue | PropertyValue[]>
): Promise<boolean> {
  console.warn('[BackendAPI] setDeviceProperty not yet implemented on backend')
  return false
}

export default {
  queryDeviceProperty,
  queryDeviceStatus,
  queryPropertyHistory,
  getDailyStats,
  getWeeklyTrend,
  setDeviceProperty,
}
