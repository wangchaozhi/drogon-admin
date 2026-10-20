// 个人中心：展示当前用户信息 + 角色 + 权限汇总。
import { useState } from 'react'
import { Button, Card, Descriptions, Form, Input, Tag, Typography, message } from 'antd'
import { useNavigate } from 'react-router-dom'
import { userApi } from '../../api/user'
import { useAuth } from '../../hooks/useAuth'
import { useDevice } from '../../hooks/useDevice'

const { Title } = Typography

type PasswordForm = {
  currentPassword: string
  newPassword: string
  confirmPassword: string
}

export default function ProfilePage() {
  const { user, roles, permissions, logout } = useAuth()
  const { isMobile, isDesktop } = useDevice()
  const navigate = useNavigate()
  const [submitting, setSubmitting] = useState(false)
  if (!user) return null

  const changePassword = async (values: PasswordForm) => {
    setSubmitting(true)
    try {
      await userApi.changePassword({
        current_password: values.currentPassword,
        new_password: values.newPassword,
      })
      message.success('密码已修改，请重新登录')
      logout()
      navigate('/login', { replace: true })
    } catch (error) {
      message.error(error instanceof Error ? error.message : String(error))
    } finally {
      setSubmitting(false)
    }
  }

  return (
    <div>
      <Card className="page-card" styles={{ body: { padding: isMobile ? 12 : 24 } }}>
        <Title level={4} style={{ marginTop: 0 }}>个人中心</Title>
        <Descriptions
          column={isDesktop ? 2 : 1}
          bordered
          size={isMobile ? 'small' : 'default'}
          labelStyle={{ width: isMobile ? 96 : 140 }}
        >
          <Descriptions.Item label="ID">{user.id}</Descriptions.Item>
          <Descriptions.Item label="昵称">{user.name}</Descriptions.Item>
          <Descriptions.Item label="邮箱" span={isDesktop ? 2 : 1}>{user.email}</Descriptions.Item>
          <Descriptions.Item label="注册时间" span={isDesktop ? 2 : 1}>
            {new Date(user.created_at * 1000).toLocaleString()}
          </Descriptions.Item>
          <Descriptions.Item label="角色" span={isDesktop ? 2 : 1}>
            {roles.length ? roles.map((r) => <Tag color="geekblue" key={r}>{r}</Tag>) : '—'}
          </Descriptions.Item>
          <Descriptions.Item label={`权限 (${permissions.length})`} span={isDesktop ? 2 : 1}>
            {permissions.length
              ? permissions.map((p) => <Tag key={p} style={{ marginBottom: 4 }}>{p}</Tag>)
              : '—'}
          </Descriptions.Item>
        </Descriptions>
      </Card>
      <Card
        className="page-card"
        style={{ marginTop: 16 }}
        styles={{ body: { padding: isMobile ? 12 : 24 } }}
      >
        <Title level={4} style={{ marginTop: 0 }}>修改密码</Title>
        <Typography.Paragraph type="secondary">
          修改成功后，所有已登录设备上的会话都会失效。
        </Typography.Paragraph>
        <Form<PasswordForm>
          layout="vertical"
          onFinish={changePassword}
          style={{ maxWidth: 480 }}
          requiredMark={false}
        >
          <Form.Item
            label="当前密码"
            name="currentPassword"
            rules={[{ required: true, message: '请输入当前密码' }]}
          >
            <Input.Password autoComplete="current-password" />
          </Form.Item>
          <Form.Item
            label="新密码"
            name="newPassword"
            rules={[
              { required: true, message: '请输入新密码' },
              { min: 12, max: 256, message: '密码长度应为 12–256 位' },
            ]}
          >
            <Input.Password autoComplete="new-password" />
          </Form.Item>
          <Form.Item
            label="确认新密码"
            name="confirmPassword"
            dependencies={['newPassword']}
            rules={[
              { required: true, message: '请再次输入新密码' },
              ({ getFieldValue }) => ({
                validator(_, value) {
                  if (!value || getFieldValue('newPassword') === value) {
                    return Promise.resolve()
                  }
                  return Promise.reject(new Error('两次输入的密码不一致'))
                },
              }),
            ]}
          >
            <Input.Password autoComplete="new-password" />
          </Form.Item>
          <Button type="primary" htmlType="submit" loading={submitting}>
            修改密码
          </Button>
        </Form>
      </Card>
    </div>
  )
}
