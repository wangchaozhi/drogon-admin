import { http } from './http'
import type { AuditLog } from './types'

export type AuditLogPage = {
  items: AuditLog[]
  total: number
  page: number
  page_size: number
}

export const auditApi = {
  list: (page = 1, pageSize = 20, action = '') => {
    const query = new URLSearchParams({
      page: String(page),
      page_size: String(pageSize),
    })
    if (action) query.set('action', action)
    return http.get<AuditLogPage>(`/api/audit-logs?${query.toString()}`)
  },
}
