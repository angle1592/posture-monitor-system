<template>
  <PageShell tone="cyber">
    <view class="container">
      <!-- 页面标题 -->
      <view class="page-header">
        <SectionHeader title="设备控制" subtitle="DEVICE CONTROL">
          <template #right>
            <StatusBadge
              :online="store.state.isOnline"
              online-text="已连接"
              offline-text="未连接"
            />
          </template>
        </SectionHeader>
      </view>

      <!-- 设备信息卡 -->
      <view class="device-card">
        <view class="device-header">
          <view class="device-icon">
            <text>🖥</text>
          </view>
          
          <view class="device-info">
            <text class="device-name">智能监测仪</text>
            <text class="device-id">ESP32-S3-N16R8</text>
          </view>
          
          <view class="device-status" v-if="store.state.isOnline">
            <view class="status-indicator"></view>
            <text class="status-text">运行中</text>
          </view>
        </view>
        
        <view class="device-stats">
          <view class="stat-item">
            <view class="stat-icon">⚙️</view>
            <view class="stat-content">
              <text class="stat-label">运行状态</text>
              <text 
                class="stat-value" 
                :class="store.state.isOnline ? (store.state.monitoringEnabled ? 'running' : 'stopped') : 'offline'"
              >
                {{ store.state.isOnline ? (store.state.monitoringEnabled ? '运行中' : '已停止') : '离线' }}
              </text>
            </view>
          </view>
          
          <view class="stat-divider"></view>
          
          <view class="stat-item">
            <view class="stat-icon">⏱️</view>
            <view class="stat-content">
              <text class="stat-label">今日使用</text>
              <text class="stat-value normal">{{ store.usageTimeText }}</text>
            </view>
          </view>
        </view>
      </view>

      <!-- 工作模式 -->
      <view class="control-section">
        <view class="section-label">
          <text class="label-icon">◈</text>
          <text class="label-text">工作模式</text>
        </view>
        
        <view class="mode-selector">
          <view
            v-for="mode in modeOptions"
            :key="mode.value"
            class="mode-option"
            :class="{ active: selectedMode === mode.value, disabled: !store.state.isOnline }"
            @click="switchMode(mode.value)"
          >
            <view class="option-glow"></view>
            
            <view class="option-content">
              <text class="option-name">{{ mode.label }}</text>
              <text class="option-desc">{{ mode.desc }}</text>
            </view>
            
            <view class="option-indicator" v-if="selectedMode === mode.value">
              <text>◉</text>
            </view>
          </view>
        </view>
      </view>

      <!-- 提醒设置 -->
      <view class="control-section">
        <view class="section-label">
          <text class="label-icon">◈</text>
          <text class="label-text">提醒方式</text>
        </view>
        
        <view class="reminder-grid">
          <view
            v-for="reminder in reminderOptions"
            :key="reminder.value"
            class="reminder-card"
            :class="{ active: selectedReminders.includes(reminder.value), disabled: !store.state.isOnline }"
            @click="toggleReminder(reminder.value)"
          >
            <view class="card-glow"></view>
            
            <view class="card-icon-wrap">
              <text class="card-icon">{{ reminder.icon }}</text>
              
              <view class="check-badge" v-if="selectedReminders.includes(reminder.value)">
                <text>✓</text>
              </view>
            </view>
            
            <text class="card-name">{{ reminder.label }}</text>
          </view>
        </view>
      </view>

      <!-- 硬件自检 -->
      <view class="control-section">
        <view class="section-label">
          <text class="label-icon">◈</text>
          <text class="label-text">硬件自检</text>
        </view>
        
        <button 
          class="selftest-btn" 
          :disabled="selfTestLoading || !store.state.isOnline" 
          @click="runHardwareSelfTest"
        >
          <text class="btn-icon">◉</text>
          <text class="btn-text">{{ selfTestLoading ? '自检中...' : '开始自检' }}</text>
        </button>
      </view>

      <!-- 定时功能 -->
      <view class="control-section">
        <view class="section-label">
          <text class="label-icon">◈</text>
          <text class="label-text">定时器</text>
        </view>
        
        <view class="timer-card">
          <view class="timer-header">
            <view class="timer-title-wrap">
              <text class="timer-icon">⏳</text>
              <view class="timer-title-content">
                <text class="timer-title">倒计时提醒</text>
                <text class="timer-status">{{ timerStatusText }}</text>
              </view>
            </view>
            
            <switch
              :checked="timerEnabled"
              @change="onTimerSwitch"
              :disabled="!store.state.isOnline"
              color="#00f0ff"
              style="transform: scale(0.9);"
            />
          </view>
          
          <view class="timer-body" :class="{ disabled: !store.state.isOnline }">
            <view class="timer-row">
              <text class="row-label">定时时长</text>
              
              <view class="duration-editor">
                <view class="duration-slider-row">
                  <text class="duration-unit">时</text>
                  <slider
                    class="duration-slider"
                    :value="durationHour"
                    :min="0"
                    :max="2"
                    :step="1"
                    :disabled="!store.state.isOnline"
                    activeColor="#00f0ff"
                    backgroundColor="rgba(148, 163, 184, 0.22)"
                    block-color="#00f0ff"
                    :block-size="18"
                    @changing="onDurationSliderChanging('hour', $event)"
                    @change="onDurationSliderChange('hour', $event)"
                  />
                  <text class="duration-value">{{ durationHour }}时</text>
                </view>
                <view class="duration-slider-row">
                  <text class="duration-unit">分</text>
                  <slider
                    class="duration-slider"
                    :value="durationMinute"
                    :min="0"
                    :max="59"
                    :step="1"
                    :disabled="!store.state.isOnline"
                    activeColor="#00f0ff"
                    backgroundColor="rgba(148, 163, 184, 0.22)"
                    block-color="#00f0ff"
                    :block-size="18"
                    @changing="onDurationSliderChanging('minute', $event)"
                    @change="onDurationSliderChange('minute', $event)"
                  />
                  <text class="duration-value">{{ durationMinute }}分</text>
                </view>
                <view class="duration-slider-row">
                  <text class="duration-unit">秒</text>
                  <slider
                    class="duration-slider"
                    :value="durationSecond"
                    :min="0"
                    :max="59"
                    :step="1"
                    :disabled="!store.state.isOnline"
                    activeColor="#00f0ff"
                    backgroundColor="rgba(148, 163, 184, 0.22)"
                    block-color="#00f0ff"
                    :block-size="18"
                    @changing="onDurationSliderChanging('second', $event)"
                    @change="onDurationSliderChange('second', $event)"
                  />
                  <text class="duration-value">{{ durationSecond }}秒</text>
                </view>
              </view>
            </view>
          </view>
        </view>
      </view>

      <view class="safe-area-bottom"></view>
    </view>
  </PageShell>
