// 注册表单：受控于父组件传入的 onSubmit。
import { Button, Form, Input } from 'antd'
import { LockOutlined, MailOutlined, UserOutlined } from '@ant-design/icons'
import { useState } from 'react'

export type RegisterValues = {
  name: string
  email: string
  password: string
}

type Props = {
  onSubmit: (v: RegisterValues) => Promise<void>
}

export default function RegisterForm({ onSubmit }: Props) {
  const [loading, setLoading] = useState(false)
  const [form] = Form.useForm<RegisterValues>()

  return (
    <Form<RegisterValues>
      form={form}
      layout="vertical"
      autoComplete="off"
      onFinish={async (v) => {
        setLoading(true)
        try {
          await onSubmit({
            name: v.name.trim(),
            email: v.email.trim(),
            password: v.password,
          })
          form.resetFields()
        } finally {
          setLoading(false)
        }
      }}
    >
      <Form.Item
        name="name"
        label="昵称"
        rules={[{ required: true, message: '请输入昵称' }]}
      >
        <Input prefix={<UserOutlined />} placeholder="用户昵称" size="large" />
      </Form.Item>
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
        rules={[
          { required: true, message: '请输入密码' },
          { min: 6, message: '密码至少 6 位' },
        ]}
      >
        <Input.Password prefix={<LockOutlined />} placeholder="至少 6 位" size="large" />
      </Form.Item>
      <Form.Item>
        <Button type="primary" htmlType="submit" loading={loading} block size="large">
          注册
        </Button>
      </Form.Item>
    </Form>
  )
}
