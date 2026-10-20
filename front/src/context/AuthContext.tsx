// AuthContext：把登录态 + RBAC 视图提升到 Provider。
// login/me 响应都包含 user/roles/permissions/menus，Provider 缓存并暴露 hasPerm/hasRole。
import { createContext, useCallback, useEffect, useMemo, useState, type ReactNode } from 'react'
import { authApi, auth } from '../api/auth'
import { ApiError, setUnauthorizedHandler } from '../api/http'
import type { LoginData, Menu, User } from '../api/types'

export type AuthValue = {
  user: User | null
  roles: string[]
  permissions: string[]
  menus: Menu[]
  loading: boolean
  error: string | null
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
  const [error, setError] = useState<string | null>(null)

  const clearAll = useCallback(() => {
    auth.clear()
    setUser(null)
    setRoles([])
    setPermissions([])
    setMenus([])
    setError(null)
  }, [])

  const applySession = useCallback((data: {
    user: User
    roles?: string[]
    permissions?: string[]
    menus?: Menu[]
  }) => {
    setUser(data.user)
    setRoles(data.roles ?? [])
    setPermissions(data.permissions ?? [])
    setMenus(data.menus ?? [])
    setError(null)
  }, [])

  const fetchCurrentUser = useCallback(async () => {
    const response = await authApi.me()
    if (response.data) applySession(response.data)
  }, [applySession])

  // 全局 401 兜底
  useEffect(() => {
    setUnauthorizedHandler(() => clearAll())
    return () => setUnauthorizedHandler(null)
  }, [clearAll])

  // 首次挂载：若有 token 则 /me 拉取 RBAC 视图
  useEffect(() => {
    if (!auth.getToken()) return
    // Initial remote session restoration intentionally updates provider state.
    // eslint-disable-next-line react-hooks/set-state-in-effect
    fetchCurrentUser()
      .catch((requestError: unknown) => {
        if (requestError instanceof ApiError && requestError.status === 401) {
          clearAll()
        } else {
          setError(
            requestError instanceof Error
              ? requestError.message
              : '无法恢复登录状态',
          )
        }
      })
      .finally(() => setLoading(false))
  }, [clearAll, fetchCurrentUser])

  const login = useCallback(async (email: string, password: string) => {
    const res = await authApi.login(email, password)
    const d = res.data
    if (d) {
      auth.saveToken(d.token)
      applySession(d)
    }
    return d as LoginData
  }, [applySession])

  const logout = useCallback(() => {
    clearAll()
  }, [clearAll])

  const refresh = useCallback(async () => {
    if (!auth.getToken()) {
      clearAll()
      return
    }
    if (!user) setLoading(true)
    setError(null)
    try {
      await fetchCurrentUser()
    } catch (requestError) {
      if (requestError instanceof ApiError && requestError.status === 401) {
        clearAll()
        return
      }
      setError(
        requestError instanceof Error
          ? requestError.message
          : '刷新登录状态失败',
      )
      throw requestError
    } finally {
      setLoading(false)
    }
  }, [clearAll, fetchCurrentUser, user])

  // 窗口重新获得焦点时刷新角色/权限，避免管理员调整后前端长期使用旧视图。
  useEffect(() => {
    if (!user) return
    const onFocus = () => {
      void refresh().catch(() => {})
    }
    window.addEventListener('focus', onFocus)
    return () => window.removeEventListener('focus', onFocus)
  }, [refresh, user])

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
      value={{
        user,
        roles,
        permissions,
        menus,
        loading,
        error,
        login,
        logout,
        refresh,
        hasPerm,
        hasRole,
      }}
    >
      {children}
    </AuthContext.Provider>
  )
}
