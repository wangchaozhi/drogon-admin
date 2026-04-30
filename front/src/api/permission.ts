// 权限管理 API（只读）
import { http } from './http'
import type { Permission } from './types'

export const permissionApi = {
  list: () => http.get<Permission[]>('/api/permissions'),
}
