// 统一的后端 API 客户端。所有请求都会带上 Authorization: Bearer <token>（若存在）。
// 响应约定：{ code: number, message: string, data: any }

export type ApiEnvelope<T = unknown> = {
  code: number
  message: string
  data: T
}

const TOKEN_KEY = 'c_web_token'
const USER_KEY = 'c_web_user'

export type User = {
  id: number
  name: string
  email: string
  created_at: number
}

export type LoginData = {
  token: string
  token_type: string
  expires_at: number
  user: User
}

export const auth = {
  getToken(): string | null {
    return localStorage.getItem(TOKEN_KEY)
  },
  getUser(): User | null {
    const s = localStorage.getItem(USER_KEY)
    if (!s) return null
    try { return JSON.parse(s) as User } catch { return null }
  },
  save(data: LoginData) {
    localStorage.setItem(TOKEN_KEY, data.token)
    localStorage.setItem(USER_KEY, JSON.stringify(data.user))
  },
  clear() {
    localStorage.removeItem(TOKEN_KEY)
    localStorage.removeItem(USER_KEY)
  },
}

async function request<T = unknown>(
  method: string,
  url: string,
  body?: unknown,
): Promise<ApiEnvelope<T>> {
  const headers: Record<string, string> = {
    'Content-Type': 'application/json',
  }
  const token = auth.getToken()
  if (token) headers['Authorization'] = `Bearer ${token}`

  const res = await fetch(url, {
    method,
    headers,
    body: body === undefined ? undefined : JSON.stringify(body),
  })

  let json: ApiEnvelope<T>
  try {
    json = await res.json()
  } catch {
    throw new Error(`HTTP ${res.status} ${res.statusText}`)
  }

  // 401 一律清理本地登录态
  if (res.status === 401) {
    auth.clear()
  }
  if (json.code !== 0) {
    throw new Error(json.message || `error code=${json.code}`)
  }
  return json
}

export const api = {
  health: () => request('GET', '/api/health'),

  register: (name: string, email: string, password: string) =>
    request<User>('POST', '/api/auth/register', { name, email, password }),

  login: (email: string, password: string) =>
    request<LoginData>('POST', '/api/auth/login', { email, password }),

  me: () => request<User>('GET', '/api/users/me'),
}