</template>

<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { onHide, onShow } from '@dcloudio/uni-app'
import store from '@/utils/store'
import PageShell from '@/components/ui/PageShell.vue'
import SectionHeader from '@/components/ui/SectionHeader.vue'
import StatusBadge from '@/components/ui/StatusBadge.vue'
import { setDeviceProperty } from '@/utils/oneNetApi'

/*
 * 页面职责：设备控制页。
 * - 下发模式切换、提醒方式、定时参数与硬件自检指令。
 * - 采用“先本地回显，再远端提交，失败回滚”的交互策略。
 * - 页面激活时提升轮询频率，保证控制反馈及时。
 */

interface ControlPageSettings {
  selectedMode?: string
  selectedReminders?: string[]
  timerEnabled?: boolean
  customDurationSec?: number
}

const reminderMaskMap: Record<string, number> = {
  light: 1,
  sound: 2,
  voice: 4,
}

const voiceCompatIdentifiers = [
  'voiceEnabled',
  'speechEnabled',
  'ttsEnabled',
  'voiceSwitch',
] as const

const TIMER_MIN_SEC = 1
const TIMER_MAX_SEC = 7200
const TIMER_DEFAULT_SEC = 15 * 60
const modeValueMap: Record<string, number> = {
  posture: 0,
  clock: 1,
  timer: 2,
}

