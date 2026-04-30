// 受保护路由：未登录跳 /login；可选按权限/角色拦截（否则 403）。
import { Navigate, Outlet, useLocation } from 'react-router-dom'
import { Spin } from 'antd'
import { useAuth } from '../hooks/useAuth'

type Props = {
  perm?: string | string[]
  role?: string | string[]
}

export default function ProtectedRoute({ perm, role }: Props) {
  const { user, loading, hasPerm, hasRole } = useAuth()
  const location = useLocation()

  if (loading) {
    return (
      <div style={{ minHeight: '100vh', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
        <Spin size="large" />
      </div>
    )
  }

  if (!user) {
    return <Navigate to="/login" replace state={{ from: location.pathname }} />
  }
  if (perm && !hasPerm(perm)) return <Navigate to="/403" replace />
  if (role && !hasRole(role)) return <Navigate to="/403" replace />
  return <Outlet />
}
