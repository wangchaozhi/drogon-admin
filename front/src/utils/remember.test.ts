import { beforeEach, describe, expect, it } from 'vitest'
import { remember } from './remember'

const KEY = 'drogon_admin_remembered_credential'

describe('remember', () => {
  beforeEach(() => localStorage.clear())

  it('persists only the email address', () => {
    remember.save({ email: 'admin@example.com' })

    expect(remember.load()).toEqual({ email: 'admin@example.com' })
    const stored = localStorage.getItem(KEY)
    expect(stored).not.toContain('password')
    expect(stored).not.toContain('"p"')
  })

  it('reads a legacy entry without exposing or preserving its password', () => {
    localStorage.setItem(
      KEY,
      JSON.stringify({
        e: btoa('legacy@example.com'),
        p: btoa('Legacy-Password-2026!'),
      }),
    )

    expect(remember.load()).toEqual({ email: 'legacy@example.com' })
    expect(localStorage.getItem(KEY)).not.toContain('"p"')
  })

  it('ignores malformed storage', () => {
    localStorage.setItem(KEY, '{not-json')
    expect(remember.load()).toBeNull()
  })
})
