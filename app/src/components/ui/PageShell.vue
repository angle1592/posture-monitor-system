<template>
  <view class="page-shell" :class="toneClass">
    <view class="cyber-grid"></view>
    <view class="scan-line"></view>
    <view class="content">
      <slot />
    </view>
  </view>
</template>

<script setup lang="ts">
import { computed } from 'vue'

const props = withDefaults(defineProps<{ tone?: 'teal' | 'mint' | 'cyber' }>(), {
  tone: 'cyber',
})

const toneClass = computed(() => `tone-${props.tone}`)
</script>

<style lang="scss" scoped>
.page-shell {
  min-height: 100vh;
  position: relative;
  overflow: hidden;
  background: var(--bg-primary);
  
  /* 科技网格背景 */
  .cyber-grid {
    position: fixed;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    pointer-events: none;
    opacity: 0.03;
    background-image: 
      linear-gradient(rgba(0, 240, 255, 0.5) 1rpx, transparent 1rpx),
      linear-gradient(90deg, rgba(0, 240, 255, 0.5) 1rpx, transparent 1rpx);
    background-size: 60rpx 60rpx;
    z-index: 0;
  }
  
  /* 扫描线效果 */
  .scan-line {
    position: fixed;
    top: 0;
    left: 0;
    right: 0;
    height: 4rpx;
    background: linear-gradient(90deg, transparent, var(--neon-cyan), transparent);
    opacity: 0.3;
    animation: scan-line 8s linear infinite;
    pointer-events: none;
    z-index: 1;
  }
  
  .content {
    position: relative;
    z-index: 2;
  }
  
  /* 主题变体 - 赛博朋克 */
  &.tone-cyber {
    background: linear-gradient(180deg, var(--bg-primary) 0%, #0d1321 100%);
  }
  
  /* 主题变体 - 青色 */
  &.tone-teal {
    background: linear-gradient(180deg, var(--bg-primary) 0%, #0a1628 50%, #0d1f2d 100%);
  }
  
  /* 主题变体 - 薄荷 */
  &.tone-mint {
    background: linear-gradient(180deg, var(--bg-primary) 0%, #0a1a1a 100%);
  }
}

@keyframes scan-line {
  0% {
    transform: translateY(-100vh);
  }
  100% {
    transform: translateY(100vh);
  }
}
</style>
