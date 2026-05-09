/**
 * OneNET 历史数据 API 测试
 *
 * 目的：验证 OneNET 历史 API 能否返回足够的历史数据供回填
 */
import { describe, expect, it } from 'vitest'

const ONENET_BASE = 'https://iot-api.heclouds.com/thingmodel'
const PRODUCT_ID = 'DX0LtPPJQ9'
const DEVICE_NAME = 'main'
const TOKEN = 'version=2018-10-31&res=products%2FDX0LtPPJQ9%2Fdevices%2Fmain&et=1866012442&method=md5&sign=pTkwfP7V2u5Lh%2FRVlscf%2Bw%3D%3D'

async function queryOneNETHistory(days: number): Promise<any[]> {
  const end_time = Date.now()
  const start_time = end_time - days * 86400000
  const url = `${ONENET_BASE}/query-device-property-history?product_id=${PRODUCT_ID}&device_name=${DEVICE_NAME}&identifier=postureType&start_time=${start_time}&end_time=${end_time}&limit=1000&sort=DESC`

  const resp = await fetch(url, { headers: { authorization: TOKEN } })
  const body = await resp.json()
  return body?.data?.list ?? []
}

describe('OneNET 历史数据', () => {
  it('可以拉取6天数据', async () => {
    const list = await queryOneNETHistory(6)
    console.log(`OneNET 6天: ${list.length}条`)
    expect(list.length).toBeGreaterThan(0)
  })

  it('可以拉取1天数据', async () => {
    const list = await queryOneNETHistory(1)
    console.log(`OneNET 1天: ${list.length}条`)
    expect(list.length).toBeGreaterThan(0)
  })

  it('OneNET 数据格式', async () => {
    const list = await queryOneNETHistory(1)
    const item = list[0]
    console.log(`  time=${item.time} value="${item.value}"`)
    expect(item).toHaveProperty('time')
    expect(item).toHaveProperty('value')
    // value 是字符串数字 '0','1','2','3'
    expect(['0', '1', '2', '3']).toContain(String(item.value))
  })
})