function pickEventValue<T>(event: unknown): T | undefined {
  const payload = event as { detail?: { value?: T } } | undefined
  return payload?.detail?.value
}

const reminderOptions = ref([
  { value: 'voice', label: '语音播报', icon: '🔊' },
  { value: 'light', label: '灯光闪烁', icon: '💡' },
  { value: 'sound', label: '蜂鸣警报', icon: '🔔' },
])
const modeOptions = [
  { value: 'posture', label: '坐姿检测', desc: '检测并提醒不良坐姿' },
  { value: 'clock', label: '时钟模式', desc: '显示时间与基础状态' },
  { value: 'timer', label: '定时器模式', desc: '专注计时与倒计时提醒' },
]
const selectedMode = ref('posture')
const selectedReminders = ref<string[]>(['voice', 'light'])

const timerEnabled = ref(false)
const customDurationSec = ref(TIMER_DEFAULT_SEC)
const lastCommittedDurationSec = ref(TIMER_DEFAULT_SEC)
const durationHour = ref(0)
const durationMinute = ref(15)
const durationSecond = ref(0)
const selfTestLoading = ref(false)
const SELF_TEST_SEQ_KEY = 'selfTestSeq'
const lastUserActionTime = ref(0)
const BOUNCE_GUARD_MS = 3000
const TIMER_MODE = 'timer'

const isTimerMode = computed(() => selectedMode.value === TIMER_MODE)
const timerStatusText = computed(() => {
  if (!store.state.isOnline) return '设备离线'
  if (timerEnabled.value) return '计时中'
  return isTimerMode.value ? '已暂停' : '未进入定时器模式'
})

function clampTimerDuration(value: number): number {
  if (!Number.isFinite(value)) return TIMER_MIN_SEC
  if (value < TIMER_MIN_SEC) return TIMER_MIN_SEC
  if (value > TIMER_MAX_SEC) return TIMER_MAX_SEC
  return Math.round(value)
}

function durationToParts(totalSec: number): [number, number, number] {
  const sec = clampTimerDuration(totalSec)
  const hours = Math.floor(sec / 3600)
  const minutes = Math.floor((sec % 3600) / 60)
  const seconds = sec % 60
  return [hours, minutes, seconds]
}

function syncDurationParts(totalSec: number) {
  const [hours, minutes, seconds] = durationToParts(totalSec)
  durationHour.value = hours
  durationMinute.value = minutes
  durationSecond.value = seconds
}

function durationPartsToSeconds(): number {
  return clampTimerDuration(
    durationHour.value * 3600 + durationMinute.value * 60 + durationSecond.value
  )
}

function parseDurationControlValue(value: unknown): number {
  const n = Number(value)
  if (!Number.isFinite(n) || n < 0) return 0
  return Math.floor(n)
}

function setDurationPart(part: 'hour' | 'minute' | 'second', value: number) {
  const next = parseDurationControlValue(value)
  if (part === 'hour') {
    durationHour.value = Math.min(2, next)
  } else if (part === 'minute') {
    durationMinute.value = Math.min(59, next)
  } else {
    durationSecond.value = Math.min(59, next)
  }
}

function getReminderMask(values: string[]): number {
  // 云端用位掩码存提醒开关，前端多选值在这里收敛成一个整数。
  return values.reduce((mask, value) => mask | (reminderMaskMap[value] || 0), 0)
}

async function syncVoiceCompat(enabled: boolean) {
  // alertModeMask 已覆盖语音控制，以下仅保留历史兼容探测，静默失败。
  for (const identifier of voiceCompatIdentifiers) {
    const ok = await setDeviceProperty({ [identifier]: enabled }, true)
    if (ok) {
      console.info(`[Control] 语音兼容字段命中: ${identifier}`)
      return
    }
  }
}

