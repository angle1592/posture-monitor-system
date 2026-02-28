/*
 * 模块职责：日期标准化工具。
 *
 * 与其他模块关系：
 * - store.ts 使用 formatLocalDate 生成本地统计键（yyyy-MM-dd），确保跨端/跨时区显示一致。
 * - history/control 等页面可复用该格式，避免各处手写日期拼接导致口径不一致。
 */

function pad2(value: number): string {
  return String(value).padStart(2, '0')
}

export function formatLocalDate(input: Date | number | string): string {
  // 接收 Date/时间戳/字符串三种输入，统一归一化为本地日期字符串。
  const date = input instanceof Date ? input : new Date(input)
  if (Number.isNaN(date.getTime())) {
    // 非法输入回退到当天，避免上层把 invalid date 写入统计键。
    const now = new Date()
    return `${now.getFullYear()}-${pad2(now.getMonth() + 1)}-${pad2(now.getDate())}`
  }

  return `${date.getFullYear()}-${pad2(date.getMonth() + 1)}-${pad2(date.getDate())}`
}
