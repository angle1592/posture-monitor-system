<template>
  <PageShell tone="cyber">
    <view class="container">
      <!-- 页面标题 -->
      <view class="page-header">
        <SectionHeader title="实时监控" subtitle="REAL-TIME MONITORING">
          <template #right>
            <StatusBadge
              :online="store.state.isOnline"
              online-text="LIVE"
              offline-text="OFFLINE"
            />
          </template>
        </SectionHeader>
      </view>

      <!-- 关键指标卡片 -->
      <view class="metrics-grid">
        <StatCard
          class="metric-card"
          icon="👤"
          :value="store.state.isOnline ? store.postureText : '--'"
          label="当前姿势"
          :value-class="store.state.isOnline ? store.postureType : ''"
        />
        
        <StatCard
          class="metric-card"
          variant="warning"
          icon="⚠️"
          :value="store.state.todayAbnormalCount"
          label="异常次数"
        />
        
        <StatCard
          class="metric-card"
          variant="success"
          icon="★"
          :value="store.healthScore"
          label="健康评分"
        />
      </view>

      <!-- 姿势详情 -->
      <view class="detail-section">
        <view class="section-label">
          <text class="label-icon">◈</text>
          <text class="label-text">姿势详情</text>
        </view>
        
        <view class="detail-card">
          <view class="detail-row">
            <view class="detail-cell">
              <view class="cell-icon">🪑</view>
              <view class="cell-content">
                <text class="cell-label">当前姿态</text>
                <view class="status-pill" :class="getPostureStatusClass">
                  {{ store.state.isOnline ? store.postureText : '离线' }}
                </view>
              </view>
            </view>
            
            <view class="detail-cell">
              <view class="cell-icon">🎯</view>
              <view class="cell-content">
                <text class="cell-label">监控模式</text>
                <text class="cell-value">{{ store.state.isOnline ? store.modeText : '--' }}</text>
              </view>
            </view>
          </view>
          
          <view class="detail-divider"></view>
          
          <view class="detail-row">
            <view class="detail-cell">
              <view class="cell-icon">📡</view>
              <view class="cell-content">
                <text class="cell-label">设备状态</text>
                <view class="status-pill" :class="store.state.isOnline ? 'online' : 'offline'">
                  {{ store.state.isOnline ? '在线' : '离线' }}
                </view>
              </view>
            </view>
            
            <view class="detail-cell">
              <view class="cell-icon">⏱️</view>
              <view class="cell-content">
                <text class="cell-label">最后更新</text>
                <text class="cell-value mono">{{ store.lastUpdateTimeText }}</text>
              </view>
            </view>
          </view>
        </view>
      </view>

      <!-- 实时日志 -->
      <view class="log-section">
        <view class="section-label">
          <text class="label-icon">◈</text>
          <text class="label-text">系统日志</text>
          <view class="clear-btn" @click="clearLogs">
            <text>⌫ 清空</text>
          </view>
        </view>
        
        <view class="log-terminal">
          <view class="terminal-header">
            <view class="window-controls">
              <view class="dot red"></view>
              <view class="dot yellow"></view>
              <view class="dot green"></view>
            </view>
            <text class="terminal-title">system.log</text>
          </view>
          
          <scroll-view class="log-content" scroll-y enhanced :show-scrollbar="false">
            <view v-if="logs.length === 0" class="log-empty">
              <text class="empty-icon">_</text>
              <text class="empty-text">等待数据输入...</text>
            </view>
            
            <view
              v-for="(log, index) in logs"
              :key="index"
              class="log-entry"
              :class="log.type"
            >
              <text class="log-time">[{{ log.time }}]</text>
              <text class="log-type">{{ getLogTypeLabel(log.type) }}</text>
              <text class="log-message">{{ log.message }}</text>
            </view>
          </scroll-view>
        </view>
      </view>
    </view>
  </PageShell>
</template>

<script setup lang="ts">
import { ref, watch, computed } from 'vue'
import { onShow, onHide } from '@dcloudio/uni-app'
import store from '@/utils/store'
import PageShell from '@/components/ui/PageShell.vue'
import SectionHeader from '@/components/ui/SectionHeader.vue'
import StatusBadge from '@/components/ui/StatusBadge.vue'
import StatCard from '@/components/ui/StatCard.vue'

/*
 * 页面职责：实时监控页。
 * - 展示当前姿势、设备在线状态、评分与系统日志。
 * - 通过 watch 监听全局状态变化并记录事件日志。
 * - 页面可见期间切换为高频轮询，离开时恢复常规轮询。
 */

// 日志记录结构：用于终端风格滚动日志展示。
interface LogEntry {
  time: string
  message: string
  type: 'info' | 'success' | 'warning' | 'error'
}

const logs = ref<LogEntry[]>([])

