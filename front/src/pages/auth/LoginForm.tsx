// 登录表单：受控于父组件传入的 onSubmit；支持"记住密码"。
import { Button, Checkbox, Form, Input } from 'antd'
import { LockOutlined, MailOutlined } from '@ant-design/icons'
import { useState } from 'react'
import { remember } from '../../utils/remember'

export type LoginValues = {
  email: string
  password: string
  remember: boolean
}

type Props = {
  onSubmit: (v: LoginValues) => Promise<void>
  initialEmail?: string
}

export default function LoginForm({ onSubmit, initialEmail = '' }: Props) {
  const [loading, setLoading] = useState(false)

  // 初始值：优先用 remember 里的凭证，否则使用 initialEmail
  const saved = remember.load()
  const initialValues: LoginValues = saved
    ? { email: saved.email, password: saved.password, remember: true }
    : { email: initialEmail, password: '', remember: false }

  return (
    <Form<LoginValues>
      layout="vertical"
      autoComplete="on"
      initialValues={initialValues}
      onFinish={async (v) => {
        setLoading(true)
        try {
          const payload: LoginValues = {
            email: v.email.trim(),
            password: v.password,
            remember: !!v.remember,
          }
          await onSubmit(payload)
          // 登录成功后根据勾选更新本地凭证
          if (payload.remember) {
            remember.save({ email: payload.email, password: payload.password })
          } else {
            remember.clear()
          }
        } finally {
          setLoading(false)
        }
      }}
    >
      <Form.Item
        name="email"
        label="邮箱"
        rules={[
          { required: true, message: '请输入邮箱' },
          { type: 'email', message: '邮箱格式不正确' },
        ]}
      >
        <Input prefix={<MailOutlined />} placeholder="you@example.com" size="large" />
      </Form.Item>

      <Form.Item
        name="password"
        label="密码"
        rules={[{ required: true, message: '请输入密码' }]}
      >
        <Input.Password prefix={<LockOutlined />} placeholder="密码" size="large" />
      </Form.Item>

      <Form.Item name="remember" valuePropName="checked" style={{ marginBottom: 12 }}>
        <Checkbox>记住密码</Checkbox>
      </Form.Item>

      <Form.Item style={{ marginBottom: 0 }}>
        <Button type="primary" htmlType="submit" loading={loading} block size="large">
          登录
        </Button>
      </Form.Item>
    </Form>
  )
}
