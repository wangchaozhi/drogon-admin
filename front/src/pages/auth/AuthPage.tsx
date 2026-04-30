// 登录/注册容器：默认登录页，底部"立即注册"链接切换到注册页；
// 注册页底部提供"去登录"链接回切。
import { useState } from 'react'
import { App as AntApp, Card, Divider, Typography } from 'antd'
import LoginForm, { type LoginValues } from './LoginForm'
import RegisterForm, { type RegisterValues } from './RegisterForm'
import { api } from '../../api'
import { useAuth } from '../../hooks/useAuth'

const { Title, Paragraph, Link, Text } = Typography

type Mode = 'login' | 'register'

export default function AuthPage() {
  const [mode, setMode] = useState<Mode>('login')
  const [rememberedEmail, setRememberedEmail] = useState('')
  const { login } = useAuth()
  const { message } = AntApp.useApp()

  async function handleLogin(v: LoginValues) {
    try {
      await login(v.email, v.password)
      message.success('登录成功')
    } catch (e) {
      message.error(e instanceof Error ? e.message : String(e))
      throw e // 让 LoginForm 感知失败，不保存"记住密码"
    }
  }

  async function handleRegister(v: RegisterValues) {
    try {
      await api.register(v.name, v.email, v.password)
      message.success('注册成功，请登录')
      setRememberedEmail(v.email)
      setMode('login')
    } catch (e) {
      message.error(e instanceof Error ? e.message : String(e))
    }
  }

  return (
    <div className="auth-page-wrapper">
      <Card className="auth-card" styles={{ body: { padding: 24 } }}>
        <div style={{ textAlign: 'center', marginBottom: 20 }}>
          <Title level={3} style={{ margin: 0 }}>
            {mode === 'login' ? '登录 c_web' : '创建新账号'}
          </Title>
          <Paragraph type="secondary" style={{ margin: '4px 0 0' }}>
            {mode === 'login' ? '欢迎回来，请登录你的账户' : '请填写信息完成注册'}
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
              <Link
                onClick={(e) => {
                  e.preventDefault()
                  setMode('register')
                }}
              >
                立即注册
              </Link>
            </Text>
          ) : (
            <Text type="secondary">
              已有账号？{' '}
              <Link
                onClick={(e) => {
                  e.preventDefault()
                  setMode('login')
                }}
              >
                去登录
              </Link>
            </Text>
          )}
        </div>
      </Card>
    </div>
  )
}
