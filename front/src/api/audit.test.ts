import { afterEach, describe, expect, it, vi } from 'vitest'
import { auditApi } from './audit'

describe('audit API', () => {
  afterEach(() => {
    vi.unstubAllGlobals()
  })

  it('sends pagination and exact audit filters', async () => {
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

    await auditApi.list(2, 50, {
      action: 'role.assign_permissions',
      actor_user_id: 7,
      target_type: 'role',
      target_id: '3',
      request_id: 'request-123',
    })

    expect(fetchMock).toHaveBeenCalledWith(
      '/api/audit-logs?page=2&page_size=50&action=role.assign_permissions'
        + '&actor_user_id=7&target_type=role&target_id=3&request_id=request-123',
      expect.objectContaining({ method: 'GET' }),
    )
  })

  it('omits empty filters', async () => {
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