// 姿势状态胶囊样式映射：在线时区分正常/异常/无人/未知，离线单独处理。
const getPostureStatusClass = computed(() => {
  if (!store.state.isOnline) return 'offline'
  if (store.state.postureType === 'normal') return 'normal'
  if (store.state.postureType === 'no_person') return 'idle'
  if (store.state.postureType === 'unknown') return 'unknown'
  return 'danger'
})

function describePostureChange(postureType: string): { message: string; type: LogEntry['type'] } {
  if (postureType === 'normal') {
    return { message: '姿态已恢复正常', type: 'success' }
  }
  if (postureType === 'head_down') {
    return { message: '检测到低头', type: 'warning' }
  }
  if (postureType === 'hunchback') {
    return { message: '检测到驼背', type: 'warning' }
  }
  if (postureType === 'no_person') {
    return { message: '当前无人', type: 'info' }
  }
  return { message: '姿态状态未知', type: 'error' }
}

const getLogTypeLabel = (type: string) => {
  const labels: Record<string, string> = {
    info: 'INFO',
    success: 'OK',
    warning: 'WARN',
    error: 'ERR'
  }
  return labels[type] || 'INFO'
}

// 添加日志
function addLog(message: string, type: LogEntry['type'] = 'info') {
  const now = new Date()
  const time = now.toLocaleTimeString('zh-CN', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  })
  logs.value.unshift({ time, message, type })
  if (logs.value.length > 50) {
    // 终端风格日志只保留最近窗口，避免长会话下渲染持续变慢。
    logs.value = logs.value.slice(0, 50)
  }
}

// 清空日志
function clearLogs() {
  logs.value = []
  addLog('日志已清空', 'info')
}

// 监听姿势变化：只记录边沿变化，避免相同状态重复刷日志。
watch(
  () => store.state.postureType,
  (newVal, oldVal) => {
    if (newVal !== oldVal) {
      const entry = describePostureChange(newVal)
      addLog(entry.message, entry.type)
    }
  }
)

// 监听在线状态变化：用于记录上线/离线事件。
watch(
  () => store.state.isOnline,
  (newVal, oldVal) => {
    if (newVal !== oldVal) {
      addLog(newVal ? '设备已上线' : '设备已离线', newVal ? 'success' : 'error')
    }
  }
)

// 页面显示时
onShow(() => {
  addLog('监控页面已激活', 'info')
  // 监控页进入后切到高频轮询，优先实时性。
  store.setPollingProfile('realtime')
})

