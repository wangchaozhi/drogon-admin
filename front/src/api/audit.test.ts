import { afterEach, describe, expect, it, vi } from 'vitest'
import { auditApi } from './audit'

describe('audit API', () => {
  afterEach(() => {
    vi.unstubAllGlobals()
  })

  it('sends pagination and an exact action filter', async () => {
    const fetchMock = vi.fn().mockResolvedValue(
      new Response(
        JSON.stringify({
          code: 0,
          message: 'ok',
          data: { items: [], total: 0, page: 2, page_size: 50 },
        }),
        { status: 200, headers: { 'Content-Type': 'application/json' } },
      ),
    )
    vi.stubGlobal('fetch', fetchMock)

    await auditApi.list(2, 50, 'role.assign_permissions')

    expect(fetchMock).toHaveBeenCalledWith(
      '/api/audit-logs?page=2&page_size=50&action=role.assign_permissions',
      expect.objectContaining({ method: 'GET' }),
    )
  })

  it('omits an empty action filter', async () => {
    const fetchMock = vi.fn().mockResolvedValue(
      new Response(
        JSON.stringify({
          code: 0,
          message: 'ok',
          data: { items: [], total: 0, page: 1, page_size: 20 },
        }),
        { status: 200, headers: { 'Content-Type': 'application/json' } },
      ),
    )
    vi.stubGlobal('fetch', fetchMock)

    await auditApi.list()

    expect(fetchMock).toHaveBeenCalledWith(
      '/api/audit-logs?page=1&page_size=20',
      expect.objectContaining({ method: 'GET' }),
    )
  })
})
