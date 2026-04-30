// 用户主页：展示当前用户信息。
// 桌面/平板 Descriptions 2 列、手机 1 列。
import { useEffect, useState } from 'react'
import { App as AntApp, Card, Descriptions, Layout, Spin } from 'antd'
import AppHeader from '../../components/AppHeader'
import { api, type User } from '../../api'
import { useAuth } from '../../hooks/useAuth'
import { useDevice } from '../../hooks/useDevice'

const { Content, Footer } = Layout

export default function HomePage() {
  const { user, logout } = useAuth()
  const { isMobile, isDesktop } = useDevice()
  const [me, setMe] = useState<User | null>(user)
  const [refreshing, setRefreshing] = useState(true)
  const { message } = AntApp.useApp()

  useEffect(() => {
    api.me()
      .then((r) => { if (r.data) setMe(r.data) })
      .catch((e) => message.error('拉取用户信息失败: ' + (e instanceof Error ? e.message : String(e))))
      .finally(() => setRefreshing(false))
  }, [message])

  const current = me ?? user
  if (!current) return null

  return (
    <Layout style={{ minHeight: '100vh' }}>
      <AppHeader user={current} onLogout={logout} />
      <Content className="home-content">
        <Card
          title="我的信息"
          extra={refreshing ? <Spin size="small" /> : null}
          styles={{ body: { padding: isMobile ? 12 : 24 } }}
        >
          <Descriptions
            column={isDesktop ? 2 : 1}
            bordered
            size={isMobile ? 'small' : 'default'}
            labelStyle={{ width: isMobile ? 96 : 140 }}
          >
            <Descriptions.Item label="ID">{current.id}</Descriptions.Item>
            <Descriptions.Item label="昵称">{current.name}</Descriptions.Item>
            <Descriptions.Item label="邮箱" span={isDesktop ? 2 : 1}>
              {current.email}
            </Descriptions.Item>
            <Descriptions.Item label="注册时间" span={isDesktop ? 2 : 1}>
              {new Date(current.created_at * 1000).toLocaleString()}
            </Descriptions.Item>
          </Descriptions>
        </Card>
      </Content>
      <Footer style={{ textAlign: 'center', padding: isMobile ? '12px' : '24px' }}>
        c_web © {new Date().getFullYear()}
      </Footer>
    </Layout>
  )
}
