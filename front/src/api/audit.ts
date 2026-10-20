import { http } from './http'
import type { AuditLog } from './types'

export type AuditLogPage = {
  items: AuditLog[]
  total: number
  page: number
  page_size: number
}

export type AuditLogFilters = {
  action?: string
  actor_user_id?: number
  target_type?: string
  target_id?: string
  request_id?: string
}

export const auditApi = {
  list: (page = 1, pageSize = 20, filters: AuditLogFilters = {}) => {
    const query = new URLSearchParams({
      page: String(page),
      page_size: String(pageSize),
    })
    if (filters.action) query.set('action', filters.action)
    if (filters.actor_user_id) {
      query.set('actor_user_id', String(filters.actor_user_id))
    }
    if (filters.target_type) query.set('target_type', filters.target_type)
    if (filters.target_id) query.set('target_id', filters.target_id)
    if (filters.request_id) query.set('request_id', filters.request_id)
    return http.get<AuditLogPage>(`/api/audit-logs?${query.toString()}`)
  },
}
