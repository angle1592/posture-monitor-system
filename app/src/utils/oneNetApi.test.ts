import { beforeEach, describe, expect, it, vi } from 'vitest'

type UniRequestOptions = {
  url: string
  method?: string
  data?: Record<string, unknown>
  success?: (res: { statusCode: number; data?: unknown }) => void
  fail?: (err: { errMsg?: string }) => void
}

describe('oneNetApi.setDeviceProperty', () => {
  beforeEach(() => {
    vi.resetModules()
  })

  it('keeps unknown identifiers until server rejects them', async () => {
    const requestSpy = vi.fn((options: UniRequestOptions) => {
      if (options.url.includes('set-device-property')) {
        options.success?.({ statusCode: 200, data: { code: 0, data: null } })
      }
    })

    ;(globalThis as unknown as { uni: { request: typeof requestSpy; setStorageSync: ReturnType<typeof vi.fn>; getStorageSync: ReturnType<typeof vi.fn> } }).uni = {
      request: requestSpy,
      setStorageSync: vi.fn(),
      getStorageSync: vi.fn(),
    }

    const { setDeviceProperty } = await import('./oneNetApi')

    const ok = await setDeviceProperty({
      alertModeMask: 7,
      cooldownMs: 15000,
    })

    expect(ok).toBe(true)

    const setCall = requestSpy.mock.calls.find(
      ([options]) => (options as UniRequestOptions).url.includes('set-device-property')
    )

    expect(setCall).toBeTruthy()
    const payload = (setCall?.[0] as UniRequestOptions).data as
      | { params?: Record<string, unknown> }
      | undefined
    expect(payload?.params).toEqual({ alertModeMask: 7, cooldownMs: 15000 })
  })

  it('retries after 10411 and drops invalid identifier', async () => {
    let setCallCount = 0
    const requestSpy = vi.fn((options: UniRequestOptions) => {
      if (options.url.includes('set-device-property')) {
        setCallCount += 1
        if (setCallCount === 1) {
          options.success?.({
            statusCode: 200,
            data: {
              code: 10411,
              msg: '属性设置失败:identifier: alertModeMask, error: identifier not exist',
            },
          })
          return
        }

        options.success?.({ statusCode: 200, data: { code: 0, data: null } })
      }
    })

    ;(globalThis as unknown as { uni: { request: typeof requestSpy; setStorageSync: ReturnType<typeof vi.fn>; getStorageSync: ReturnType<typeof vi.fn> } }).uni = {
      request: requestSpy,
      setStorageSync: vi.fn(),
      getStorageSync: vi.fn(),
    }

    const { setDeviceProperty } = await import('./oneNetApi')
    const ok = await setDeviceProperty({ alertModeMask: 7, currentMode: 1 })

    expect(ok).toBe(true)
    const setCalls = requestSpy.mock.calls.filter(
      ([options]) => (options as UniRequestOptions).url.includes('set-device-property')
    )
    expect(setCalls.length).toBe(2)

    const secondPayload = (setCalls[1][0] as UniRequestOptions).data as
      | { params?: Record<string, unknown> }
      | undefined
    expect(secondPayload?.params).toEqual({ currentMode: 1 })
  })
})
