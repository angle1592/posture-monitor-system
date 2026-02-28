import { describe, expect, it } from 'vitest'

import { formatLocalDate } from './date'

describe('formatLocalDate', () => {
  it('formats date values to YYYY-MM-DD', () => {
    const result = formatLocalDate(new Date('2026-02-26T13:00:00+08:00'))
    expect(result).toBe('2026-02-26')
  })

  it('falls back to today for invalid input', () => {
    const result = formatLocalDate('invalid-date-input')
    expect(result).toMatch(/^\d{4}-\d{2}-\d{2}$/)
  })
})