function loadSettings() {
  // 页面私有设置与全局 store 分离存储，避免互相覆盖。
  try {
    const saved = uni.getStorageSync('controlPageSettings') as ControlPageSettings | undefined
    if (saved) {
      selectedMode.value = store.state.currentMode || saved.selectedMode || 'posture'
      selectedReminders.value = saved.selectedReminders || ['voice', 'light']
      timerEnabled.value = selectedMode.value === TIMER_MODE && (saved.timerEnabled || false)
      customDurationSec.value = clampTimerDuration(saved.customDurationSec ?? TIMER_DEFAULT_SEC)
      lastCommittedDurationSec.value = customDurationSec.value
      syncDurationParts(customDurationSec.value)
    } else {
      selectedMode.value = store.state.currentMode || 'posture'
      timerEnabled.value = false
      customDurationSec.value = TIMER_DEFAULT_SEC
      lastCommittedDurationSec.value = TIMER_DEFAULT_SEC
      syncDurationParts(TIMER_DEFAULT_SEC)
    }
  } catch (e) {
    console.error('[Control] 加载设置失败:', e)
  }
}

function saveSettings() {
  uni.setStorageSync('controlPageSettings', {
    selectedMode: selectedMode.value,
    selectedReminders: selectedReminders.value,
    timerEnabled: timerEnabled.value,
    customDurationSec: customDurationSec.value,
  })
}

async function switchMode(mode: string) {
  if (!store.state.isOnline) {
    uni.showToast({ title: '设备未连接', icon: 'none' })
    return
  }
  if (selectedMode.value === mode) return

  lastUserActionTime.value = Date.now()
  const oldMode = selectedMode.value
  selectedMode.value = mode

  // 先本地切换再远端提交，失败时回滚，减少操作等待感。
  const success = await setDeviceProperty({ currentMode: modeValueMap[mode] })
  if (!success) {
    selectedMode.value = oldMode
    uni.showToast({ title: '模式切换失败', icon: 'none' })
    return
  }

  store.state.currentMode = mode
  if (mode !== TIMER_MODE) {
    timerEnabled.value = false
  }
  saveSettings()
  // 不立即拉取状态：设备刚切换还来不及上报，拉回来的旧值会覆盖本地更新
}

async function toggleReminder(value: string) {
  if (!store.state.isOnline) {
    uni.showToast({ title: '设备未连接', icon: 'none' })
    return
  }

  const old = [...selectedReminders.value]
  const index = selectedReminders.value.indexOf(value)
  if (index > -1) {
    selectedReminders.value.splice(index, 1)
  } else {
    selectedReminders.value.push(value)
  }

  // 将多选提醒项收敛为 alertModeMask 位掩码后下发到设备。
  const success = await setDeviceProperty({
    alertModeMask: getReminderMask(selectedReminders.value),
  })

  if (!success) {
    selectedReminders.value = old
    uni.showToast({ title: '设置失败，请重试', icon: 'none' })
    return
  }

  if (value === 'voice') {
    await syncVoiceCompat(selectedReminders.value.includes('voice'))
  }

  saveSettings()
}

async function onTimerSwitch(e: unknown) {
  const value = pickEventValue<boolean>(e)
  if (typeof value !== 'boolean') return
  if (!store.state.isOnline) {
    uni.showToast({ title: '设备未连接', icon: 'none' })
    return
  }

  const oldValue = timerEnabled.value
  const oldMode = selectedMode.value
  timerEnabled.value = value
  customDurationSec.value = durationPartsToSeconds()
  syncDurationParts(customDurationSec.value)

  const params: Record<string, number | boolean> = {
    timerRunning: timerEnabled.value,
    alertModeMask: getReminderMask(selectedReminders.value),
  }
  if (timerEnabled.value) {
    params.currentMode = modeValueMap.timer
    params.timerDurationSec = customDurationSec.value
  }

  const success = await setDeviceProperty(params)
  if (!success) {
    timerEnabled.value = oldValue
    selectedMode.value = oldMode
    customDurationSec.value = lastCommittedDurationSec.value
    syncDurationParts(lastCommittedDurationSec.value)
    uni.showToast({ title: '设置失败，请重试', icon: 'none' })
    return
  }

  lastCommittedDurationSec.value = customDurationSec.value
  if (timerEnabled.value) {
    selectedMode.value = TIMER_MODE
    store.state.currentMode = TIMER_MODE
  }

  saveSettings()
}

