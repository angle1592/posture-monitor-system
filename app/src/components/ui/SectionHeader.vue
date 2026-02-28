<template>
  <view class="section-header-cyber" :class="{ compact }">
    <view class="title-group">
      <text class="title">{{ title }}</text>
      <text v-if="subtitle" class="subtitle">{{ subtitle }}</text>
    </view>
    <view class="right-slot">
      <slot name="right" />
    </view>
    <view class="accent-line"></view>
  </view>
</template>

<script setup lang="ts">
withDefaults(defineProps<{ title: string; subtitle?: string; compact?: boolean }>(), {
  subtitle: '',
  compact: false,
})
</script>

<style lang="scss" scoped>
.section-header-cyber {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  gap: 20rpx;
  position: relative;
  padding-bottom: 20rpx;
  margin-bottom: 20rpx;
  
  .title-group {
    display: flex;
    flex-direction: column;
    gap: 8rpx;
    min-width: 0;
  }
  
  .title {
    color: var(--text-primary);
    font-size: 44rpx;
    line-height: 1.2;
    font-weight: 800;
    letter-spacing: -0.5rpx;
    background: linear-gradient(135deg, #fff 0%, var(--neon-cyan) 100%);
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    background-clip: text;
  }
  
  .subtitle {
    color: var(--text-tertiary);
    font-size: 26rpx;
    line-height: 1.4;
    font-weight: 400;
  }
  
  .right-slot {
    display: flex;
    align-items: center;
    justify-content: flex-end;
    flex-shrink: 0;
  }
  
  /* 装饰线 */
  .accent-line {
    position: absolute;
    bottom: 0;
    left: 0;
    width: 80rpx;
    height: 4rpx;
    background: linear-gradient(90deg, var(--neon-cyan) 0%, transparent 100%);
    border-radius: 2rpx;
    box-shadow: 0 0 10rpx var(--neon-cyan);
  }
  
  &.compact {
    .title {
      font-size: 36rpx;
      font-weight: 700;
    }
    
    .accent-line {
      width: 60rpx;
      height: 3rpx;
    }
  }
}
</style>
