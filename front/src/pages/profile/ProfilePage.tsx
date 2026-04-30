// 个人中心：展示当前用户信息 + 角色 + 权限汇总。
import { Card, Descriptions, Tag, Typography } from 'antd'
import { useAuth } from '../../hooks/useAuth'
import { useDevice } from '../../hooks/useDevice'

const { Title } = Typography

export default function ProfilePage() {
  const { user, roles, permissions } = useAuth()
  const { isMobile, isDesktop } = useDevice()
  if (!user) return null

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
    </div>
  )
}
