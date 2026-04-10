# App 全局 StatCard 标题优先布局设计

## 1. 背景

当前 App 多个页面的卡片都通过 `app/src/components/ui/StatCard.vue` 渲染。

该组件当前布局为：

1. 上方大号 `value`
2. 下方小号 `label`

在英语仪表盘场景里这种布局勉强成立，但在当前中文信息卡中会产生明显阅读违和，例如：

- `无人`
- `当前姿势`

用户会感受到“值在前、标题在后”，像是语序颠倒。

## 2. 目标

将全局 `StatCard` 统一改为：

1. 第一行：`icon + label`
2. 第二行：大号 `value`

从而让所有信息卡都更符合中文阅读顺序。

## 3. 非目标

1. 不新增 `StatCard` 变体组件。
2. 不修改页面对 `StatCard` 的调用签名。
3. 不重做整体配色、霓虹边框和变体体系。

## 4. 方案选择

本次采用：

- **单组件全局重排**

即仅修改 `StatCard.vue`，让所有页面自然获得新的布局，而不是在页面层单独对姿态卡做特殊处理。

## 5. 布局定义

新的卡片结构应为：

```text
[icon] [label]
[large value]
```

示例：

```text
👤 当前姿势
无人
```

```text
⚠️ 异常次数
12
```

## 6. 实现约束

1. 图标放入标题行，而不是独立占一整块垂直空间。
2. `valueClass` 的颜色语义继续保留。
3. 各种 `variant-*` 背景和 glow 继续生效。
4. 改动尽量只收敛在 `StatCard.vue`，避免页面样式连锁修改。

## 7. 影响范围

将同步影响所有使用 `StatCard` 的页面，包括但不限于：

- 首页 `pages/index/index.vue`
- 实时监控页 `pages/monitor/index.vue`
- 历史页 `pages/history/index.vue`

这是预期行为，不视为回归。
