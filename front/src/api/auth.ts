// 认证相关 API + 本地 token 管理。
import { http, tokenStore } from './http'
import type { LoginData, Me, User } from './types'

export const authApi = {
  health: () => http.get('/api/health'),
  register: (name: string, email: string, password: string) =>
    http.post<User>('/api/auth/register', { name, email, password }),
  login: (email: string, password: string) =>
    http.post<LoginData>('/api/auth/login', { email, password }),
  me: () => http.get<Me>('/api/users/me'),
}

export const auth = {
  getToken: () => tokenStore.get(),
  saveToken: (t: string) => tokenStore.set(t),
  clear: () => tokenStore.clear(),
}
