import { describe, expect, it } from 'vitest'

import { parseRealtimePayload, toPropertyPatch } from './message-adapter'

describe('message-adapter', () => {
  it('parses OneNET-style params payload into a flat property patch', () => {
    const raw = JSON.stringify({
      id: 'evt-1',
      params: {
        postureType: { value: 'head_down', time: '2026-04-11T12:00:00.000Z' },
        personPresent: { value: true },
        fillLightOn: { value: false },
      },
    })

    const envelope = parseRealtimePayload(raw)
    const patch = envelope && toPropertyPatch(envelope)

    expect(patch).toEqual({
      postureType: 'head_down',
      personPresent: true,
      fillLightOn: false,
      _meta: {
        source: 'mqtt',
        timestamp: '2026-04-11T12:00:00.000Z',
      },
    })
  })

  it('accepts a plain property object payload without nested value wrappers', () => {
    const raw = JSON.stringify({
      postureType: 'normal',
      monitoringEnabled: true,
      currentMode: 2,
    })

    const envelope = parseRealtimePayload(raw)
    const patch = envelope && toPropertyPatch(envelope)

    expect(patch).toEqual({
      postureType: 'normal',
      monitoringEnabled: true,
      currentMode: 2,
      _meta: {
        source: 'mqtt',
        timestamp: null,
      },
    })
  })

  it('returns null for invalid JSON payloads', () => {
    expect(parseRealtimePayload('{not-json}')).toBeNull()
  })
})
