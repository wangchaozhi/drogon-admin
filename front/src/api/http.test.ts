import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import {
  request,
  setUnauthorizedHandler,
  tokenStore,
} from './http'

describe('HTTP client', () => {
  beforeEach(() => {
    localStorage.clear()
  })

  afterEach(() => {
    setUnauthorizedHandler(null)
    vi.unstubAllGlobals()
  })

  it('injects the bearer token and returns a successful envelope', async () => {
    tokenStore.set('signed-token')
    const fetchMock = vi.fn().mockResolvedValue(
      new Response(
        JSON.stringify({ code: 0, message: 'ok', data: { id: 1 } }),
        { status: 200, headers: { 'Content-Type': 'application/json' } },
      ),
    )
    vi.stubGlobal('fetch', fetchMock)

    const response = await request<{ id: number }>('GET', '/api/users/me')

    expect(response.data.id).toBe(1)
    const init = fetchMock.mock.calls[0][1] as RequestInit
    expect((init.headers as Record<string, string>).Authorization).toBe(
      'Bearer signed-token',
    )
  })

  it('clears local auth state and invokes the global handler on 401', async () => {
    tokenStore.set('revoked-token')
    const unauthorized = vi.fn()
    setUnauthorizedHandler(unauthorized)
    vi.stubGlobal(
      'fetch',
      vi.fn().mockResolvedValue(
        new Response(
          JSON.stringify({ code: 401, message: 'token revoked', data: null }),
          { status: 401, headers: { 'Content-Type': 'application/json' } },
        ),
      ),
    )

    await expect(request('GET', '/api/users/me')).rejects.toEqual(
      expect.objectContaining({
        code: 401,
        status: 401,
        message: 'token revoked',
      }),
    )
    expect(tokenStore.get()).toBeNull()
    expect(unauthorized).toHaveBeenCalledOnce()
  })

  it('wraps a non-JSON response as an ApiError', async () => {
    vi.stubGlobal(
      'fetch',
      vi.fn().mockResolvedValue(
        new Response('upstream unavailable', { status: 502 }),
      ),
    )

    await expect(request('GET', '/api/health')).rejects.toMatchObject({
      code: -1,
      status: 502,
    })
  })
})
