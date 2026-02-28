# OneNET 接口文档

本文档对应 `src/utils/oneNetApi.ts`，用于说明当前项目调用的 OneNET HTTP 接口、参数和返回语义。

## 基础信息

- Base URL (ThingModel): `https://iot-api.heclouds.com/thingmodel`
- Base URL (Device): `https://iot-api.heclouds.com/device`
- 认证方式: 请求头 `authorization: <token>`
- Content-Type: `application/json`
- 设备定位参数:
  - `product_id`
  - `device_name`

## 通用响应约定

封装层会先检查 HTTP 状态码，再检查业务码:

- HTTP 非 `200` -> 视为请求失败
- body 中 `code` 存在且不为 `0` -> 视为业务失败
- 成功时使用 `body.data` 作为函数返回值

## 类型定义

```ts
export type PropertyValue = boolean | number | string | null

export interface PropertyItem {
  identifier: string
  time: number | string
  value: PropertyValue
  data_type?: string
  access_mode?: string
  name?: string
  description?: string
}

export interface HistoryDataPoint {
  time: number | string
  value: PropertyValue
}
```

---

## 1) 查询设备属性

- 函数: `queryDeviceProperty()`
- 方法: `GET`
- 路径: `/thingmodel/query-device-property`
- Query 参数:
  - `product_id`
  - `device_name`
- 返回: `Promise<PropertyItem[] | null>`

### 示例请求

```http
GET /thingmodel/query-device-property?product_id=<pid>&device_name=<name>
```

### 说明

- 成功返回属性数组。
- 请求异常或业务失败时返回 `null`（并输出日志）。

---

## 2) 设置设备属性

- 函数: `setDeviceProperty(params)`
- 方法: `POST`
- 路径: `/thingmodel/set-device-property`
- Body:

```json
{
  "product_id": "<pid>",
  "device_name": "<name>",
  "params": {
    "isPosture": true,
    "cooldownMs": 30000
  }
}
```

- 返回: `Promise<boolean>`

### 说明

- 接口调用成功返回 `true`。
- 异常返回 `false`（并输出日志）。

---

## 3) 查询属性历史

- 函数: `queryPropertyHistory(identifier, days = 7)`
- 方法: `GET`
- 路径: `/thingmodel/query-device-property-history`
- Query 参数:
  - `product_id`
  - `device_name`
  - `identifier`
  - `start_time` (ms)
  - `end_time` (ms)
  - `limit` (默认 200)
  - `sort` (默认 `DESC`)
- 返回: `Promise<HistoryDataPoint[]>`

### 示例请求

```http
GET /thingmodel/query-device-property-history?product_id=<pid>&device_name=<name>&identifier=isPosture&start_time=...&end_time=...&limit=200&sort=DESC
```

### 说明

- 成功返回历史点数组。
- 无数据时返回空数组 `[]`。
- 异常时返回空数组 `[]`（并输出日志）。

---

## 4) 查询设备在线状态

- 函数: `queryDeviceStatus()`
- 方法: `GET`
- 路径: `/device/detail`
- Query 参数:
  - `product_id`
  - `device_name`
- 返回: `Promise<boolean>`

### 在线判定规则

- `status === 1`
- 且 `last_time` 距当前时间小于 120 秒

满足以上条件判定为在线，否则离线。

---

## 配置相关函数

- `getConfig()`
  - 返回当前基础配置摘要（`baseUrl/productId/deviceName/hasToken`）
- `updateToken(newToken)`
  - 更新内存 token 并写入 `uni.setStorageSync('oneNetToken', newToken)`
- `restoreToken()`
  - 从 `uni.getStorageSync('oneNetToken')` 恢复 token

---

## 与状态层的关系

`src/utils/store.ts` 主要依赖以下函数:

- 轮询主路径: `queryDeviceProperty()`
- 属性为空时兜底在线检查: `queryDeviceStatus()`
- 控制页写入: `setDeviceProperty()`
- 历史页趋势: `queryPropertyHistory()`

因此 API 层和 Store 层构成了当前项目的数据主链路:

`OneNET -> oneNetApi.ts -> store.ts -> pages/*.vue`
