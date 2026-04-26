/// <reference types="vite/client" />

interface ImportMetaEnv {
  readonly VITE_ONENET_PRODUCT_ID: string
  readonly VITE_ONENET_DEVICE_NAME: string
  readonly VITE_ONENET_TOKEN: string
  readonly VITE_USE_MOCK_HISTORY?: string
  readonly VITE_MQTT_ENABLED?: string
  readonly VITE_MQTT_HOST?: string
  readonly VITE_MQTT_PORT?: string
  readonly VITE_MQTT_USE_SSL?: string
  readonly VITE_MQTT_CLIENT_ID?: string
  readonly VITE_MQTT_USERNAME?: string
  readonly VITE_MQTT_PASSWORD?: string
  readonly VITE_MQTT_TOPIC_UP?: string
  readonly VITE_MQTT_TOPIC_DOWN?: string
}

interface ImportMeta {
  readonly env: ImportMetaEnv
}

declare module '*.vue' {
  import { DefineComponent } from 'vue'
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const component: DefineComponent<{}, {}, any>
  export default component
}
