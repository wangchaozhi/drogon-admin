// 统一的 HTTP 客户端：自动注入 Bearer token；约定 { code, message, data } 响应。
// 401 清除本地登录态；非 0 码抛 ApiError 便于上层识别特殊码（如 403）。

export type ApiEnvelope<T = unknown> = {
  code: number
  message: string
  data: T
}

export class ApiError extends Error {
  code: number
  status: number
  constructor(code: number, status: number, message: string) {
    super(message)
    this.code = code
    this.status = status
  }
}

const TOKEN_KEY = 'drogon_admin_token'

export const tokenStore = {
  get(): string | null {
    return localStorage.getItem(TOKEN_KEY)
  },
  set(t: string) {
    localStorage.setItem(TOKEN_KEY, t)
  },
  clear() {
    localStorage.removeItem(TOKEN_KEY)
  },
}

// 401 钩子：供 AuthContext 订阅，便于全局登出。
type UnauthorizedHandler = () => void
let onUnauthorized: UnauthorizedHandler | null = null
export function setUnauthorizedHandler(fn: UnauthorizedHandler | null) {
  onUnauthorized = fn
}

export async function request<T = unknown>(
  method: string,
  url: string,
  body?: unknown,
): Promise<ApiEnvelope<T>> {
  const headers: Record<string, string> = {
    'Content-Type': 'application/json',
  }
  const token = tokenStore.get()
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
    throw new ApiError(-1, res.status, `HTTP ${res.status} ${res.statusText}`)
  }

  if (res.status === 401) {
    tokenStore.clear()
    onUnauthorized?.()
  }
  if (json.code !== 0) {
    throw new ApiError(json.code, res.status, json.message || `error code=${json.code}`)
  }
  return json
}

export const http = {
  get: <T = unknown>(url: string) => request<T>('GET', url),
  post: <T = unknown>(url: string, body?: unknown) => request<T>('POST', url, body),
  put: <T = unknown>(url: string, body?: unknown) => request<T>('PUT', url, body),
  del: <T = unknown>(url: string) => request<T>('DELETE', url),
}
