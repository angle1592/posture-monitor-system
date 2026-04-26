/*
 * 模块职责：OneNET 云平台 HTTP API 封装层。
 *
 * 认证流程：
 * 1) 通过 `.env` 注入 `VITE_ONENET_PRODUCT_ID / VITE_ONENET_DEVICE_NAME / VITE_ONENET_TOKEN`。
 * 2) 请求时把 token 放入 `authorization` 请求头。
 * 3) 设备维度由 `product_id + device_name` 唯一确定，接口按该设备读写属性。
 *
 * 与其他模块关系：
 * - store.ts 通过本模块获取属性流、设备在线状态并下发控制指令。
 * - constants.ts 提供默认超时等协议参数，本模块负责网络交互与错误归一化。
 */

import { getMockDeviceStatus, getMockPropertyHistory, getMockRealtimeProperties } from './mockData'

const CONFIG = {
  // 物模型相关接口基地址：查询属性/历史、设置属性。
  thingmodelBase: 'https://iot-api.heclouds.com/thingmodel',
  // 设备详情接口基地址：用于判定在线状态。
  deviceBase: 'https://iot-api.heclouds.com/device',
  // 以下凭据来自 .env；为空时请求会失败并进入统一错误处理。
  productId: import.meta.env.VITE_ONENET_PRODUCT_ID || '',
  deviceName: import.meta.env.VITE_ONENET_DEVICE_NAME || '',
  token: import.meta.env.VITE_ONENET_TOKEN || '',
}

const USE_MOCK_HISTORY = String(import.meta.env.VITE_USE_MOCK_HISTORY ?? '').trim() === 'true'

// ===== 类型定义 =====

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

interface DeviceDetail {
  status: number  // 1=在线(设备已激活且连接中)
  last_time: string
  activate_time?: string
  enable_status?: boolean
}

function filterUnsupportedSetParams(
  params: Record<string, PropertyValue | PropertyValue[]>,
  blockedIdentifiers: Set<string>
): Record<string, PropertyValue | PropertyValue[]> {
  const filteredEntries = Object.entries(params).filter(([key]) => !blockedIdentifiers.has(key))
  const filtered = Object.fromEntries(filteredEntries) as Record<string, PropertyValue | PropertyValue[]>

  const droppedKeys = Object.keys(params).filter(
    (key) => !Object.prototype.hasOwnProperty.call(filtered, key)
  )
  if (droppedKeys.length > 0) {
    console.warn('[OneNET] 跳过本次无效属性:', droppedKeys.join(', '))
  }

  return filtered
}

function extractInvalidIdentifier(error: unknown): string | null {
  const message = error instanceof Error ? error.message : String(error)
  const match = message.match(/identifier:\s*([A-Za-z0-9_]+)/)
  return match?.[1] ?? null
}

function isIdentifierNotExistError(error: unknown): boolean {
  const message = error instanceof Error ? error.message : String(error)
  return message.includes('identifier not exist')
}

// ===== 通用请求 =====

function request<T>(
  url: string,
  method: 'GET' | 'POST' = 'GET',
  data?: Record<string, unknown>
): Promise<T> {
  // 统一错误策略：
  // - HTTP 非 200 -> 直接失败
  // - OneNET code 非 0 -> 按业务错误失败
  // - 网络失败 -> 返回网络错误
  return new Promise((resolve, reject) => {
    uni.request({
      url,
      method,
      data,
      header: {
        'authorization': CONFIG.token,
        'Content-Type': 'application/json',
      },
      success: (res: UniApp.RequestSuccessCallbackResult) => {
        if (res.statusCode !== 200) {
          reject(new Error(`HTTP ${res.statusCode}`))
          return
        }
        const body = res.data as { code?: number; msg?: string; data?: T } | undefined
        if (body && body.code !== undefined && body.code !== 0) {
          console.warn(`[OneNET] code=${body.code}: ${body.msg}`)
          reject(new Error(`${body.code}: ${body.msg}`))
          return
        }
        resolve((body?.data ?? null) as T)
      },
      fail: (err: UniApp.GeneralCallbackResult) => {
        reject(new Error(err.errMsg || '网络错误'))
      }
    })
  })
}

function qs(params: Record<string, string | number>): string {
  // 仅负责基础 query string 拼装，避免在各 API 方法里重复拼接。
  return Object.entries(params).map(([k, v]) => `${k}=${encodeURIComponent(v)}`).join('&')
}

// ===== API =====

/**
 * GET /thingmodel/query-device-property?product_id=...&device_name=...
 * 返回: PropertyItem[] (data直接是数组)
 */
export async function queryDeviceProperty(): Promise<PropertyItem[] | null> {
  if (USE_MOCK_HISTORY) {
    return getMockRealtimeProperties()
  }

  try {
    // 端点：GET /thingmodel/query-device-property
    // 返回：属性数组；异常时返回 null，交由上层走兜底逻辑。
    const q = qs({ product_id: CONFIG.productId, device_name: CONFIG.deviceName })
    const result = await request<PropertyItem[]>(`${CONFIG.thingmodelBase}/query-device-property?${q}`)
    return Array.isArray(result) ? result : null
  } catch (e) {
    console.error('[OneNET] 查询属性失败:', e)
    return null
  }
}

