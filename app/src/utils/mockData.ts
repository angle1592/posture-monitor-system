import { POSTURE_TYPES, PROP_IDS, SYSTEM_MODES } from './constants'
import type { HistoryDataPoint, PropertyItem } from './oneNetApi'
import type { PostureValue } from './historyChart'

const MOCK_PATTERN = [
  POSTURE_TYPES.NORMAL,
  POSTURE_TYPES.HEAD_DOWN,
  POSTURE_TYPES.NORMAL,
  POSTURE_TYPES.HUNCHBACK,
  POSTURE_TYPES.NO_PERSON,
  POSTURE_TYPES.NORMAL,
] as const

function patternAt(index: number) {
  return MOCK_PATTERN[index % MOCK_PATTERN.length]
}

function buildProperty(identifier: string, value: string | number | boolean, time: number): PropertyItem {
  return {
    identifier,
    value,
    time,
  }
}

export function getMockDeviceStatus(): boolean {
  return true
}

export function getMockRealtimeProperties(now: number = Date.now()): PropertyItem[] {
  const slot = Math.floor(now / 5000)
  const postureType = patternAt(slot)
  const personPresent = postureType !== POSTURE_TYPES.NO_PERSON
  const fillLightOn = personPresent && slot % 2 === 0

  return [
    buildProperty(PROP_IDS.POSTURE_TYPE, postureType, now),
    buildProperty(PROP_IDS.PERSON_PRESENT, personPresent, now),
    buildProperty(PROP_IDS.FILL_LIGHT_ON, fillLightOn, now),
    buildProperty(PROP_IDS.MONITORING_ENABLED, true, now),
    buildProperty(PROP_IDS.CURRENT_MODE, SYSTEM_MODES.POSTURE, now),
  ]
}

export function getMockPropertyHistory(identifier: string, days: number, now: number = Date.now()): HistoryDataPoint[] {
  if (identifier !== PROP_IDS.POSTURE_TYPE) {
    return []
  }

  const points: HistoryDataPoint[] = []
  const dayMs = 24 * 60 * 60 * 1000
  const stepMs = 5 * 60 * 1000
  const start = now - days * dayMs

  for (let timestamp = start; timestamp <= now; timestamp += stepMs) {
    const date = new Date(timestamp)
    const hour = date.getHours()
    const minuteBucket = Math.floor(date.getMinutes() / 5)
    let posture: PostureValue = POSTURE_TYPES.NORMAL

    if (hour < 7 || hour >= 23) {
      posture = POSTURE_TYPES.NO_PERSON
    } else if (hour >= 12 && hour < 14) {
      posture = minuteBucket % 2 === 0 ? POSTURE_TYPES.NO_PERSON : POSTURE_TYPES.NORMAL
    } else if (hour >= 9 && hour < 11) {
      posture = minuteBucket % 4 === 0 ? POSTURE_TYPES.HEAD_DOWN : POSTURE_TYPES.NORMAL
    } else if (hour >= 15 && hour < 18) {
      posture = minuteBucket % 5 === 0 ? POSTURE_TYPES.HUNCHBACK : POSTURE_TYPES.NORMAL
    } else if (hour >= 20 && hour < 22) {
      posture = minuteBucket % 3 === 0 ? POSTURE_TYPES.HEAD_DOWN : POSTURE_TYPES.NORMAL
    }

    points.push({
      time: timestamp,
      value: posture,
    })
  }

  return points
}
