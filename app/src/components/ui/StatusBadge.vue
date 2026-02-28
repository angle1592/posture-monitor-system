<template>
  <view class="status-badge-cyber" :class="online ? 'online' : 'offline'">
    <view class="pulse-ring" v-if="online"></view>
    <view class="dot"></view>
    <text class="label">{{ online ? onlineText : offlineText }}</text>
  </view>
</template>

<script setup lang="ts">
withDefaults(defineProps<{ online: boolean; onlineText?: string; offlineText?: string }>(), {
  onlineText: '在线',
  offlineText: '离线',
})
</script>

<style lang="scss" scoped>
.status-badge-cyber {
  display: inline-flex;
  align-items: center;
  gap: 12rpx;
  padding: 12rpx 24rpx;
  border-radius: var(--radius-pill);
  border: 1rpx solid transparent;
  transition: all var(--duration-base) var(--ease-out);
  position: relative;
  overflow: hidden;
  
  /* 脉冲环 */
  .pulse-ring {
    position: absolute;
    left: 24rpx;
    width: 12rpx;
    height: 12rpx;
    border-radius: 50%;
    background: var(--state-success);
    animation: pulse-ring 2s ease-out infinite;
  }
  
  .dot {
    width: 12rpx;
    height: 12rpx;
    border-radius: 50%;
    position: relative;
    z-index: 1;
    transition: all var(--duration-base);
  }
  
  .label {
    font-size: 24rpx;
    font-weight: 700;
    letter-spacing: 1rpx;
    position: relative;
    z-index: 1;
    text-transform: uppercase;
  }
  
  /* 在线状态 */
  &.online {
    background: linear-gradient(135deg, rgba(0, 230, 118, 0.15) 0%, rgba(0, 230, 118, 0.05) 100%);
    border-color: rgba(0, 230, 118, 0.4);
    box-shadow: 0 0 20rpx rgba(0, 230, 118, 0.2), inset 0 0 20rpx rgba(0, 230, 118, 0.05);
    
    .dot {
      background: var(--state-success);
      box-shadow: 0 0 12rpx var(--state-success), 0 0 24rpx rgba(0, 230, 118, 0.5);
      animation: dot-pulse 2s ease-in-out infinite;
    }
    
    .label {
      color: var(--state-success);
      text-shadow: 0 0 10rpx rgba(0, 230, 118, 0.5);
    }
  }
  
  /* 离线状态 */
  &.offline {
    background: linear-gradient(135deg, rgba(100, 116, 139, 0.2) 0%, rgba(100, 116, 139, 0.05) 100%);
    border-color: rgba(100, 116, 139, 0.4);
    
    .dot {
      background: var(--state-offline);
      box-shadow: none;
    }
    
    .label {
      color: var(--text-tertiary);
    }
  }
}

@keyframes pulse-ring {
  0% {
    transform: scale(1);
    opacity: 0.8;
  }
  100% {
    transform: scale(4);
    opacity: 0;
  }
}

@keyframes dot-pulse {
  0%, 100% {
    transform: scale(1);
    opacity: 1;
  }
  50% {
    transform: scale(1.2);
    opacity: 0.8;
  }
}
</style>
