# App HTTP + MQTT Hybrid Architecture

## Communication Split

- HTTP (`src/utils/oneNetApi.ts`):
  - Query current device properties
  - Query device history
  - Query device online status
  - Send control writes through `setDeviceProperty()`
- MQTT (`src/utils/realtime/*`):
  - Receive realtime property pushes over WebSocket on H5/App Plus
  - Expose connection-state callbacks to `src/utils/store.ts`
  - Fall back to HTTP polling automatically when MQTT is unavailable

## Platform Policy

- H5 / App Plus: MQTT is optional and controlled by `VITE_MQTT_ENABLED`
- MP-WEIXIN: current implementation stays on HTTP polling only
- If MQTT connection fails, UI behavior must continue to work through existing HTTP polling

## Runtime Configuration

MQTT runtime values come from `.env` and are intentionally not hardcoded in the repo:

- `VITE_MQTT_ENABLED`
- `VITE_MQTT_HOST`
- `VITE_MQTT_PORT`
- `VITE_MQTT_USE_SSL`
- `VITE_MQTT_CLIENT_ID`
- `VITE_MQTT_USERNAME`
- `VITE_MQTT_PASSWORD`
- `VITE_MQTT_TOPIC_UP`
- `VITE_MQTT_TOPIC_DOWN`

Before production rollout, confirm the real broker/topic values from the OneNET console and fill them into deployment-specific `.env` files.

## State Flow

1. `store.init()` restores HTTP token and initializes realtime transport config.
2. `App.vue` foreground lifecycle calls `store.setAppVisibility(true)`, which attempts MQTT connect when enabled.
3. MQTT messages are parsed by `src/utils/realtime/message-adapter.ts` and converted into property patches.
4. `store.applyRealtimePatch()` writes those patches into the same state shape used by HTTP polling.
5. If MQTT is stale or unavailable, `store.confirmDeviceSync()` and periodic polling use HTTP to keep state correct.

## Operations Checklist

- Broker unreachable: inspect `[Realtime] MQTT connect failed` or `[Realtime] MQTT error` logs
- Topic misconfigured: page stays on HTTP fallback and never updates `lastRealtimeMessageTime`
- Connected but no device data: verify device-side publish topic and whether OneNET property push is enabled
- Need to force HTTP-only mode: set `VITE_MQTT_ENABLED=false`

## Legacy File

- `src/utils/mqtt.js` is kept only as a legacy reference.
- New code should import `src/utils/realtime/mqtt-client.ts` instead.
