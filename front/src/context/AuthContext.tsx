// AuthContext：把登录态 + RBAC 视图提升到 Provider。
// login/me 响应都包含 user/roles/permissions/menus，Provider 缓存并暴露 hasPerm/hasRole。
import { createContext, useCallback, useEffect, useMemo, useState, type ReactNode } from 'react'
import { authApi, auth } from '../api/auth'
import { setUnauthorizedHandler } from '../api/http'
import type { LoginData, Menu, User } from '../api/types'

export type AuthValue = {
  user: User | null
  roles: string[]
  permissions: string[]
  menus: Menu[]
  loading: boolean
  login: (email: string, password: string) => Promise<LoginData>
  logout: () => void
  refresh: () => Promise<void>
  hasPerm: (code: string | string[]) => boolean
  hasRole: (code: string | string[]) => boolean
}

// eslint-disable-next-line react-refresh/only-export-components
export const AuthContext = createContext<AuthValue | null>(null)

export function AuthProvider({ children }: { children: ReactNode }) {
  const [user, setUser] = useState<User | null>(null)
  const [roles, setRoles] = useState<string[]>([])
  const [permissions, setPermissions] = useState<string[]>([])
  const [menus, setMenus] = useState<Menu[]>([])
  const [loading, setLoading] = useState<boolean>(!!auth.getToken())

  const clearAll = useCallback(() => {
    auth.clear()
    setUser(null)
    setRoles([])
    setPermissions([])
    setMenus([])
  }, [])

  // 全局 401 兜底
  useEffect(() => {
    setUnauthorizedHandler(() => clearAll())
    return () => setUnauthorizedHandler(null)
  }, [clearAll])

  // 首次挂载：若有 token 则 /me 拉取 RBAC 视图
  useEffect(() => {
    if (!auth.getToken()) return
    authApi.me()
      .then((r) => {
        if (r.data) {
          setUser(r.data.user)
          setRoles(r.data.roles ?? [])
          setPermissions(r.data.permissions ?? [])
          setMenus(r.data.menus ?? [])
        }
      })
      .catch(() => clearAll())
      .finally(() => setLoading(false))
  }, [clearAll])

  const login = useCallback(async (email: string, password: string) => {
    const res = await authApi.login(email, password)
    const d = res.data
    if (d) {
      auth.saveToken(d.token)
      setUser(d.user)
      setRoles(d.roles ?? [])
      setPermissions(d.permissions ?? [])
      setMenus(d.menus ?? [])
    }
    return d as LoginData
  }, [])

  const logout = useCallback(() => {
    clearAll()
  }, [clearAll])

  const refresh = useCallback(async () => {
    try {
      const r = await authApi.me()
      if (r.data) {
        setUser(r.data.user)
        setRoles(r.data.roles ?? [])
        setPermissions(r.data.permissions ?? [])
        setMenus(r.data.menus ?? [])
      }
    } catch {
      clearAll()
    }
  }, [clearAll])

  const permSet = useMemo(() => new Set(permissions), [permissions])
  const roleSet = useMemo(() => new Set(roles), [roles])

  const hasPerm = useCallback(
    (code: string | string[]) => {
      if (roleSet.has('admin')) return true
      const codes = Array.isArray(code) ? code : [code]
      return codes.some((c) => permSet.has(c))
    },
    [permSet, roleSet],
  )

  const hasRole = useCallback(
    (code: string | string[]) => {
      const codes = Array.isArray(code) ? code : [code]
      return codes.some((c) => roleSet.has(c))
    },
    [roleSet],
  )

  return (
    <AuthContext.Provider
      value={{ user, roles, permissions, menus, loading, login, logout, refresh, hasPerm, hasRole }}
    >
      {children}
    </AuthContext.Provider>
  )
}
