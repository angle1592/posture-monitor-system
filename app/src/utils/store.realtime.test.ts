import { beforeEach, describe, expect, it, vi } from 'vitest'

vi.mock('./oneNetApi', () => ({
  queryDeviceProperty: vi.fn(),
  queryDeviceStatus: vi.fn(),
  restoreToken: vi.fn(),
}))

vi.mock('./realtime/mqtt-client', () => ({
  connectRealtime: vi.fn(),
  disconnectRealtime: vi.fn(),
  getRealtimeState: vi.fn(() => 'idle'),
  initRealtimeClient: vi.fn(),
  onRealtimeMessage: vi.fn(),
  onRealtimeStateChange: vi.fn(),
  subscribeRealtime: vi.fn(),
}))

import store from './store'
import { queryDeviceProperty, queryDeviceStatus } from './oneNetApi'

const mockedQueryDeviceProperty = vi.mocked(queryDeviceProperty)
const mockedQueryDeviceStatus = vi.mocked(queryDeviceStatus)

describe('store realtime integration', () => {
  beforeEach(() => {
    mockedQueryDeviceProperty.mockReset()
    mockedQueryDeviceStatus.mockReset()

    ;(globalThis as unknown as { uni: { setStorageSync: ReturnType<typeof vi.fn>; getStorageSync: ReturnType<typeof vi.fn> } }).uni = {
      setStorageSync: vi.fn(),
      getStorageSync: vi.fn(),
    }

    store.state.postureType = 'normal'
    store.state.personPresent = false
    store.state.fillLightOn = false
    store.state.currentMode = 'posture'
    store.state.isOnline = false
    store.state.lastCheckTime = 0
    store.state.lastError = null
    store.state.realtimeState = 'idle'
    store.state.realtimeAvailable = false
    store.state.lastRealtimeMessageTime = 0
  })

  it('applies realtime property patches through the same state shape used by polling', () => {
    store.applyRealtimePatch({
      postureType: 'head_down',
      personPresent: true,
      fillLightOn: true,
      currentMode: 2,
      _meta: {
        source: 'mqtt',
        timestamp: '2026-04-11T12:00:00.000Z',
      },
    })

    expect(store.state.postureType).toBe('head_down')
    expect(store.state.personPresent).toBe(true)
    expect(store.state.fillLightOn).toBe(true)
    expect(store.state.currentMode).toBe('timer')
    expect(store.state.realtimeState).toBe('connected')
    expect(store.state.realtimeAvailable).toBe(true)
    expect(store.state.lastRealtimeMessageTime).toBe(new Date('2026-04-11T12:00:00.000Z').getTime())
  })

  it('falls back to HTTP refresh after write when realtime is unavailable', async () => {
    mockedQueryDeviceProperty.mockResolvedValueOnce([
      { identifier: 'currentMode', value: 1, time: Date.now() },
    ])
    mockedQueryDeviceStatus.mockResolvedValueOnce(true)

    const strategy = await store.confirmDeviceSync()

    expect(strategy).toBe('http')
    expect(store.state.currentMode).toBe('clock')
    expect(store.state.isOnline).toBe(true)
  })

  it('skips HTTP refresh after write when realtime is already available', async () => {
    store.state.realtimeAvailable = true
    store.state.realtimeState = 'connected'
    store.state.lastRealtimeMessageTime = Date.now()

    const strategy = await store.confirmDeviceSync()

    expect(strategy).toBe('mqtt')
    expect(mockedQueryDeviceProperty).not.toHaveBeenCalled()
    expect(mockedQueryDeviceStatus).not.toHaveBeenCalled()
  })
})
