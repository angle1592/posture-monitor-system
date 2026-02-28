<template>
  <view class="stat-card-cyber" :class="[variantClass, { 'has-icon': icon }]">
    <view v-if="icon" class="icon-wrap">
      <text class="icon">{{ icon }}</text>
    </view>
    <view class="content">
      <text class="value" :class="valueClass">{{ value }}</text>
      <text class="label">{{ label }}</text>
    </view>
    <view class="glow-border"></view>
    <slot />
  </view>
</template>

<script setup lang="ts">
import { computed } from 'vue'

const props = withDefaults(
  defineProps<{
    value: string | number
    label: string
    icon?: string
    variant?: 'default' | 'primary' | 'warning' | 'success' | 'cyber'
    valueClass?: string
  }>(),
  {
    icon: '',
    variant: 'default',
    valueClass: '',
  }
)

const variantClass = computed(() => `variant-${props.variant}`)
</script>

<style lang="scss" scoped>
.stat-card-cyber {
  position: relative;
  background: linear-gradient(135deg, var(--bg-tertiary) 0%, var(--bg-secondary) 100%);
  border-radius: var(--radius-lg);
  padding: 32rpx 28rpx;
  display: flex;
  flex-direction: column;
  gap: 12rpx;
  overflow: hidden;
  border: 1rpx solid var(--border-subtle);
  box-shadow: var(--shadow-md);
  animation: card-enter var(--duration-slow) var(--ease-out) both;
  
  /* 玻璃拟态效果 */
  &::before {
    content: '';
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    background: linear-gradient(135deg, rgba(255,255,255,0.05) 0%, transparent 50%);
    pointer-events: none;
  }
  
  /* 发光边框 */
  .glow-border {
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    border-radius: var(--radius-lg);
    border: 1rpx solid transparent;
    pointer-events: none;
    transition: all var(--duration-base);
  }
  
  .icon-wrap {
    width: 64rpx;
    height: 64rpx;
    border-radius: var(--radius-md);
    background: var(--neon-cyan-dim);
    display: flex;
    align-items: center;
    justify-content: center;
    margin-bottom: 8rpx;
    
    .icon {
      font-size: 32rpx;
    }
  }
  
  .content {
    position: relative;
    z-index: 1;
  }
  
  .value {
    font-size: 48rpx;
    font-weight: 800;
    color: var(--text-primary);
    line-height: 1.1;
    letter-spacing: -1rpx;
    
    &.heading {
      background: linear-gradient(135deg, var(--neon-cyan) 0%, #fff 100%);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      background-clip: text;
    }
    
    &.success {
      color: var(--state-success);
      text-shadow: 0 0 20rpx rgba(0, 230, 118, 0.4);
    }
    
    &.warning {
      color: var(--state-warning);
      text-shadow: 0 0 20rpx rgba(255, 171, 0, 0.4);
    }
    
    &.danger {
      color: var(--state-danger);
      text-shadow: 0 0 20rpx rgba(255, 82, 82, 0.4);
    }
  }
  
  .label {
    font-size: 24rpx;
    color: var(--text-tertiary);
    font-weight: 500;
    text-transform: uppercase;
    letter-spacing: 1rpx;
  }
  
  /* 变体样式 */
  &.variant-primary {
    background: linear-gradient(135deg, rgba(0, 240, 255, 0.15) 0%, var(--bg-tertiary) 100%);
    
    .glow-border {
      border-color: var(--neon-cyan);
      box-shadow: 0 0 30rpx rgba(0, 240, 255, 0.2), inset 0 0 30rpx rgba(0, 240, 255, 0.05);
    }
    
    .icon-wrap {
      background: var(--neon-cyan);
      box-shadow: 0 0 20rpx rgba(0, 240, 255, 0.5);
    }
  }
  
  &.variant-cyber {
    background: linear-gradient(135deg, rgba(168, 85, 247, 0.15) 0%, var(--bg-tertiary) 100%);
    
    .glow-border {
      border-color: var(--neon-purple);
      box-shadow: 0 0 30rpx rgba(168, 85, 247, 0.2);
    }
    
    .icon-wrap {
      background: var(--neon-purple);
      box-shadow: 0 0 20rpx rgba(168, 85, 247, 0.5);
    }
  }
  
  &.variant-success {
    background: linear-gradient(135deg, rgba(0, 230, 118, 0.1) 0%, var(--bg-tertiary) 100%);
    
    .icon-wrap {
      background: var(--state-success);
      box-shadow: 0 0 20rpx rgba(0, 230, 118, 0.4);
    }
  }
  
  &.variant-warning {
    background: linear-gradient(135deg, rgba(255, 171, 0, 0.1) 0%, var(--bg-tertiary) 100%);
    
    .icon-wrap {
      background: var(--state-warning);
      box-shadow: 0 0 20rpx rgba(255, 171, 0, 0.4);
    }
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
</style>