/**
 * POST /thingmodel/set-device-property
 * body: { product_id, device_name, params: { key: value } }
 */
export async function setDeviceProperty(
  params: Record<string, PropertyValue | PropertyValue[]>
): Promise<boolean> {
  // 端点：POST /thingmodel/set-device-property
  // 入参：params 为 { identifier: value } 的键值映射。
  // 返回：true=至少一次提交成功；false=失败（含重试失败）。
  const blockedIdentifiers = new Set<string>()
  let firstError: unknown = null
  try {
    const safeParams = filterUnsupportedSetParams(params, blockedIdentifiers)
    if (Object.keys(safeParams).length === 0) {
      console.warn('[OneNET] 未发送属性设置：全部字段未在云端物模型中定义')
      return false
    }

    await request(
      `${CONFIG.thingmodelBase}/set-device-property`,
      'POST',
      { product_id: CONFIG.productId, device_name: CONFIG.deviceName, params: safeParams }
    )
    return true
  } catch (e) {
    firstError = e
  }

  // OneNET 在 identifier 不存在时会整包失败。
  // 这里抽取坏字段并重试一次，尽量让其余合法字段成功下发。
  const invalidIdentifier = extractInvalidIdentifier(firstError)
  if (!invalidIdentifier || !isIdentifierNotExistError(firstError)) {
    console.error('[OneNET] 设置属性失败:', firstError)
    return false
  }

  blockedIdentifiers.add(invalidIdentifier)
  console.warn(`[OneNET] 标记本次无效属性: ${invalidIdentifier}`)

  const retryParams = filterUnsupportedSetParams(params, blockedIdentifiers)
  if (Object.keys(retryParams).length === 0) {
    console.error('[OneNET] 设置属性失败:', firstError)
    return false
  }

  try {
    await request(
      `${CONFIG.thingmodelBase}/set-device-property`,
      'POST',
      { product_id: CONFIG.productId, device_name: CONFIG.deviceName, params: retryParams }
    )
    return true
  } catch (retryError) {
    console.error('[OneNET] 设置属性失败(重试后):', retryError)
    return false
  }
}

/**
 * GET /thingmodel/query-device-property-history?...&start_time=...&end_time=...
 * 返回: { list: HistoryDataPoint[] }
 */
export async function queryPropertyHistory(
  identifier: string,
  days: number = 7
): Promise<HistoryDataPoint[]> {
  if (USE_MOCK_HISTORY) {
    return getMockPropertyHistory(identifier, days)
  }

  try {
    // 端点：GET /thingmodel/query-device-property-history
    // 返回：{ list: HistoryDataPoint[] }；异常时返回空数组。
    const end_time = Date.now()
    const start_time = end_time - days * 86400000
    const q = qs({
      product_id: CONFIG.productId,
      device_name: CONFIG.deviceName,
      identifier,
      start_time,
      end_time,
      limit: 200,
      sort: 'DESC',
    })
    const result = await request<{ list: HistoryDataPoint[] }>(
      `${CONFIG.thingmodelBase}/query-device-property-history?${q}`
    )
    return result?.list || []
  } catch (e) {
    console.error('[OneNET] 查询历史失败:', e)
    return []
  }
}

/**
 * GET /device/detail?product_id=...&device_name=...
 * 返回: DeviceDetail (status=1表示在线, last_time=最后通信时间)
 */
export async function queryDeviceStatus(): Promise<boolean> {
  if (USE_MOCK_HISTORY) {
    return getMockDeviceStatus()
  }

  try {
    // 端点：GET /device/detail
    // 判定策略：status=1 且 last_time 未超时，才视为在线。
    const q = qs({ product_id: CONFIG.productId, device_name: CONFIG.deviceName })
    const result = await request<DeviceDetail>(`${CONFIG.deviceBase}/detail?${q}`)
    if (result.status !== 1) {
      return false
    }

    if (!result.last_time) {
      return true
    }

    const lastMs = new Date(result.last_time).getTime()
    if (Number.isNaN(lastMs)) {
      return true
    }

    // 服务端 status=1 但时间戳过旧时，仍按离线处理，避免“伪在线”。
    return Date.now() - lastMs < 300000
  } catch (e) {
    console.warn('[OneNET] 查询设备状态失败:', e)
    return false
  }
}

// ===== 配置管理 =====

export function getConfig() {
  // 仅用于调试展示，避免直接泄露 token 明文。
  return {
    baseUrl: CONFIG.thingmodelBase,
    productId: CONFIG.productId,
    deviceName: CONFIG.deviceName,
    hasToken: !!CONFIG.token,
    useMockHistory: USE_MOCK_HISTORY,
  }
}

export function updateToken(newToken: string) {
  // 运行时切换 token（如设置页临时更新）并持久化到本地。
  CONFIG.token = newToken
  uni.setStorageSync('oneNetToken', newToken)
}

export function restoreToken() {
  // 启动阶段恢复本地 token，覆盖 .env 默认值。
  const saved = uni.getStorageSync('oneNetToken') as string
  if (saved) CONFIG.token = saved
}

export default {
  queryDeviceProperty,
  setDeviceProperty,
  queryPropertyHistory,
  queryDeviceStatus,
  getConfig,
  updateToken,
  restoreToken,
}