// 页面隐藏时
onHide(() => {
  // 离开监控页恢复常规轮询，降低无效请求压力。
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

/* 指标网格 */
.metrics-grid {
  display: flex;
  gap: 20rpx;
  margin-bottom: 40rpx;
  animation: slide-up var(--duration-slow) var(--ease-out) 100ms both;
  
  .metric-card {
    flex: 1;
    animation: card-enter var(--duration-slow) var(--ease-out) both;
    
    &:nth-child(1) { animation-delay: 100ms; }
    &:nth-child(2) { animation-delay: 150ms; }
    &:nth-child(3) { animation-delay: 200ms; }
  }
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
  
  .clear-btn {
    margin-left: auto;
    padding: 10rpx 20rpx;
    background: var(--bg-tertiary);
    border-radius: var(--radius-pill);
    border: 1rpx solid var(--border-subtle);
    
    text {
      font-size: 24rpx;
      color: var(--text-tertiary);
    }
    
    &:active {
      background: var(--bg-elevated);
      border-color: var(--neon-cyan);
    }
  }
}

/* 详情区域 */
.detail-section {
  margin-bottom: 40rpx;
  animation: slide-up var(--duration-slow) var(--ease-out) 200ms both;
  
  .detail-card {
    background: linear-gradient(135deg, var(--bg-tertiary) 0%, var(--bg-secondary) 100%);
    border-radius: var(--radius-xl);
    padding: 32rpx;
    border: 1rpx solid var(--border-subtle);
    
    .detail-row {
      display: flex;
      gap: 24rpx;
      
      .detail-cell {
        flex: 1;
        display: flex;
        align-items: center;
        gap: 20rpx;
        
        .cell-icon {
          width: 72rpx;
          height: 72rpx;
          border-radius: var(--radius-md);
          background: var(--neon-cyan-dim);
          display: flex;
          align-items: center;
          justify-content: center;
          font-size: 36rpx;
          border: 1rpx solid rgba(0, 240, 255, 0.2);
        }
        
        .cell-content {
          flex: 1;
          
          .cell-label {
            display: block;
            font-size: 24rpx;
            color: var(--text-tertiary);
            margin-bottom: 8rpx;
          }
          
          .cell-value {
            display: block;
            font-size: 30rpx;
            font-weight: 700;
            color: var(--text-primary);
            
            &.mono {
              font-family: var(--font-mono);
              color: var(--neon-cyan);
            }
          }
          
          .status-pill {
            display: inline-flex;
            padding: 8rpx 20rpx;
            border-radius: var(--radius-pill);
            font-size: 24rpx;
            font-weight: 700;
            
            &.normal {
              background: rgba(0, 230, 118, 0.15);
              color: var(--state-success);
              border: 1rpx solid rgba(0, 230, 118, 0.3);
            }
            
             &.danger {
               background: rgba(255, 82, 82, 0.15);
               color: var(--state-danger);
               border: 1rpx solid rgba(255, 82, 82, 0.3);
             }

             &.idle {
               background: rgba(245, 158, 11, 0.15);
               color: #f59e0b;
               border: 1rpx solid rgba(245, 158, 11, 0.3);
             }

             &.unknown {
               background: rgba(168, 85, 247, 0.15);
               color: #c084fc;
               border: 1rpx solid rgba(192, 132, 252, 0.3);
             }
             
             &.online {
               background: rgba(0, 230, 118, 0.15);
              color: var(--state-success);
              border: 1rpx solid rgba(0, 230, 118, 0.3);
            }
            
            &.offline {
              background: rgba(100, 116, 139, 0.2);
              color: var(--state-offline);
              border: 1rpx solid rgba(100, 116, 139, 0.4);
            }
          }
        }
      }
    }
    
    .detail-divider {
      height: 1rpx;
      background: var(--border-subtle);
      margin: 24rpx 0;
    }
  }
}

/* 日志区域 */
.log-section {
  animation: slide-up var(--duration-slow) var(--ease-out) 300ms both;
  
  .log-terminal {
    background: #0d1117;
    border-radius: var(--radius-xl);
    overflow: hidden;
    border: 1rpx solid var(--border-subtle);
    
    .terminal-header {
      display: flex;
      align-items: center;
      gap: 16rpx;
      padding: 20rpx 24rpx;
      background: rgba(255, 255, 255, 0.03);
      border-bottom: 1rpx solid var(--border-subtle);
      
      .window-controls {
        display: flex;
        gap: 10rpx;
        
        .dot {
          width: 14rpx;
          height: 14rpx;
          border-radius: 50%;
          
          &.red { background: #ff5f56; }
          &.yellow { background: #ffbd2e; }
          &.green { background: #27c93f; }
        }
      }
      
      .terminal-title {
        font-size: 24rpx;
        color: var(--text-tertiary);
        font-family: var(--font-mono);
      }
    }
    
    .log-content {
      height: 400rpx;
      padding: 20rpx;
      
      .log-empty {
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        height: 100%;
        color: var(--text-muted);
        
        .empty-icon {
          font-size: 48rpx;
          opacity: 0.5;
          margin-bottom: 16rpx;
        }
        
        .empty-text {
          font-size: 26rpx;
        }
      }
      
      .log-entry {
        display: flex;
        align-items: flex-start;
        gap: 16rpx;
        padding: 12rpx 0;
        font-family: var(--font-mono);
        font-size: 24rpx;
        line-height: 1.5;
        border-bottom: 1rpx solid rgba(255, 255, 255, 0.03);
        animation: log-fade-in var(--duration-base) var(--ease-out) both;
        
        .log-time {
          color: var(--text-muted);
          flex-shrink: 0;
        }
        
        .log-type {
          width: 60rpx;
          flex-shrink: 0;
          font-weight: 700;
        }
        
        .log-message {
          color: var(--text-secondary);
          flex: 1;
          word-break: break-all;
        }
        
        &.info {
          .log-type { color: #58a6ff; }
        }
        
        &.success {
          .log-type { color: var(--state-success); }
        }
        
        &.warning {
          .log-type { color: var(--state-warning); }
        }
        &.error {
          .log-type { color: var(--state-danger); }
        }
      }
    }
  }
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

@keyframes card-enter {
  from {
    opacity: 0;
    transform: translateY(30rpx) scale(0.95);
  }
  to {
    opacity: 1;
    transform: translateY(0) scale(1);
  }
}

@keyframes log-fade-in {
  from {
    opacity: 0;
    transform: translateX(-20rpx);
  }
  to {
    opacity: 1;
    transform: translateX(0);
  }
}

/* 小屏幕适配 */
@media screen and (max-width: 375px) {
  .container {
    padding: 30rpx 20rpx 50rpx;
  }
  
  .metrics-grid {
    gap: 16rpx;
  }
  
  .detail-section {
    .detail-card {
      padding: 24rpx;
      
      .detail-row {
        flex-direction: column;
        gap: 20rpx;
      }
      
      .detail-divider {
        margin: 16rpx 0;
      }
    }
  }
  
  .log-section {
    .log-terminal {
      .log-content {
        height: 320rpx;
        
        .log-entry {
          font-size: 22rpx;
          gap: 12rpx;
          
          .log-type {
            width: 50rpx;
          }
        }
      }
    }
  }
}
</style>
