import type { IClientOptions, MqttClient } from 'mqtt'

import type {
  RealtimeConnectionState,
  RealtimeTransportConfig,
} from './types'

type RealtimeMessageHandler = (topic: string, payload: string) => void
type RealtimeStateHandler = (state: RealtimeConnectionState) => void

let client: MqttClient | null = null
let config: RealtimeTransportConfig | null = null
let realtimeState: RealtimeConnectionState = 'idle'
let activeSubscription: string | null = null

const messageHandlers = new Set<RealtimeMessageHandler>()
const stateHandlers = new Set<RealtimeStateHandler>()

function isSupportedPlatform(): boolean {
  return typeof window !== 'undefined'
}

function hasUsableConfig(candidate: RealtimeTransportConfig | null): candidate is RealtimeTransportConfig {
  return !!candidate && candidate.enabled && !!candidate.host && !!candidate.subscribeTopic
}

function emitState(nextState: RealtimeConnectionState) {
  realtimeState = nextState
  stateHandlers.forEach((handler) => handler(nextState))
}

function emitMessage(topic: string, payload: string) {
  messageHandlers.forEach((handler) => handler(topic, payload))
}

function getBrokerUrl(currentConfig: RealtimeTransportConfig): string {
  const protocol = currentConfig.useSSL ? 'wss' : 'ws'
  return `${protocol}://${currentConfig.host}:${currentConfig.port}/mqtt`
}

async function loadMqttModule() {
  return import('mqtt')
}

export function initRealtimeClient(nextConfig: RealtimeTransportConfig) {
  config = nextConfig

  if (!nextConfig.enabled) {
    emitState('unsupported')
    return
  }

  emitState('idle')
}

export function getRealtimeState(): RealtimeConnectionState {
  return realtimeState
}

export function onRealtimeStateChange(handler: RealtimeStateHandler): () => void {
  stateHandlers.add(handler)
  return () => stateHandlers.delete(handler)
}

export function onRealtimeMessage(handler: RealtimeMessageHandler): () => void {
  messageHandlers.add(handler)
  return () => messageHandlers.delete(handler)
}

export async function subscribeRealtime(topic: string): Promise<void> {
  if (!client) {
    throw new Error('MQTT 未连接')
  }

  if (activeSubscription === topic) {
    return
  }

  if (activeSubscription) {
    await new Promise<void>((resolve, reject) => {
      client?.unsubscribe(activeSubscription as string, (error) => {
        if (error) {
          reject(error)
          return
        }
        resolve()
      })
    })
  }

  await new Promise<void>((resolve, reject) => {
    client?.subscribe(topic, (error) => {
      if (error) {
        reject(error)
        return
      }
      resolve()
    })
  })

  activeSubscription = topic
}

export async function connectRealtime(): Promise<RealtimeConnectionState> {
  if (!isSupportedPlatform()) {
    emitState('unsupported')
    return realtimeState
  }

  if (!hasUsableConfig(config)) {
    emitState('unsupported')
    return realtimeState
  }

  if (client && realtimeState === 'connected') {
    return realtimeState
  }

  emitState('connecting')

  try {
    const mqtt = await loadMqttModule()
    const options: IClientOptions = {
      clientId: config.clientId || undefined,
      username: config.username || undefined,
      password: config.password || undefined,
      reconnectPeriod: 5000,
      connectTimeout: 30000,
    }

    client = mqtt.connect(getBrokerUrl(config), options)

    client.on('message', (topic, payload) => {
      emitMessage(topic, payload.toString())
    })

    client.on('close', () => {
      emitState('degraded')
    })

    client.on('error', (error) => {
      console.error('[Realtime] MQTT error:', error)
      emitState('degraded')
    })

    await new Promise<void>((resolve, reject) => {
      client?.once('connect', async () => {
        try {
          await subscribeRealtime(config!.subscribeTopic)
          emitState('connected')
          resolve()
        } catch (error) {
          reject(error)
        }
      })
      client?.once('error', reject)
    })
  } catch (error) {
    console.error('[Realtime] MQTT connect failed:', error)
    emitState('degraded')
  }

  return realtimeState
}

export async function disconnectRealtime(): Promise<void> {
  if (!client) {
    emitState(config?.enabled ? 'idle' : 'unsupported')
    return
  }

  const currentClient = client
  client = null
  activeSubscription = null

  await new Promise<void>((resolve) => {
    currentClient.end(false, {}, () => resolve())
  })

  emitState(config?.enabled ? 'idle' : 'unsupported')
}
