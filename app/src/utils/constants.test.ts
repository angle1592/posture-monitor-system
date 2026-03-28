import { describe, expect, it } from 'vitest'

import { PROP_IDS as appPropIds } from './constants'
import { PROP_IDS as sharedPropIds } from '../../../shared/protocol/constants'

describe('protocol constants sync', () => {
  it('keeps app property identifiers aligned with shared protocol', () => {
    expect(appPropIds).toEqual(sharedPropIds)
  })
})
