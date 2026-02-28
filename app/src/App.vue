<script setup lang="ts">
import { onLaunch, onShow, onHide } from "@dcloudio/uni-app";
import store from "./utils/store";

// 应用启动
onLaunch(() => {
  console.log("[App] Launch");
  store.init();
});

// 应用显示（前台）
onShow(() => {
  console.log("[App] Show");
  store.setAppVisibility(true);
  store.setPollingProfile('normal');
});

// 应用隐藏（后台）
onHide(() => {
  console.log("[App] Hide");
  store.setAppVisibility(false);
});
</script>

<style lang="scss">
/* ==========================================
   科技未来感设计系统 - Cyberpunk Tech Theme
   ========================================== */

:root {
  /* 核心色调 - 深色科技主题 */
  --bg-primary: #0a0e1a;           /* 深邃太空蓝 */
  --bg-secondary: #111827;         /* 次级背景 */
  --bg-tertiary: #1a1f35;          /* 卡片背景 */
  --bg-elevated: #1e2538;          /* 悬浮层 */
  
  /* 霓虹主色 - 青色科技 */
  --neon-cyan: #00f0ff;            /* 主色-霓虹青 */
  --neon-cyan-dim: rgba(0, 240, 255, 0.15);
  --neon-cyan-glow: 0 0 20rpx rgba(0, 240, 255, 0.5);
  
  /* 霓虹辅色 - 紫色 */
  --neon-purple: #a855f7;          /* 辅色-霓虹紫 */
  --neon-purple-dim: rgba(168, 85, 247, 0.15);
  --neon-purple-glow: 0 0 20rpx rgba(168, 85, 247, 0.4);
  
  /* 状态色 */
  --state-success: #00e676;        /* 成功-霓虹绿 */
  --state-warning: #ffab00;        /* 警告-琥珀 */
  --state-danger: #ff5252;         /* 危险-霓虹红 */
  --state-offline: #64748b;        /* 离线-灰蓝 */
  
  /* 文字色 */
  --text-primary: #ffffff;
  --text-secondary: rgba(255, 255, 255, 0.7);
  --text-tertiary: rgba(255, 255, 255, 0.5);
  --text-muted: rgba(255, 255, 255, 0.35);
  
  /* 边框与分隔 */
  --border-subtle: rgba(255, 255, 255, 0.08);
  --border-glow: rgba(0, 240, 255, 0.3);
  
  /* 圆角 - 现代科技感 */
  --radius-sm: 8rpx;
  --radius-md: 16rpx;
  --radius-lg: 24rpx;
  --radius-xl: 32rpx;
  --radius-pill: 9999rpx;
  
  /* 阴影 - 深度层次 */
  --shadow-sm: 0 2rpx 8rpx rgba(0, 0, 0, 0.3);
  --shadow-md: 0 8rpx 24rpx rgba(0, 0, 0, 0.4);
  --shadow-lg: 0 16rpx 48rpx rgba(0, 0, 0, 0.5);
  --shadow-neon: 0 0 40rpx rgba(0, 240, 255, 0.2);
  
  /* 动效时间 */
  --duration-fast: 150ms;
  --duration-base: 300ms;
  --duration-slow: 500ms;
  --ease-out: cubic-bezier(0.16, 1, 0.3, 1);
  --ease-spring: cubic-bezier(0.34, 1.56, 0.64, 1);
  
  /* 字体 */
  --font-display: 'PingFang SC', 'SF Pro Display', -apple-system, sans-serif;
  --font-mono: 'SF Mono', 'Monaco', monospace;
}

/* 全局基础样式 */
page {
  background: var(--bg-primary);
  font-family: var(--font-display);
  color: var(--text-primary);
  font-size: 28rpx;
  line-height: 1.5;
  -webkit-font-smoothing: antialiased;
}

/* 安全区域底部 */
.safe-area-bottom {
  padding-bottom: constant(safe-area-inset-bottom);
  padding-bottom: env(safe-area-inset-bottom);
}

/* 加载状态 */
.loading-text {
  text-align: center;
  color: var(--text-tertiary);
  font-size: 26rpx;
  padding: 40rpx 0;
}

/* 空状态 */
.empty-state {
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 80rpx 0;
  
  .empty-text {
    font-size: 30rpx;
    color: var(--text-secondary);
    font-weight: 600;
    margin-bottom: 12rpx;
  }
  
  .empty-subtext {
    font-size: 26rpx;
    color: var(--text-tertiary);
  }
}

/* 全局动画 */
@keyframes fadeInUp {
  from {
    opacity: 0;
    transform: translateY(30rpx);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}

@keyframes pulse-glow {
  0%, 100% {
    box-shadow: 0 0 20rpx rgba(0, 240, 255, 0.4);
  }
  50% {
    box-shadow: 0 0 40rpx rgba(0, 240, 255, 0.7);
  }
}

@keyframes scan-line {
  0% {
    transform: translateY(-100%);
  }
  100% {
    transform: translateY(100vh);
  }
}

@keyframes float {
  0%, 100% {
    transform: translateY(0);
  }
  50% {
    transform: translateY(-10rpx);
  }
}

@keyframes gradient-shift {
  0% {
    background-position: 0% 50%;
  }
  50% {
    background-position: 100% 50%;
  }
  100% {
    background-position: 0% 50%;
  }
}
</style>
