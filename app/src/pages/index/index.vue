<template>
  <PageShell tone="cyber">
    <view class="container">
      <!-- 顶部欢迎区 -->
      <view class="header-section">
        <SectionHeader title="坐姿监测" subtitle="POSTURE MONITORING SYSTEM">
          <template #right>
            <StatusBadge
              :online="store.state.isOnline"
              online-text="ONLINE"
              offline-text="OFFLINE"
            />
          </template>
        </SectionHeader>
      </view>

      <!-- 核心数据卡片 -->
      <view class="data-overview">
        <StatCard
          class="data-card main-card"
          variant="primary"
          icon="⏱"
          :value="store.usageTimeText"
          label="今日时长"
        />
        <view class="data-card-group">
          <StatCard
            class="data-card small-card"
            :value="store.state.isOnline ? store.postureText : '--'"
            label="当前姿势"
            :value-class="store.state.isOnline ? store.postureType : ''"
          />
          <StatCard
            class="data-card small-card"
            variant="warning"
            :value="store.state.todayAbnormalCount"
            label="异常次数"
            value-class="highlight"
          />
        </view>
      </view>

      <!-- 实时姿势监控 -->
      <view class="monitor-section">
        <view class="section-label">
          <text class="label-icon">◈</text>
          <text class="label-text">实时监控</text>
          <text class="update-time">{{ store.lastUpdateTimeText }}</text>
        </view>
        
        <view class="posture-display" :class="store.state.isOnline ? (store.state.isPosture ? 'good' : 'bad') : 'offline'">
          <!-- 姿势可视化 - 科技感人形 -->
          <view class="posture-visual">
            <view class="human-figure" :class="{ abnormal: !store.state.isPosture && store.state.isOnline }">
              <!-- 头部 -->
              <view class="figure-head">
                <view class="head-inner"></view>
                <view class="head-glow"></view>
              </view>
              <!-- 脊柱 -->
              <view class="spine">
                <view class="spine-segment" v-for="i in 5" :key="i"></view>
              </view>
              <!-- 肩膀 -->
              <view class="shoulders">
                <view class="shoulder left"></view>
                <view class="shoulder right"></view>
              </view>
            </view>
            
            <!-- 扫描效果 -->
            <view class="scan-overlay" v-if="store.state.isOnline">
              <view class="scan-beam"></view>
            </view>
          </view>
          
          <!-- 状态信息 -->
          <view class="posture-info">
            <view class="status-code">{{ store.state.isOnline ? 'SYS-001' : 'SYS-OFF' }}</view>
            <text class="posture-status">{{ store.state.isOnline ? (store.state.isPosture ? '姿势正常' : '姿势异常') : '设备离线' }}</text>
            <text class="posture-desc">{{ store.state.isOnline ? (store.state.isPosture ? '脊柱对齐良好，保持当前状态' : '检测到脊柱弯曲，请立即调整') : '等待设备连接...' }}</text>
            <view class="mode-tag">
              <text class="tag-dot"></text>
              <text>{{ store.modeText }}</text>
            </view>
          </view>
        </view>
      </view>

      <!-- 今日健康评分 -->
      <view class="health-section">
        <view class="section-label">
          <text class="label-icon">◈</text>
          <text class="label-text">健康评分</text>
        </view>
        
        <view class="health-card">
          <view class="score-display">
            <view class="score-ring" :class="scoreLevel">
              <svg class="ring-svg" viewBox="0 0 100 100">
                <circle class="ring-bg" cx="50" cy="50" r="42"/>
                <circle class="ring-progress" cx="50" cy="50" r="42" :style="{ strokeDashoffset: 264 - (264 * store.healthScore / 100) }"/>
              </svg>
              <view class="score-value">
                <text class="value">{{ store.healthScore }}</text>
                <text class="unit">分</text>
              </view>
            </view>
          </view>
          
          <view class="score-stats">
            <view class="stat-item">
              <view class="stat-bar good">
                <view class="bar-fill" :style="{ width: goodPercent + '%' }"></view>
              </view>
              <view class="stat-label">
                <text class="dot good"></text>
                <text>良好</text>
              </view>
              <text class="stat-value">{{ goodPercent }}%</text>
            </view>
            
            <view class="stat-item">
              <view class="stat-bar bad">
                <view class="bar-fill" :style="{ width: badPercent + '%' }"></view>
              </view>
              <view class="stat-label">
                <text class="dot bad"></text>
                <text>异常</text>
              </view>
              <text class="stat-value">{{ badPercent }}%</text>
            </view>
            
            <view class="stat-item">
              <view class="stat-indicator" :class="store.state.isOnline ? (store.state.monitoringEnabled ? 'active' : 'paused') : 'offline'"></view>
              <view class="stat-label">
                <text class="dot" :class="store.state.isOnline ? (store.state.monitoringEnabled ? 'good' : 'neutral') : 'bad'"></text>
                <text>监控状态</text>
              </view>
              <text class="stat-value">
                {{ store.state.isOnline ? (store.state.monitoringEnabled ? '运行中' : '已暂停') : '离线' }}
              </text>
            </view>
          </view>
        </view>
      </view>

      <!-- 快捷操作 -->
      <view class="quick-actions">
        <view class="action-item" @click="goTo('/pages/monitor/index')" :style="{ animationDelay: '100ms' }">
          <view class="action-icon">
            <text>◉</text>
          </view>
          <text class="action-text">监控</text>
          <text class="action-desc">实时数据</text>
        </view>
        <view class="action-item" @click="goTo('/pages/history/index')" :style="{ animationDelay: '200ms' }">
          <view class="action-icon">
            <text>◫</text>
          </view>
          <text class="action-text">历史</text>
          <text class="action-desc">数据回顾</text>
        </view>
        <view class="action-item" @click="goTo('/pages/control/index')" :style="{ animationDelay: '300ms' }">
          <view class="action-icon">
            <text>⚙</text>
          </view>
          <text class="action-text">控制</text>
          <text class="action-desc">设备设置</text>
        </view>
      </view>
    </view>
  </PageShell>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import store from '@/utils/store'
