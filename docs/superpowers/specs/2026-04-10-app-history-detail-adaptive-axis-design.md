# App 当天明细自适应时间轴设计

## 1. 背景

当前当天明细曲线虽然已经能显示，但 X 轴仍固定写死为：

- `00:00`
- `12:00`
- `24:00`

当当天只有一小段数据时，所有点都会被压到左侧，导致：

1. 曲线细节看不清
2. `5/10/15 分钟` 聚合差异不明显

## 2. 目标

1. 当天明细 X 轴按“实际有数据的时间范围”自适应。
2. 底部时间标签同步显示该范围的：
   - 起点
   - 中点
   - 终点
3. 保留现有 7 天趋势图逻辑不变。

## 3. 方案

### 3.1 数据范围提取

对当天明细的已追踪桶点（`score !== null`）求：

- `minBucketStart`
- `maxBucketStart`

若只有一个点，则在其左右增加最小窗口宽度，避免全部堆在一条竖线上。

### 3.2 坐标映射

当天明细点的 X 坐标改为：

```text
x = plotLeft + ((bucketStart - axisStart) / (axisEnd - axisStart)) * plotWidth
```

### 3.3 时间标签

底部刻度不再固定写死，而是根据：

- `axisStart`
- `(axisStart + axisEnd) / 2`
- `axisEnd`

格式化出 3 个时间标签。

## 4. 影响范围

主要修改：

- `app/src/utils/historyChart.ts`
- `app/src/utils/historyChart.test.ts`
- `app/src/pages/history/index.vue`
