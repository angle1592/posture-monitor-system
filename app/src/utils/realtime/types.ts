export type RealtimeConnectionState = 'idle' | 'connecting' | 'connected' | 'degraded' | 'unsupported'

export interface RealtimeTransportConfig {
  enabled: boolean
  host: string
  port: number
  useSSL: boolean
  clientId: string
  username: string
  password: string
  publishTopic: string
  subscribeTopic: string
}

export interface RealtimeMessageEnvelope {
  payload: Record<string, unknown>
  timestamp: string | number | null
}

export interface RealtimePropertyPatch {
  postureType?: unknown
  monitoringEnabled?: unknown
  currentMode?: unknown
  personPresent?: unknown
  ambientLux?: unknown
  fillLightOn?: unknown
  timerRunning?: unknown
  timerDurationSec?: unknown
  alertModeMask?: unknown
  cooldownMs?: unknown
  cfgVersion?: unknown
  selfTest?: unknown
  isPosture?: unknown
  _meta: {
    source: 'mqtt'
    timestamp: string | number | null
  }
}
