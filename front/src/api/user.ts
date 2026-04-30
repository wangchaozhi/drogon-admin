// 用户管理 API
import { http } from './http'
import type { User } from './types'

// 后端返回 { items, total, page, page_size }
export type UserPage = {
  items: User[]
  total: number
  page: number
  page_size: number
}

export type UserRoleIds = { role_ids: number[] }

export type UserCreateReq = {
  name: string
  email: string
  password: string
}

export type UserUpdateReq = {
  name: string
  email: string
  password?: string  // 留空表示不修改密码
}

export const userApi = {
  list: (page = 1, pageSize = 10, keyword = '') =>
    http.get<UserPage>(
      `/api/users?page=${page}&page_size=${pageSize}&keyword=${encodeURIComponent(keyword)}`,
    ),
  create: (req: UserCreateReq) => http.post<User>('/api/users', req),
  update: (id: number, req: UserUpdateReq) => http.put<User>(`/api/users/${id}`, req),
  remove: (id: number) => http.del(`/api/users/${id}`),
  getRoles: (id: number) =>
    http.get<UserRoleIds>(`/api/users/${id}/roles`).then((r) => ({ ...r, data: r.data.role_ids })),
  setRoles: (id: number, role_ids: number[]) =>
    http.put(`/api/users/${id}/roles`, { role_ids }),
}
