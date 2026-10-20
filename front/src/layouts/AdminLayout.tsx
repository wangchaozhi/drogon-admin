// 后台布局：左侧 Sider（动态菜单）+ 顶部 Header（用户菜单）+ 右侧 Content。
// 手机端 Sider 折叠为 Drawer。
import { useState } from 'react'
import { Alert, App as AntApp, Avatar, Button, Drawer, Dropdown, Layout, Space, Typography } from 'antd'
import {
  LogoutOutlined,
  MenuFoldOutlined,
  MenuUnfoldOutlined,
  MenuOutlined as MobileMenuOutlined,
  UserOutlined,
  ProfileOutlined,
} from '@ant-design/icons'
import { Outlet, useNavigate } from 'react-router-dom'
import { authApi } from '../api/auth'
import DynamicMenu from '../components/DynamicMenu'
import { useAuth } from '../hooks/useAuth'
import { useDevice } from '../hooks/useDevice'

const { Header, Sider, Content } = Layout
const { Text } = Typography

export default function AdminLayout() {
  const { user, menus, roles, error, logout, refresh } = useAuth()
  const { isMobile } = useDevice()
  const { message, modal } = AntApp.useApp()
  const navigate = useNavigate()
  const [collapsed, setCollapsed] = useState(false)
  const [drawerOpen, setDrawerOpen] = useState(false)

  const confirmLogout = () => {
    modal.confirm({
      title: '退出登录',
      content: '确定要退出当前账号吗？',
      okText: '退出',
      cancelText: '取消',
      onOk: async () => {
        try {
          await authApi.logout()
          message.success('已安全退出，现有会话已吊销')
        } catch (logoutError) {
          message.warning(
            logoutError instanceof Error
              ? `服务端注销失败：${logoutError.message}`
              : '服务端注销失败，已清理本地会话',
          )
        } finally {
          logout()
        }
      },
    })
  }

  const sider = (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column' }}>
      <div className="admin-brand">
        <span className="admin-brand-dot" />
        {!collapsed && <span className="admin-brand-text">drogon-admin</span>}
      </div>
      <nav aria-label="主导航" style={{ flex: 1, overflowY: 'auto' }}>
        <DynamicMenu menus={menus} mode="inline" theme="dark" inlineCollapsed={!isMobile && collapsed} />
      </nav>
    </div>
  )

  return (
    <Layout style={{ minHeight: '100vh' }}>
      {!isMobile && (
        <Sider
          width={220}
          collapsedWidth={72}
          collapsible
          collapsed={collapsed}
          trigger={null}
          theme="dark"
          className="admin-sider"
        >
          {sider}
        </Sider>
      )}
      {isMobile && (
        <Drawer
          open={drawerOpen}
          onClose={() => setDrawerOpen(false)}
          placement="left"
          width={240}
          title="导航菜单"
          styles={{ body: { padding: 0, background: '#001529' } }}
        >
          {sider}
        </Drawer>
      )}
      <Layout>
        <Header className="admin-header">
          <Button
            type="text"
            icon={
              isMobile
                ? <MobileMenuOutlined />
                : collapsed ? <MenuUnfoldOutlined /> : <MenuFoldOutlined />
            }
            aria-label={isMobile ? '打开导航菜单' : collapsed ? '展开导航菜单' : '收起导航菜单'}
            aria-expanded={isMobile ? drawerOpen : !collapsed}
            onClick={() => (isMobile ? setDrawerOpen(true) : setCollapsed(!collapsed))}
          />
          <div style={{ flex: 1 }} />
          <Dropdown
            menu={{
              items: [
                { key: 'profile', icon: <ProfileOutlined />, label: '个人中心' },
                { type: 'divider' },
                { key: 'logout', icon: <LogoutOutlined />, label: '退出登录', danger: true },
              ],
              onClick: ({ key }) => {
                if (key === 'profile') navigate('/profile')
                else if (key === 'logout') confirmLogout()
              },
            }}
          >
            <Button type="text" aria-label="打开用户菜单" style={{ height: 'auto', padding: '4px 8px' }}>
              <Space>
                <Avatar size={isMobile ? 'small' : 'default'} icon={<UserOutlined />} />
                {!isMobile && (
                  <span>
                    <Text strong>{user?.name}</Text>
                    {roles.length > 0 && (
                      <Text type="secondary" style={{ marginLeft: 8, fontSize: 12 }}>
                        {roles.join(' / ')}
                      </Text>
                    )}
                  </span>
                )}
              </Space>
            </Button>
          </Dropdown>
        </Header>
        <Content className="admin-content">
          {error && (
            <Alert
              type="warning"
              showIcon
              closable
              message="权限状态刷新失败"
              description={error}
              action={
                <Button size="small" onClick={() => void refresh().catch(() => {})}>
                  重试
                </Button>
              }
              style={{ marginBottom: 12 }}
            />
          )}
          <Outlet />
        </Content>
      </Layout>
    </Layout>
  )
}
