function pad2(value: number): string {
  return String(value).padStart(2, '0')
}

export function formatLocalDate(input: Date | number | string): string {
  const date = input instanceof Date ? input : new Date(input)
  if (Number.isNaN(date.getTime())) {
    // 非法输入回退到当天，避免上层把 invalid date 写入统计键。
    const now = new Date()
    return `${now.getFullYear()}-${pad2(now.getMonth() + 1)}-${pad2(now.getDate())}`
  }

  return `${date.getFullYear()}-${pad2(date.getMonth() + 1)}-${pad2(date.getDate())}`
}