function onDurationSliderChanging(part: 'hour' | 'minute' | 'second', e: unknown) {
  setDurationPart(part, Number(pickEventValue<number>(e)))
}

function onDurationSliderChange(part: 'hour' | 'minute' | 'second', e: unknown) {
  setDurationPart(part, Number(pickEventValue<number>(e)))
  void commitDurationParts()
}

async function commitDurationParts() {
  if (!store.state.isOnline) {
    uni.showToast({ title: '设备未连接', icon: 'none' })
    syncDurationParts(customDurationSec.value)
    return
  }

  const next = durationPartsToSeconds()
  if (next === lastCommittedDurationSec.value) {
    customDurationSec.value = next
    syncDurationParts(next)
    return
  }

  const oldValue = lastCommittedDurationSec.value
  customDurationSec.value = next
  syncDurationParts(next)

  const success = await setDeviceProperty({
    timerDurationSec: next,
  })

  if (!success) {
    customDurationSec.value = oldValue
    syncDurationParts(oldValue)
    uni.showToast({ title: '设置失败，请重试', icon: 'none' })
    return
  }

  lastCommittedDurationSec.value = next
  saveSettings()
}

async function runHardwareSelfTest() {
  if (!store.state.isOnline || selfTestLoading.value) {
    return
  }

  selfTestLoading.value = true
  const current = Number(uni.getStorageSync(SELF_TEST_SEQ_KEY))
  // 用递增序号触发自检，避免重复值被云端去重而丢命令。
  const nextSeq = Number.isFinite(current) && current > 0 ? (current % 100000) + 1 : 1
  uni.setStorageSync(SELF_TEST_SEQ_KEY, nextSeq)

  const trigger = nextSeq
  const success = await setDeviceProperty({
    alertModeMask: getReminderMask(selectedReminders.value),
    selfTest: trigger,
  })
  selfTestLoading.value = false

  if (!success) {
    uni.showToast({ title: '自检触发失败', icon: 'none' })
    return
  }

  uni.showToast({ title: '已触发自检', icon: 'success' })
}

watch(
  () => store.state.currentMode,
  (mode) => {
    if (!mode || selectedMode.value === mode) return
    // 用户刚操作过 3 秒内，忽略轮询拉回来的旧值，防止弹回
    if (Date.now() - lastUserActionTime.value < BOUNCE_GUARD_MS) return
    selectedMode.value = mode
    if (mode !== TIMER_MODE) {
      timerEnabled.value = false
    }
    saveSettings()
  }
)

onShow(() => {
  store.setPollingProfile('realtime')
  loadSettings()
  void store.fetchLatest()
})

onHide(() => {
  store.setPollingProfile('normal', false)
})
</script>

<style lang="scss" scoped>
.container {
  padding: 40rpx 30rpx 60rpx;
  min-height: 100vh;
}

/* 页面头部 */
.page-header {
  margin-bottom: 40rpx;
  animation: slide-up var(--duration-slow) var(--ease-out) both;
}

/* 区块标签 */
.section-label {
  display: flex;
  align-items: center;
  gap: 12rpx;
  margin-bottom: 20rpx;
  
  .label-icon {
    font-size: 24rpx;
    color: var(--neon-cyan);
    text-shadow: 0 0 10rpx var(--neon-cyan);
  }
  
  .label-text {
    font-size: 28rpx;
    font-weight: 700;
    color: var(--text-secondary);
    text-transform: uppercase;
    letter-spacing: 2rpx;
  }
}

