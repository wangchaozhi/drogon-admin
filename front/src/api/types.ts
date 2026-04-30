// 通用领域类型：与后端 RBAC DTO 对齐。
export type User = {
  id: number
  name: string
  email: string
  created_at: number
}

export type Role = {
  id: number
  code: string
  name: string
  description?: string
}

export type Menu = {
  id: number
  parent_id: number
  name: string         // 显示名
  path: string         // 前端路由 path（目录类可为空）
  icon: string         // antd 图标名，如 DashboardOutlined
  component: string    // 页面组件标识（可选）
  sort: number
  type: 'dir' | 'menu'
  visible: number      // 0 | 1
  children?: Menu[]
}

export type Permission = {
  id: number
  menu_id: number
  code: string       // 如 "user:view"
  name: string
  type: 'menu' | 'button'
}

export type LoginData = {
  token: string
  token_type: string
  expires_at: number
  user: User
  roles: string[]
  permissions: string[]
  menus: Menu[]
}

export type Me = {
  user: User
  roles: string[]
  permissions: string[]
  menus: Menu[]
}

export type Page<T> = {
  list: T[]
  total: number
  page: number
  page_size: number
}