import PageShell from '@/components/ui/PageShell.vue'
import SectionHeader from '@/components/ui/SectionHeader.vue'
import StatusBadge from '@/components/ui/StatusBadge.vue'
import StatCard from '@/components/ui/StatCard.vue'

/*
 * 页面职责：首页总览。
 * - 汇总展示在线状态、实时姿势、健康评分与快捷入口。
 * - 主要消费 store 中的衍生数据，避免在页面重复业务计算。
 */

// 计算属性：将全局健康评分拆分为“良好/异常”占比，供进度条直接渲染。
const goodPercent = computed(() => store.healthScore)
const badPercent = computed(() => 100 - store.healthScore)
const scoreLevel = computed(() => {
  const s = store.healthScore
  // 评分阈值与视觉分档保持一致，避免同分值在不同页面出现风格漂移。
  if (s >= 85) return 'excellent'
  if (s >= 70) return 'good'
  return 'poor'
})

// 页面跳转
function goTo(url: string) {
  uni.switchTab({ url })
}
</script>

<style lang="scss" scoped>
.container {
  padding: 40rpx 30rpx 60rpx;
  min-height: 100vh;
}

/* 顶部区域 */
.header-section {
  margin-bottom: 40rpx;
}

/* 核心数据卡片 */
.data-overview {
  display: flex;
  gap: 24rpx;
  margin-bottom: 40rpx;
  
  .data-card {
    animation: slide-up var(--duration-slow) var(--ease-out) both;
  }
  
  .main-card {
    flex: 1.2;
  }
  
  .data-card-group {
    flex: 1;
    display: flex;
    flex-direction: column;
    gap: 24rpx;
    
    .small-card {
      flex: 1;
    }
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
  
  .update-time {
    margin-left: auto;
    font-size: 22rpx;
    color: var(--text-muted);
    font-family: var(--font-mono);
  }
}

/* 实时监控区域 */
.monitor-section {
  margin-bottom: 40rpx;
  animation: slide-up var(--duration-slow) var(--ease-out) 100ms both;
  
  .posture-display {
    background: linear-gradient(135deg, var(--bg-tertiary) 0%, var(--bg-secondary) 100%);
    border-radius: var(--radius-xl);
    padding: 40rpx;
    display: flex;
    align-items: center;
    gap: 40rpx;
    border: 1rpx solid var(--border-subtle);
    position: relative;
    overflow: hidden;
    
    /* 状态发光边框 */
    &::before {
      content: '';
      position: absolute;
      inset: 0;
      border-radius: var(--radius-xl);
      padding: 2rpx;
      background: linear-gradient(135deg, var(--neon-cyan) 0%, transparent 50%);
      -webkit-mask: linear-gradient(#fff 0 0) content-box, linear-gradient(#fff 0 0);
      mask: linear-gradient(#fff 0 0) content-box, linear-gradient(#fff 0 0);
      -webkit-mask-composite: xor;
      mask-composite: exclude;
      opacity: 0.5;
    }
    
    &.good::before {
      background: linear-gradient(135deg, var(--state-success) 0%, transparent 50%);
    }
    
    &.bad::before {
      background: linear-gradient(135deg, var(--state-danger) 0%, transparent 50%);
    }
    
    &.offline::before {
      background: linear-gradient(135deg, var(--state-offline) 0%, transparent 50%);
    }
  }
  
  /* 姿势可视化 */
  .posture-visual {
    position: relative;
    width: 180rpx;
    height: 220rpx;
    display: flex;
    align-items: center;
    justify-content: center;
    
    .human-figure {
      display: flex;
      flex-direction: column;
      align-items: center;
      transition: transform 0.5s var(--ease-spring);
      
      .figure-head {
        width: 56rpx;
        height: 56rpx;
        border-radius: 50%;
        background: var(--neon-cyan);
        position: relative;
        margin-bottom: 16rpx;
        box-shadow: 0 0 30rpx rgba(0, 240, 255, 0.5);
        
        .head-inner {
          position: absolute;
          inset: 8rpx;
          border-radius: 50%;
          background: var(--bg-primary);
        }
        
        .head-glow {
          position: absolute;
          inset: -8rpx;
          border-radius: 50%;
          border: 2rpx solid var(--neon-cyan);
          opacity: 0.5;
          animation: head-pulse 2s ease-in-out infinite;
        }
      }
      
      .spine {
        display: flex;
        flex-direction: column;
        gap: 8rpx;
        align-items: center;
        
        .spine-segment {
          width: 20rpx;
          height: 20rpx;
          border-radius: 50%;
          background: var(--neon-cyan);
          opacity: 0.8;
          box-shadow: 0 0 15rpx rgba(0, 240, 255, 0.3);
          
          &:nth-child(2) { opacity: 0.65; }
          &:nth-child(3) { opacity: 0.5; }
          &:nth-child(4) { opacity: 0.35; }
          &:nth-child(5) { opacity: 0.2; }
        }
      }
      
      .shoulders {
        position: absolute;
        top: 80rpx;
        display: flex;
        gap: 40rpx;
        
        .shoulder {
          width: 32rpx;
          height: 12rpx;
          border-radius: 6rpx;
          background: var(--neon-cyan);
          opacity: 0.7;
          box-shadow: 0 0 15rpx rgba(0, 240, 255, 0.3);
        }
      }
      
      &.abnormal {
        transform: rotate(15deg) translateX(10rpx);
        
        .figure-head, .spine-segment, .shoulder {
          background: var(--state-danger);
          box-shadow: 0 0 30rpx rgba(255, 82, 82, 0.5);
        }
        
        .head-glow {
          border-color: var(--state-danger);
        }
      }
    }
    
    .scan-overlay {
      position: absolute;
      inset: 0;
      pointer-events: none;
      overflow: hidden;
      
      .scan-beam {
        position: absolute;
        left: 0;
        right: 0;
        height: 4rpx;
        background: linear-gradient(90deg, transparent, var(--neon-cyan), transparent);
        box-shadow: 0 0 20rpx var(--neon-cyan);
        animation: scan-beam 2s ease-in-out infinite;
      }
    }
  }
  
  /* 姿势信息 */
  .posture-info {
    flex: 1;
    
    .status-code {
      font-size: 22rpx;
      color: var(--text-muted);
      font-family: var(--font-mono);
      margin-bottom: 12rpx;
      letter-spacing: 1rpx;
    }
    
    .posture-status {
      display: block;
      font-size: 40rpx;
      font-weight: 800;
      color: var(--text-primary);
      margin-bottom: 12rpx;
      letter-spacing: 1rpx;
    }
    
    .posture-desc {
      display: block;
      font-size: 26rpx;
      color: var(--text-tertiary);
      line-height: 1.5;
      margin-bottom: 24rpx;
    }
    
    .mode-tag {
      display: inline-flex;
      align-items: center;
      gap: 10rpx;
      padding: 10rpx 20rpx;
      background: var(--neon-cyan-dim);
      border-radius: var(--radius-pill);
      border: 1rpx solid rgba(0, 240, 255, 0.3);
      
      .tag-dot {
        width: 8rpx;
        height: 8rpx;
        border-radius: 50%;
        background: var(--neon-cyan);
        box-shadow: 0 0 10rpx var(--neon-cyan);
      }
      
      text {
        font-size: 24rpx;
        color: var(--neon-cyan);
        font-weight: 600;
      }
    }
  }
}

/* 健康评分区域 */
.health-section {
  margin-bottom: 40rpx;
  animation: slide-up var(--duration-slow) var(--ease-out) 200ms both;
  
  .health-card {
    background: linear-gradient(135deg, var(--bg-tertiary) 0%, var(--bg-secondary) 100%);
    border-radius: var(--radius-xl);
    padding: 40rpx;
    display: flex;
    align-items: center;
    gap: 50rpx;
    border: 1rpx solid var(--border-subtle);
  }
  
  .score-display {
    .score-ring {
      position: relative;
      width: 200rpx;
      height: 200rpx;
      
      .ring-svg {
        width: 100%;
        height: 100%;
        transform: rotate(-90deg);
        
        circle {
          fill: none;
          stroke-width: 8;
        }
        
        .ring-bg {
          stroke: rgba(255, 255, 255, 0.1);
        }
        
        .ring-progress {
          stroke: var(--neon-cyan);
          stroke-linecap: round;
          stroke-dasharray: 264;
          transition: stroke-dashoffset 1s var(--ease-out);
          filter: drop-shadow(0 0 10rpx var(--neon-cyan));
        }
      }
      
      &.excellent .ring-progress {
        stroke: var(--state-success);
        filter: drop-shadow(0 0 10rpx var(--state-success));
      }
      
      &.good .ring-progress {
        stroke: var(--state-warning);
        filter: drop-shadow(0 0 10rpx var(--state-warning));
      }
      
      &.poor .ring-progress {
        stroke: var(--state-danger);
        filter: drop-shadow(0 0 10rpx var(--state-danger));
      }
      
      .score-value {
        position: absolute;
        inset: 0;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        
        .value {
          font-size: 64rpx;
          font-weight: 800;
          color: var(--text-primary);
          line-height: 1;
        }
        
        .unit {
          font-size: 24rpx;
          color: var(--text-tertiary);
          margin-top: 4rpx;
        }
      }
    }
  }
  
  .score-stats {
    flex: 1;
    display: flex;
    flex-direction: column;
    gap: 24rpx;
    
    .stat-item {
      display: flex;
      align-items: center;
      gap: 16rpx;
      
      .stat-bar {
        width: 120rpx;
        height: 8rpx;
        background: rgba(255, 255, 255, 0.1);
        border-radius: 4rpx;
        overflow: hidden;
        
        .bar-fill {
          height: 100%;
          border-radius: 4rpx;
          transition: width 0.5s var(--ease-out);
        }
        
        &.good .bar-fill {
          background: var(--state-success);
          box-shadow: 0 0 10rpx var(--state-success);
        }
        
        &.bad .bar-fill {
          background: var(--state-danger);
          box-shadow: 0 0 10rpx var(--state-danger);
        }
      }
      
      .stat-indicator {
        width: 120rpx;
        height: 8rpx;
        border-radius: 4rpx;
        
        &.active {
          background: var(--state-success);
          box-shadow: 0 0 10rpx var(--state-success);
          animation: pulse-bar 2s ease-in-out infinite;
        }
        
        &.paused {
          background: var(--state-warning);
        }
        
        &.offline {
          background: var(--state-offline);
        }
      }
      
      .stat-label {
        flex: 1;
        display: flex;
        align-items: center;
        gap: 10rpx;
        font-size: 26rpx;
        color: var(--text-secondary);
        
        .dot {
          width: 10rpx;
          height: 10rpx;
          border-radius: 50%;
          
          &.good {
            background: var(--state-success);
            box-shadow: 0 0 8rpx var(--state-success);
          }
          
          &.bad {
            background: var(--state-danger);
            box-shadow: 0 0 8rpx var(--state-danger);
          }
          
          &.neutral {
            background: var(--text-muted);
          }
        }
      }
      
      .stat-value {
        font-size: 28rpx;
        font-weight: 700;
        color: var(--text-primary);
        font-family: var(--font-mono);
      }
    }
  }
}

/* 快捷操作 */
.quick-actions {
  display: flex;
  gap: 24rpx;
  animation: slide-up var(--duration-slow) var(--ease-out) 300ms both;
  
  .action-item {
    flex: 1;
    background: linear-gradient(135deg, var(--bg-tertiary) 0%, var(--bg-secondary) 100%);
    border-radius: var(--radius-lg);
    padding: 32rpx 20rpx;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 12rpx;
    border: 1rpx solid var(--border-subtle);
    transition: all var(--duration-base) var(--ease-out);
    position: relative;
    overflow: hidden;
    animation: card-enter var(--duration-slow) var(--ease-out) both;
    
    &::before {
      content: '';
      position: absolute;
      top: 0;
      left: 0;
      right: 0;
      height: 2rpx;
      background: linear-gradient(90deg, transparent, var(--neon-cyan), transparent);
      opacity: 0;
      transition: opacity var(--duration-base);
    }
    
    &:active {
      transform: scale(0.96);
      border-color: var(--neon-cyan);
      box-shadow: var(--neon-cyan-glow);
      
      &::before {
        opacity: 1;
      }
    }
    
    .action-icon {
      width: 80rpx;
      height: 80rpx;
      border-radius: var(--radius-md);
      background: var(--neon-cyan-dim);
      display: flex;
      align-items: center;
      justify-content: center;
      border: 1rpx solid rgba(0, 240, 255, 0.3);
      
      text {
        font-size: 36rpx;
        color: var(--neon-cyan);
      }
    }
    
    .action-text {
      font-size: 28rpx;
      font-weight: 700;
      color: var(--text-primary);
    }
    
    .action-desc {
      font-size: 22rpx;
      color: var(--text-tertiary);
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

@keyframes head-pulse {
  0%, 100% {
    transform: scale(1);
    opacity: 0.5;
  }
  50% {
    transform: scale(1.2);
    opacity: 0.8;
  }
}

@keyframes scan-beam {
  0%, 100% {
    top: 0;
    opacity: 0;
  }
  50% {
    top: 100%;
    opacity: 1;
  }
}

@keyframes pulse-bar {
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
  
  .data-overview {
    gap: 16rpx;
    
    .data-card-group {
      gap: 16rpx;
    }
  }
  
  .monitor-section {
    .posture-display {
      padding: 30rpx;
      gap: 30rpx;
    }
    
    .posture-visual {
      width: 140rpx;
      height: 180rpx;
    }
    
    .posture-info {
      .posture-status {
        font-size: 32rpx;
      }
      
      .posture-desc {
        font-size: 24rpx;
      }
    }
  }
  
  .health-section {
    .health-card {
      padding: 30rpx;
      gap: 30rpx;
    }
    
    .score-display {
      .score-ring {
        width: 160rpx;
        height: 160rpx;
        
        .score-value {
          .value {
            font-size: 48rpx;
          }
        }
      }
    }
  }
  
  .quick-actions {
    gap: 16rpx;
    
    .action-item {
      padding: 24rpx 16rpx;
      
      .action-icon {
        width: 64rpx;
        height: 64rpx;
        
        text {
          font-size: 28rpx;
        }
      }
      
      .action-text {
        font-size: 24rpx;
      }
    }
  }
}
</style>
