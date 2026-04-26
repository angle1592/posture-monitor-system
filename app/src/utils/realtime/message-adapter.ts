import { PROP_IDS } from '../constants'
import type { RealtimeMessageEnvelope, RealtimePropertyPatch } from './types'

type WrappedValue = {
  value?: unknown
  time?: string | number
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value)
}

function getEnvelopePayload(parsed: Record<string, unknown>): Record<string, unknown> {
  const candidate = parsed.params
  return isRecord(candidate) ? candidate : parsed
}

function unwrapValue(entry: unknown): unknown {
  if (!isRecord(entry) || !('value' in entry)) {
    return entry
  }

  return (entry as WrappedValue).value
}

function getTimestampFromPayload(payload: Record<string, unknown>): string | number | null {
  for (const value of Object.values(payload)) {
    if (isRecord(value) && 'time' in value) {
      return (value as WrappedValue).time ?? null
    }
  }

  return null
}

export function parseRealtimePayload(raw: string): RealtimeMessageEnvelope | null {
  try {
    const parsed = JSON.parse(raw) as unknown
    if (!isRecord(parsed)) {
      return null
    }

    const payload = getEnvelopePayload(parsed)
    return {
      payload,
      timestamp: getTimestampFromPayload(payload),
    }
  } catch (error) {
    console.warn('[Realtime] 无法解析 payload:', error)
    return null
  }
}

export function toPropertyPatch(message: RealtimeMessageEnvelope): RealtimePropertyPatch | null {
  const payload = message.payload
  const patch: RealtimePropertyPatch = {
    _meta: {
      source: 'mqtt',
      timestamp: message.timestamp,
    },
  }

  const supportedKeys = Object.values(PROP_IDS)
  for (const key of supportedKeys) {
    if (!(key in payload)) continue
    patch[key] = unwrapValue(payload[key])
  }

  return Object.keys(patch).length > 1 ? patch : null
}