/* 设备卡片 */
.device-card {
  background: linear-gradient(135deg, var(--bg-tertiary) 0%, var(--bg-secondary) 100%);
  border-radius: var(--radius-xl);
  padding: 32rpx;
  margin-bottom: 40rpx;
  border: 1rpx solid var(--border-subtle);
  animation: slide-up var(--duration-slow) var(--ease-out) 100ms both;
  
  .device-header {
    display: flex;
    align-items: center;
    gap: 24rpx;
    margin-bottom: 32rpx;
    padding-bottom: 32rpx;
    border-bottom: 1rpx solid var(--border-subtle);
    
    .device-icon {
      width: 100rpx;
      height: 100rpx;
      border-radius: var(--radius-lg);
      background: var(--neon-cyan-dim);
      border: 2rpx solid rgba(0, 240, 255, 0.3);
      display: flex;
      align-items: center;
      justify-content: center;
      box-shadow: 0 0 30rpx rgba(0, 240, 255, 0.2);
      
      text {
        font-size: 48rpx;
      }
    }
    
    .device-info {
      flex: 1;
      
      .device-name {
        display: block;
        font-size: 34rpx;
        font-weight: 800;
        color: var(--text-primary);
        margin-bottom: 8rpx;
      }
      
      .device-id {
        display: block;
        font-size: 24rpx;
        color: var(--text-tertiary);
        font-family: var(--font-mono);
      }
    }
    
    .device-status {
      display: flex;
      align-items: center;
      gap: 10rpx;
      padding: 12rpx 20rpx;
      background: rgba(0, 230, 118, 0.15);
      border: 1rpx solid rgba(0, 230, 118, 0.3);
      border-radius: var(--radius-pill);
      
      .status-indicator {
        width: 10rpx;
        height: 10rpx;
        border-radius: 50%;
        background: var(--state-success);
        box-shadow: 0 0 10rpx var(--state-success);
        animation: pulse 2s ease-in-out infinite;
      }
      
      .status-text {
        font-size: 24rpx;
        color: var(--state-success);
        font-weight: 700;
      }
    }
  }
  
  .device-stats {
    display: flex;
    align-items: center;
    
    .stat-item {
      flex: 1;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 16rpx;
      
      .stat-icon {
        font-size: 40rpx;
        opacity: 0.8;
      }
      
      .stat-content {
        .stat-label {
          display: block;
          font-size: 24rpx;
          color: var(--text-tertiary);
          margin-bottom: 4rpx;
        }
        
        .stat-value {
          display: block;
          font-size: 30rpx;
          font-weight: 700;
          
          &.running {
            color: var(--state-success);
            text-shadow: 0 0 10rpx rgba(0, 230, 118, 0.4);
          }
          
          &.stopped {
            color: var(--state-warning);
          }
          
          &.offline {
            color: var(--state-offline);
          }
          
          &.normal {
            color: var(--text-primary);
          }
        }
      }
    }
    
    .stat-divider {
      width: 2rpx;
      height: 60rpx;
      background: var(--border-subtle);
    }
  }
}

/* 控制区块 */
.control-section {
  margin-bottom: 40rpx;
  animation: slide-up var(--duration-slow) var(--ease-out) both;
  
  &:nth-child(3) { animation-delay: 200ms; }
  &:nth-child(4) { animation-delay: 250ms; }
  &:nth-child(5) { animation-delay: 300ms; }
  &:nth-child(6) { animation-delay: 350ms; }
}

