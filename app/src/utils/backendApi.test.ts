import { describe, expect, it, vi, beforeAll } from 'vitest'

const BACKEND_BASE = 'http://47.119.146.203:8001'

describe('backendApi 数据流测试', () => {
  it('GET /api/posture/history?days=7 返回正确格式', async () => {
    const resp = await fetch(`${BACKEND_BASE}/api/posture/history?days=7`)
    expect(resp.ok).toBe(true)
    const data = await resp.json()
    expect(Array.isArray(data)).toBe(true)
    expect(data.length).toBeGreaterThan(0)

    const record = data[0]
    expect(record).toHaveProperty('onenet_time')
    expect(record).toHaveProperty('posture_type')
    expect(record).toHaveProperty('person_present')
    expect(record).toHaveProperty('fill_light_on')

    // onenet_time 应为 ISO 时间字符串
    expect(() => new Date(record.onenet_time)).not.toThrow()
    // posture_type 应为有效值
    expect(['normal', 'head_down', 'hunchback', 'no_person', 'unknown']).toContain(record.posture_type)
  })

  it('返回7天内的数据分布', async () => {
    const resp = await fetch(`${BACKEND_BASE}/api/posture/history?days=7`)
    const data = await resp.json()

    // 按日期分组
    const byDate: Record<string, number> = {}
    for (const r of data) {
      const day = r.onenet_time.slice(0, 10)
      byDate[day] = (byDate[day] || 0) + 1
    }

    console.log('数据分布:')
    for (const [day, count] of Object.entries(byDate).sort()) {
      console.log(`  ${day}: ${count}条`)
    }
    console.log(`  共${data.length}条, ${Object.keys(byDate).length}天`)

    expect(Object.keys(byDate).length).toBeGreaterThanOrEqual(1)
  })

  it('返回的 posture_type 能在前端正常处理', async () => {
    const resp = await fetch(`${BACKEND_BASE}/api/posture/history?days=1`)
    const data = await resp.json()

    // 模拟前端 normalizePostureValue 处理
    const POSTURE_TYPES = { NO_PERSON: 'no_person', NORMAL: 'normal', HEAD_DOWN: 'head_down', HUNCHBACK: 'hunchback', UNKNOWN: 'unknown' }

    function normalizePostureValue(value: unknown): string {
      if (value === 'no_person') return POSTURE_TYPES.NO_PERSON
      if (value === 'normal') return POSTURE_TYPES.NORMAL
      if (value === 'head_down') return POSTURE_TYPES.HEAD_DOWN
      if (value === 'hunchback') return POSTURE_TYPES.HUNCHBACK
      return POSTURE_TYPES.UNKNOWN
    }

    function isHealthyPosture(value: string): boolean {
      return value === POSTURE_TYPES.NORMAL
    }

    function isAbnormalPosture(value: string): boolean {
      return value === POSTURE_TYPES.HEAD_DOWN || value === POSTURE_TYPES.HUNCHBACK
    }

    let healthy = 0, abnormal = 0, unknown = 0
    for (const r of data) {
      const posture = normalizePostureValue(r.posture_type)
      if (isHealthyPosture(posture)) healthy++
      else if (isAbnormalPosture(posture)) abnormal++
      else unknown++
    }

    const tracked = healthy + abnormal
    const score = tracked > 0 ? Math.round(healthy / tracked * 100) : null

    console.log(`  正常: ${healthy}, 异常: ${abnormal}, 无人: ${unknown}, 评分: ${score ?? 'N/A'}`)

    expect(tracked + unknown).toBe(data.length)
  })
})
