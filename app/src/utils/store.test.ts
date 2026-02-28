import { beforeEach, describe, expect, it, vi } from 'vitest'

vi.mock('./oneNetApi', () => ({
  queryDeviceProperty: vi.fn(),
  queryDeviceStatus: vi.fn(),
  restoreToken: vi.fn(),
}))

import store from './store'
import { queryDeviceProperty, queryDeviceStatus } from './oneNetApi'

const mockedQueryDeviceProperty = vi.mocked(queryDeviceProperty)
const mockedQueryDeviceStatus = vi.mocked(queryDeviceStatus)

function resetStoreState() {
  store.state.isOnline = false
  store.state.lastCheckTime = 0
  store.state.isPosture = true
  store.state.lastPostureTime = 0
  store.state.todayAbnormalCount = 0
  store.state.todayGoodMinutes = 0
  store.state.todayTotalMinutes = 0
  store.state.usageRemainderMs = 0
  store.state.monitoringEnabled = true
  store.state.currentMode = 'posture'
  store.state.isLoading = false
  store.state.lastError = null
}

describe('store.fetchLatest', () => {
  beforeEach(() => {
    vi.useRealTimers()
    mockedQueryDeviceProperty.mockReset()
    mockedQueryDeviceStatus.mockReset()

    ;(globalThis as unknown as { uni: { setStorageSync: ReturnType<typeof vi.fn>; getStorageSync: ReturnType<typeof vi.fn> } }).uni = {
      setStorageSync: vi.fn(),
      getStorageSync: vi.fn(),
    }

    resetStoreState()
  })

  it('keeps online when property stream is fresh even if status endpoint says offline', async () => {
    mockedQueryDeviceProperty.mockResolvedValueOnce([
      { identifier: 'isPosture', value: true, time: Date.now() },
    ])
    mockedQueryDeviceStatus.mockResolvedValueOnce(false)

    await store.fetchLatest()

    expect(store.state.isOnline).toBe(true)
  })

  it('accumulates usage minutes while monitoring is enabled', async () => {
    vi.useFakeTimers()
    vi.setSystemTime(new Date('2026-02-26T12:00:00.000Z'))

    store.state.lastCheckTime = Date.now() - 61000
    store.state.isPosture = true
    store.state.monitoringEnabled = true

    mockedQueryDeviceProperty.mockResolvedValueOnce([
      { identifier: 'isPosture', value: true, time: Date.now() },
    ])
    mockedQueryDeviceStatus.mockResolvedValueOnce(true)

    await store.fetchLatest()

    expect(store.state.todayTotalMinutes).toBe(1)
    expect(store.state.todayGoodMinutes).toBe(1)
  })

  it('maps numeric currentMode from device property', async () => {
    mockedQueryDeviceProperty.mockResolvedValueOnce([
      { identifier: 'isPosture', value: true, time: Date.now() },
      { identifier: 'currentMode', value: 2, time: Date.now() },
    ])
    mockedQueryDeviceStatus.mockResolvedValueOnce(true)

    await store.fetchLatest()

    expect(store.state.currentMode).toBe('timer')
  })
})