/* 模式选择器 */
.mode-selector {
  display: flex;
  flex-direction: column;
  gap: 16rpx;
  
  .mode-option {
    position: relative;
    display: flex;
    align-items: center;
    padding: 28rpx;
    background: linear-gradient(135deg, var(--bg-tertiary) 0%, var(--bg-secondary) 100%);
    border-radius: var(--radius-lg);
    border: 2rpx solid var(--border-subtle);
    transition: all var(--duration-base);
    overflow: hidden;
    
    .option-glow {
      position: absolute;
      inset: 0;
      background: linear-gradient(90deg, var(--neon-cyan-dim), transparent);
      opacity: 0;
      transition: opacity var(--duration-base);
    }
    
    .option-content {
      position: relative;
      flex: 1;
      z-index: 1;
      
      .option-name {
        display: block;
        font-size: 30rpx;
        font-weight: 700;
        color: var(--text-primary);
        margin-bottom: 8rpx;
      }
      
      .option-desc {
        display: block;
        font-size: 24rpx;
        color: var(--text-tertiary);
      }
    }
    
    .option-indicator {
      position: relative;
      z-index: 1;
      
      text {
        font-size: 36rpx;
        color: var(--neon-cyan);
        text-shadow: 0 0 20rpx var(--neon-cyan);
      }
    }
    
    &.active {
      border-color: var(--neon-cyan);
      box-shadow: var(--neon-cyan-glow);
      
      .option-glow {
        opacity: 1;
      }
      
      .option-name {
        color: var(--neon-cyan);
      }
    }
    
    &.disabled {
      opacity: 0.5;
      pointer-events: none;
    }
    
    &:active {
      transform: scale(0.98);
    }
  }
}

/* 提醒网格 */
.reminder-grid {
  display: flex;
  gap: 20rpx;
  
  .reminder-card {
    flex: 1;
    position: relative;
    display: flex;
    flex-direction: column;
    align-items: center;
    padding: 36rpx 20rpx;
    background: linear-gradient(135deg, var(--bg-tertiary) 0%, var(--bg-secondary) 100%);
    border-radius: var(--radius-lg);
    border: 2rpx solid var(--border-subtle);
    transition: all var(--duration-base);
    overflow: hidden;
    
    .card-glow {
      position: absolute;
      inset: 0;
      background: linear-gradient(135deg, var(--neon-purple-dim), transparent);
      opacity: 0;
      transition: opacity var(--duration-base);
    }
    
    .card-icon-wrap {
      position: relative;
      width: 88rpx;
      height: 88rpx;
      border-radius: 50%;
      background: var(--bg-elevated);
      border: 2rpx solid var(--border-subtle);
      display: flex;
      align-items: center;
      justify-content: center;
      margin-bottom: 20rpx;
      transition: all var(--duration-base);
      
      .card-icon {
        font-size: 40rpx;
      }
      
      .check-badge {
        position: absolute;
        right: -8rpx;
        bottom: -8rpx;
        width: 36rpx;
        height: 36rpx;
        border-radius: 50%;
        background: var(--neon-cyan);
        border: 4rpx solid var(--bg-primary);
        display: flex;
        align-items: center;
        justify-content: center;
        
        text {
          font-size: 20rpx;
          color: var(--bg-primary);
          font-weight: 900;
        }
      }
    }
    
    .card-name {
      position: relative;
      font-size: 26rpx;
      color: var(--text-tertiary);
      font-weight: 600;
      transition: all var(--duration-base);
    }
    
    &.active {
      border-color: var(--neon-purple);
      box-shadow: var(--neon-purple-glow);
      
      .card-glow {
        opacity: 1;
      }
      
      .card-icon-wrap {
        background: var(--neon-purple);
        border-color: var(--neon-purple);
        box-shadow: 0 0 30rpx rgba(168, 85, 247, 0.4);
        
        .card-icon {
          filter: brightness(0) invert(1);
        }
      }
      
      .card-name {
        color: var(--neon-purple);
      }
    }
    
    &.disabled {
      opacity: 0.5;
      pointer-events: none;
    }
    
    &:active {
      transform: scale(0.95);
    }
  }
}

