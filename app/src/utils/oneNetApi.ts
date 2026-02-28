// OneNET IoT HTTP API — 已通过实际请求验证全部4个端点
// Base: iot-api.heclouds.com (物联网套件API，非openapi.heclouds.com)
// Auth: device-level token, res=products/{pid}/devices/{devname}, method=md5
// Credentials loaded from .env (VITE_ONENET_*)

import { DEVICE_DEFAULTS } from './constants'

const CONFIG = {
  thingmodelBase: 'https://iot-api.heclouds.com/thingmodel',
  deviceBase: 'https://iot-api.heclouds.com/device',
  productId: import.meta.env.VITE_ONENET_PRODUCT_ID || '',
  deviceName: import.meta.env.VITE_ONENET_DEVICE_NAME || '',
  token: import.meta.env.VITE_ONENET_TOKEN || '',
}

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
  return Object.entries(params).map(([k, v]) => `${k}=${encodeURIComponent(v)}`).join('&')
}

// ===== API =====

/**
 * GET /thingmodel/query-device-property?product_id=...&device_name=...
 * 返回: PropertyItem[] (data直接是数组)
 */
export async function queryDeviceProperty(): Promise<PropertyItem[] | null> {
  try {
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
  // 这里抽取出坏字段并重试一次，尽量让合法字段成功落云。
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
  try {
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
  try {
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
  return {
    baseUrl: CONFIG.thingmodelBase,
    productId: CONFIG.productId,
    deviceName: CONFIG.deviceName,
    hasToken: !!CONFIG.token,
  }
}

export function updateToken(newToken: string) {
  CONFIG.token = newToken
  uni.setStorageSync('oneNetToken', newToken)
}

export function restoreToken() {
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
