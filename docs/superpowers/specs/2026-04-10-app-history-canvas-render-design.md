# App 历史页 Canvas 曲线渲染设计

## 1. 背景

历史页当前数据层已经切换为：

- 7 天趋势点
- 当天时间桶点
- `no_person` 断线

但在当前目标端上，内联 SVG 曲线没有按预期显示，导致：

1. 7 天趋势只剩节点/日期卡片的视觉印象
2. 当天明细即使有统计，也不稳定显示曲线

## 2. 目标

1. 保留现有数据准备逻辑不变。
2. 将历史页曲线层从 `SVG` 替换为 `canvas`。
3. 同时覆盖：
   - 7 天趋势
   - 当天明细
4. 保留 `no_person` 断线语义。

## 3. 非目标

1. 不改健康评分口径。
2. 不改历史聚合逻辑。
3. 不引入第三方图表库。

## 4. 方案

### 4.1 渲染层替换

将历史页中的：

- `<svg class="curve-svg"> ... </svg>`

替换为：

- `<canvas canvas-id="weekTrendCanvas" />`
- `<canvas canvas-id="detailTrendCanvas" />`

### 4.2 数据层保持不变

继续复用：

- `weekChartPoints`
- `detailChartPoints`
- `splitChartSegments`

页面只需要将这些点集交给 `canvas` 绘制函数。

### 4.3 绘制内容

Canvas 需要绘制：

1. 背景参考线
2. 平滑折线/贝塞尔曲线
3. 节点圆点
4. `no_person` 导致的断线段

## 5. 影响范围

主要修改：

- `app/src/pages/history/index.vue`

必要时可抽一层本页内 helper，但不新增复杂组件。