/* 自检按钮 */
.selftest-btn {
  width: 100%;
  height: 96rpx;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 16rpx;
  background: linear-gradient(135deg, var(--neon-cyan) 0%, var(--neon-purple) 100%);
  border: none;
  border-radius: var(--radius-lg);
  box-shadow: 0 0 30rpx rgba(0, 240, 255, 0.3);
  transition: all var(--duration-base);
  
  .btn-icon {
    font-size: 32rpx;
  }
  
  .btn-text {
    font-size: 30rpx;
    color: var(--bg-primary);
    font-weight: 800;
  }
  
  &[disabled] {
    opacity: 0.4;
    box-shadow: none;
  }
  
  &:active:not([disabled]) {
    transform: scale(0.98);
    box-shadow: 0 0 50rpx rgba(0, 240, 255, 0.5);
  }
}

/* 定时卡片 */
.timer-card {
  background: linear-gradient(135deg, var(--bg-tertiary) 0%, var(--bg-secondary) 100%);
  border-radius: var(--radius-xl);
  padding: 32rpx;
  border: 1rpx solid var(--border-subtle);
  
  .timer-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 24rpx;
    
    .timer-title-wrap {
      display: flex;
      align-items: center;
      gap: 16rpx;
      
      .timer-icon {
        font-size: 36rpx;
      }

      .timer-title-content {
        display: flex;
        flex-direction: column;
        gap: 4rpx;
      }
      
      .timer-title {
        font-size: 30rpx;
        font-weight: 700;
        color: var(--text-primary);
      }

      .timer-status {
        font-size: 22rpx;
        color: var(--text-tertiary);
      }
    }
  }
  
  .timer-body {
    padding-top: 24rpx;
    border-top: 1rpx solid var(--border-subtle);
    transition: opacity var(--duration-base);
    
    &.disabled {
      opacity: 0.4;
      pointer-events: none;
    }
    
    .timer-row {
      display: flex;
      align-items: flex-start;
      justify-content: space-between;
      gap: 20rpx;
      
      .row-label {
        font-size: 28rpx;
        color: var(--text-secondary);
        padding-top: 18rpx;
      }
      
      .duration-editor {
        display: flex;
        flex-direction: column;
        justify-content: flex-end;
        gap: 12rpx;
        flex: 1;
      }

      .duration-slider-row {
        display: flex;
        align-items: center;
        gap: 12rpx;
        width: 100%;
        padding: 8rpx 14rpx;
        background: var(--bg-elevated);
        border: 1rpx solid var(--border-subtle);
        border-radius: var(--radius-sm);

        .duration-slider {
          flex: 1;
          margin: 0;
        }

        .duration-unit {
          width: 28rpx;
          font-size: 24rpx;
          color: var(--text-tertiary);
        }

        .duration-value {
          width: 72rpx;
          font-size: 24rpx;
          color: var(--neon-cyan);
          font-weight: 700;
          text-align: right;
        }
      }

    }
  }
}

.safe-area-bottom {
  height: 60rpx;
}

/* 动画 */
@keyframes slide-up {
  from {
    opacity: 0;
    transform: translateY(40rpx);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}

@keyframes pulse {
  0%, 100% {
    opacity: 1;
  }
  50% {
    opacity: 0.5;
  }
}

/* 小屏幕适配 */
@media screen and (max-width: 375px) {
  .container {
    padding: 30rpx 20rpx 50rpx;
  }
  
  .device-card {
    padding: 24rpx;
    
    .device-header {
      .device-icon {
        width: 80rpx;
        height: 80rpx;
        
        text {
          font-size: 40rpx;
        }
      }
      
      .device-info {
        .device-name {
          font-size: 30rpx;
        }
      }
    }
  }
  
  .reminder-grid {
    gap: 12rpx;
    
    .reminder-card {
      padding: 28rpx 16rpx;
      
      .card-icon-wrap {
        width: 72rpx;
        height: 72rpx;
        
        .card-icon {
          font-size: 32rpx;
        }
      }
      
      .card-name {
        font-size: 24rpx;
      }
    }
  }
}
</style>
