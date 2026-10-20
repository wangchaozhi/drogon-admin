// 聚合导出。verbatimModuleSyntax 下必须显式区分 type 与 value。
export type {
  ApiEnvelope,
} from './http'
export { ApiError, http, request, tokenStore, setUnauthorizedHandler } from './http'

export type {
  User,
  Role,
  Menu,
  Permission,
  AuditLog,
  LoginData,
  Me,
  Page,
} from './types'

export { authApi, auth } from './auth'
export { userApi } from './user'
export { roleApi } from './role'
export type { RoleUpsertReq } from './role'
export { menuApi } from './menu'
export type { MenuUpsertReq } from './menu'
export { permissionApi } from './permission'
export { auditApi } from './audit'
export type { AuditLogPage } from './audit'

// 兼容旧代码的 api 聚合（旧代码使用 api.login/api.me 等）
import { authApi } from './auth'
export const api = authApi
