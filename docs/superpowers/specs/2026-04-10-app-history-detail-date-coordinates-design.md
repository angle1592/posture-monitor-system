# App 当天明细曲线日期坐标修复设计

## 1. 背景

历史页当前已经能正确计算当天统计摘要，但在部分运行环境中出现：

- `良好时长` 和 `异常占比` 有值
- `当天明细贝塞尔曲线` 不显示

这说明问题更可能出在“曲线坐标生成”而不是“统计数据缺失”。

## 2. 根因判断

当前历史页使用：

```ts
new Date(`${selectedDetailDate.value}T00:00:00`).getTime()
```

来计算当天起点时间。

这种字符串解析在不同运行环境中并不稳定，可能导致：

1. `dayStart` 解析异常
2. 时间桶到 X 坐标的换算结果失真或无效
3. SVG 曲线路径无法正常绘制

而统计摘要仍可正常工作，因为它只基于历史点计数，不依赖这条坐标换算链。

## 3. 目标

1. 使用稳定的本地日期解析方式计算当天起点毫秒值。
2. 将“时间桶 -> 图表点坐标”的逻辑从页面中抽成可测试纯函数。
3. 保持当前历史页交互和视觉结构不变，只修复数据坐标生成。

## 4. 方案

### 4.1 日期工具层

在 `date.ts` 中新增一个稳定工具，例如：

- `parseLocalDateStartMs('2026-04-10')`

它不依赖 `new Date('yyyy-MM-ddT00:00:00')` 的字符串解析，而是手动拆分年/月/日后通过本地构造：

```ts
new Date(year, month - 1, day, 0, 0, 0, 0).getTime()
```

### 4.2 historyChart 工具层

在 `historyChart.ts` 中新增纯函数，将：

- 选中日期
- 时间桶数据
- 画布尺寸
- Y 轴映射函数

转换为可绘制的 `ChartDisplayPoint[]`。

这样页面层不再自己拼 `dayStart` 并换算坐标。

## 5. 影响范围

主要修改：

- `app/src/utils/date.ts`
- `app/src/utils/date.test.ts`
- `app/src/utils/historyChart.ts`
- `app/src/utils/historyChart.test.ts`
- `app/src/pages/history/index.vue`

## 6. 非目标

1. 不改 7 天趋势曲线逻辑。
2. 不改 mock 历史数据生成逻辑。
3. 不改当天统计摘要口径。
