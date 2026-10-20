// “记住我”仅保存邮箱。密码和 token 之外的凭据绝不写入持久存储。
// load 兼容旧 {e,p} 数据，但只读取邮箱；下一次 save 会移除旧密码字段。

const KEY = 'drogon_admin_remembered_credential'

export type RememberedCredential = {
  email: string
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
      JSON.stringify({ e: encode(cred.email) }),
    )
  },
  load(): RememberedCredential | null {
    const raw = localStorage.getItem(KEY)
    if (!raw) return null
    try {
      const o = JSON.parse(raw) as { e?: string; p?: string }
      if (!o.e) return null
      if (o.p) {
        // 发现旧版本保存的密码时立即原地迁移并删除该字段。
        localStorage.setItem(KEY, JSON.stringify({ e: o.e }))
      }
      return { email: decode(o.e) }
    } catch {
      return null
    }
  },
  clear() {
    localStorage.removeItem(KEY)
  },
}
