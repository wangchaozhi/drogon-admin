// 角色管理 API
import { http } from './http'
import type { Role } from './types'

export type RoleUpsertReq = {
  code: string
  name: string
  description?: string
}

type PermIds = { permission_ids: number[] }

export const roleApi = {
  list: () => http.get<Role[]>('/api/roles'),
  create: (req: RoleUpsertReq) => http.post<Role>('/api/roles', req),
  update: (id: number, req: RoleUpsertReq) => http.put<Role>(`/api/roles/${id}`, req),
  remove: (id: number) => http.del(`/api/roles/${id}`),
  getPermissions: (id: number) =>
    http.get<PermIds>(`/api/roles/${id}/permissions`).then((r) => ({ ...r, data: r.data.permission_ids })),
  setPermissions: (id: number, permission_ids: number[]) =>
    http.put(`/api/roles/${id}/permissions`, { permission_ids }),
}
