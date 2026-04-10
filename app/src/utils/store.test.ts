import { beforeEach, describe, expect, it, vi } from 'vitest'

vi.mock('./oneNetApi', () => ({
  queryDeviceProperty: vi.fn(),
  queryDeviceStatus: vi.fn(),
  restoreToken: vi.fn(),
}))

import store from './store'
import { queryDeviceProperty, queryDeviceStatus } from './oneNetApi'
import { PROP_IDS } from './constants'

const mockedQueryDeviceProperty = vi.mocked(queryDeviceProperty)
const mockedQueryDeviceStatus = vi.mocked(queryDeviceStatus)

function resetStoreState() {
  store.state.isOnline = false
  store.state.lastCheckTime = 0
  store.state.isPosture = true
  store.state.postureType = 'normal'
  store.state.personPresent = false
  store.state.fillLightOn = false
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
      { identifier: PROP_IDS.POSTURE_TYPE, value: 'normal', time: Date.now() },
    ])
    mockedQueryDeviceStatus.mockResolvedValueOnce(false)

    await store.fetchLatest()

    expect(store.state.isOnline).toBe(true)
  })

  it('accumulates usage minutes while monitoring is enabled', async () => {
    vi.useFakeTimers()
    vi.setSystemTime(new Date('2026-02-26T12:00:00.000Z'))

    store.state.lastCheckTime = Date.now() - 61000
    store.state.postureType = 'normal'
    store.state.isPosture = true
    store.state.monitoringEnabled = true

    mockedQueryDeviceProperty.mockResolvedValueOnce([
      { identifier: PROP_IDS.POSTURE_TYPE, value: 'normal', time: Date.now() },
    ])
    mockedQueryDeviceStatus.mockResolvedValueOnce(true)

    await store.fetchLatest()

    expect(store.state.todayTotalMinutes).toBe(1)
    expect(store.state.todayGoodMinutes).toBe(1)
  })

  it('maps numeric currentMode from device property', async () => {
    mockedQueryDeviceProperty.mockResolvedValueOnce([
      { identifier: PROP_IDS.POSTURE_TYPE, value: 'normal', time: Date.now() },
      { identifier: PROP_IDS.CURRENT_MODE, value: 2, time: Date.now() },
    ])
    mockedQueryDeviceStatus.mockResolvedValueOnce(true)

    await store.fetchLatest()

    expect(store.state.currentMode).toBe('timer')
  })

  it('derives posture state from postureType/personPresent/fillLightOn properties', async () => {
    mockedQueryDeviceProperty.mockResolvedValueOnce([
      { identifier: PROP_IDS.POSTURE_TYPE, value: 'head_down', time: Date.now() },
      { identifier: PROP_IDS.PERSON_PRESENT, value: true, time: Date.now() },
      { identifier: PROP_IDS.FILL_LIGHT_ON, value: false, time: Date.now() },
    ])
    mockedQueryDeviceStatus.mockResolvedValueOnce(true)

    await store.fetchLatest()

    expect(store.state.postureType).toBe('head_down')
    expect(store.state.personPresent).toBe(true)
    expect(store.state.fillLightOn).toBe(false)
    expect(store.postureText).toBe('低头')
    expect(store.state.isPosture).toBe(false)
  })

  it('treats numeric posture code 0 as no_person instead of unknown or abnormal posture', async () => {
    mockedQueryDeviceProperty.mockResolvedValueOnce([
      { identifier: PROP_IDS.POSTURE_TYPE, value: 0, time: Date.now() },
      { identifier: PROP_IDS.PERSON_PRESENT, value: false, time: Date.now() },
    ])
    mockedQueryDeviceStatus.mockResolvedValueOnce(true)

    await store.fetchLatest()

    expect(store.state.postureType).toBe('no_person')
    expect(store.postureText).toBe('无人')
    expect(store.state.isPosture).toBe(false)
  })
})
