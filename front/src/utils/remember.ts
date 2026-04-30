// "记住密码"本地存储工具：勾选时保存邮箱+密码，下次自动填充。
// 注意：密码明文存 localStorage 仅为用户体验，存在 XSS 风险，生产环境建议改为仅记住邮箱。
// 这里用 base64 做最基本的肉眼不可读处理（非加密）。

const KEY = 'drogon_admin_remembered_credential'

export type RememberedCredential = {
  email: string
  password: string
}

function encode(s: string): string {
  try { return btoa(unescape(encodeURIComponent(s))) } catch { return s }
}
function decode(s: string): string {
  try { return decodeURIComponent(escape(atob(s))) } catch { return s }
}

export const remember = {
  save(cred: RememberedCredential) {
    localStorage.setItem(
      KEY,
      JSON.stringify({ e: encode(cred.email), p: encode(cred.password) }),
    )
  },
  load(): RememberedCredential | null {
    const raw = localStorage.getItem(KEY)
    if (!raw) return null
    try {
      const o = JSON.parse(raw) as { e?: string; p?: string }
      if (!o.e || !o.p) return null
      return { email: decode(o.e), password: decode(o.p) }
    } catch {
      return null
    }
  },
  clear() {
    localStorage.removeItem(KEY)
  },
}
