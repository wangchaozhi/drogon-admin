// 菜单管理 API
import { http } from './http'
import type { Menu } from './types'

export type MenuUpsertReq = {
  parent_id?: number
  name: string
  path?: string
  icon?: string
  component?: string
  sort?: number
  type?: 'dir' | 'menu'
  visible?: number
}

export const menuApi = {
  tree: () => http.get<Menu[]>('/api/menus/tree'),
  create: (req: MenuUpsertReq) => http.post<Menu>('/api/menus', req),
  update: (id: number, req: MenuUpsertReq) => http.put<Menu>(`/api/menus/${id}`, req),
  remove: (id: number) => http.del(`/api/menus/${id}`),
}
