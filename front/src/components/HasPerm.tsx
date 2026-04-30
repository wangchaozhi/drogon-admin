// 按钮级权限守卫：无权限时默认不渲染，可传 fallback。
import type { ReactNode } from 'react'
import { useAuth } from '../hooks/useAuth'

type Props = {
  code: string | string[]
  fallback?: ReactNode
  children: ReactNode
}

export default function HasPerm({ code, fallback = null, children }: Props) {
  const { hasPerm } = useAuth()
  return <>{hasPerm(code) ? children : fallback}</>
}
