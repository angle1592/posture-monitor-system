/*
 * 模块职责：设备数据 API 封装层。
 *
 * 数据源已从 OneNET 切换到自建 FastAPI 后端。
 * 保持与原 oneNetApi.ts 相同的导出接口，store.ts 无需修改。
 */

import {
  queryDeviceProperty as _queryDeviceProperty,
  queryDeviceStatus as _queryDeviceStatus,
  queryPropertyHistory as _queryPropertyHistory,
  setDeviceProperty as _setDeviceProperty,
} from './backendApi'
import type { PropertyItem, PropertyValue, HistoryDataPoint } from './backendApi'

// 重新导出类型（兼容现有导入）
export type { PropertyItem, PropertyValue, HistoryDataPoint }

// ===== API（委托给 backendApi） =====

export async function queryDeviceProperty(): Promise<PropertyItem[] | null> {
  return _queryDeviceProperty()
}

export async function queryDeviceStatus(): Promise<boolean> {
  return _queryDeviceStatus()
}

export async function queryPropertyHistory(
  identifier: string,
  days: number = 7
): Promise<HistoryDataPoint[]> {
  return _queryPropertyHistory(identifier, days)
}

export async function setDeviceProperty(
  params: Record<string, PropertyValue | PropertyValue[]>
): Promise<boolean> {
  return _setDeviceProperty(params)
}

// ===== 配置管理（保留兼容） =====

export function getConfig() {
  return {
    baseUrl: 'fastapi-backend',
    productId: '',
    deviceName: '',
    hasToken: true,
  }
}

export function updateToken(_newToken: string) {
  console.warn('[OneNET] Token management disabled (using backend)')
}

export function restoreToken() {
  // 后端模式下不需要恢复 token
}

export default {
  queryDeviceProperty,
  queryDeviceStatus,
  queryPropertyHistory,
  setDeviceProperty,
  getConfig,
  updateToken,
  restoreToken,
}
