// 登录/注册页：半屏渐变 hero + 右侧卡片。
// 登录成功后根据 location.state.from 回跳，默认去 /dashboard。
import { useState } from 'react'
import { App as AntApp, Card, Divider, Typography } from 'antd'
import { Navigate, useLocation, useNavigate } from 'react-router-dom'
import LoginForm, { type LoginValues } from './LoginForm'
import RegisterForm, { type RegisterValues } from './RegisterForm'
import { authApi } from '../../api/auth'
import { useAuth } from '../../hooks/useAuth'

const { Title, Paragraph, Link, Text } = Typography

type Mode = 'login' | 'register'

export default function LoginPage() {
  const { user, login } = useAuth()
  const { message } = AntApp.useApp()
  const navigate = useNavigate()
  const location = useLocation()

  const [mode, setMode] = useState<Mode>('login')
  const [rememberedEmail, setRememberedEmail] = useState('')

  if (user) {
    const from = (location.state as { from?: string } | null)?.from
    return <Navigate to={from || '/dashboard'} replace />
  }

  async function handleLogin(v: LoginValues) {
    try {
      await login(v.email, v.password)
      message.success('登录成功')
      const from = (location.state as { from?: string } | null)?.from
      navigate(from || '/dashboard', { replace: true })
    } catch (e) {
      message.error(e instanceof Error ? e.message : String(e))
      throw e
    }
  }
  async function handleRegister(v: RegisterValues) {
    try {
      await authApi.register(v.name, v.email, v.password)
      message.success('注册成功，请登录')
      setRememberedEmail(v.email)
      setMode('login')
    } catch (e) {
      message.error(e instanceof Error ? e.message : String(e))
    }
  }

  return (
    <div className="login-page">
      <div className="login-hero">
        <div className="login-hero-inner">
          <div className="login-logo">
            <span className="login-logo-dot" />
            <span className="login-logo-text">drogon-admin</span>
          </div>
          <Title level={1} className="login-hero-title">
            精致、现代、高效的
            <br />后台管理平台
          </Title>
          <Paragraph className="login-hero-sub">
            内建 RBAC 权限体系：角色、菜单、按钮级权限一站式管理。
          </Paragraph>
          <div className="login-hero-tags">
            <span>🚀 快速</span>
            <span>🛡️ 安全</span>
            <span>🎨 精美</span>
          </div>
        </div>
      </div>
      <div className="login-card-wrap">
        <Card className="login-card" styles={{ body: { padding: 28 } }}>
          <div style={{ textAlign: 'center', marginBottom: 20 }}>
            <Title level={3} style={{ margin: 0 }}>
              {mode === 'login' ? '欢迎回来' : '创建新账号'}
            </Title>
            <Paragraph type="secondary" style={{ margin: '4px 0 0' }}>
              {mode === 'login' ? '请登录你的账户' : '请填写信息完成注册'}
            </Paragraph>
          </div>
          {mode === 'login' ? (
            <LoginForm onSubmit={handleLogin} initialEmail={rememberedEmail} />
          ) : (
            <RegisterForm onSubmit={handleRegister} />
          )}
          <Divider plain style={{ margin: '16px 0 12px' }} />
          <div style={{ textAlign: 'center' }}>
            {mode === 'login' ? (
              <Text type="secondary">
                还没有账号？{' '}
                <Link onClick={(e) => { e.preventDefault(); setMode('register') }}>立即注册</Link>
              </Text>
            ) : (
              <Text type="secondary">
                已有账号？{' '}
                <Link onClick={(e) => { e.preventDefault(); setMode('login') }}>去登录</Link>
              </Text>
            )}
          </div>
        </Card>
      </div>
    </div>
  )
}
