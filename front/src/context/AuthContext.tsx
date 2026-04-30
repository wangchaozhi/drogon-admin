// AuthContext：把登录态提升到 Provider，避免多个组件各自持有独立 state。
import { createContext, useCallback, useEffect, useState, type ReactNode } from 'react'
import { api, auth, type LoginData, type User } from '../api'

export type AuthValue = {
  user: User | null
  loading: boolean
  login: (email: string, password: string) => Promise<LoginData>
  logout: () => void
  refresh: () => Promise<void>
}

// eslint-disable-next-line react-refresh/only-export-components
export const AuthContext = createContext<AuthValue | null>(null)

export function AuthProvider({ children }: { children: ReactNode }) {
  const [user, setUser] = useState<User | null>(() => auth.getUser())
  const [loading, setLoading] = useState<boolean>(!!auth.getToken())

  // 首次挂载：如果本地有 token，校验并拉取最新 user
  useEffect(() => {
    if (!auth.getToken()) return
    api.me()
      .then((r) => { if (r.data) setUser(r.data) })
      .catch(() => { auth.clear(); setUser(null) })
      .finally(() => setLoading(false))
  }, [])

  const login = useCallback(async (email: string, password: string) => {
    const res = await api.login(email, password)
    if (res.data) {
      auth.save(res.data)
      setUser(res.data.user)
    }
    return res.data as LoginData
  }, [])

  const logout = useCallback(() => {
    auth.clear()
    setUser(null)
  }, [])

  const refresh = useCallback(async () => {
    try {
      const r = await api.me()
      if (r.data) setUser(r.data)
    } catch {
      auth.clear()
      setUser(null)
    }
  }, [])

  return (
    <AuthContext.Provider value={{ user, loading, login, logout, refresh }}>
      {children}
    </AuthContext.Provider>
  )
}
