// 根据登录态切换页面；启动期显示全屏 Spin。
import { Spin } from 'antd'
import { useAuth } from '../hooks/useAuth'
import AuthPage from './auth/AuthPage'
import HomePage from './home/HomePage'

export default function AppRoutes() {
  const { user, loading } = useAuth()

  if (loading) {
    return (
      <div
        style={{
          minHeight: '100vh',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
        }}
      >
        <Spin size="large" />
      </div>
    )
  }

  return user ? <HomePage /> : <AuthPage />
}
