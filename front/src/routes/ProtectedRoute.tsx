// 受保护路由：未登录跳 /login；可选按权限/角色拦截（否则 403）。
import { Navigate, Outlet, useLocation } from 'react-router-dom'
import { Button, Result, Spin } from 'antd'
import { useAuth } from '../hooks/useAuth'

type Props = {
  perm?: string | string[]
  role?: string | string[]
}

export default function ProtectedRoute({ perm, role }: Props) {
  const { user, loading, error, refresh, hasPerm, hasRole } = useAuth()
  const location = useLocation()

  if (loading) {
    return (
      <div
        role="status"
        aria-label="正在恢复登录状态"
        style={{ minHeight: '100vh', display: 'flex', alignItems: 'center', justifyContent: 'center' }}
      >
        <Spin size="large" />
      </div>
    )
  }

  if (!user && error) {
    return (
      <Result
        status="warning"
        title="暂时无法恢复登录状态"
        subTitle={error}
        extra={
          <Button
            type="primary"
            aria-label="重试恢复登录状态"
            onClick={() => void refresh().catch(() => {})}
          >
            重试
          </Button>
        }
      />
    )
  }

  if (!user) {
    return <Navigate to="/login" replace state={{ from: location.pathname }} />
  }
  if (perm && !hasPerm(perm)) return <Navigate to="/403" replace />
  if (role && !hasRole(role)) return <Navigate to="/403" replace />
  return <Outlet />
}
