// 仪表盘：欢迎 + 核心统计卡片。
import { Card, Col, Row, Statistic, Typography } from 'antd'
import { DashboardOutlined, SafetyOutlined, TeamOutlined, UserOutlined } from '@ant-design/icons'
import { useAuth } from '../../hooks/useAuth'

const { Title, Paragraph } = Typography

export default function DashboardPage() {
  const { user, roles, permissions, menus } = useAuth()

  return (
    <div>
      <Card className="page-card" styles={{ body: { padding: 24 } }}>
        <Title level={3} style={{ margin: 0 }}>
          你好，{user?.name} 👋
        </Title>
        <Paragraph type="secondary" style={{ margin: '4px 0 0' }}>
          欢迎回到 c_web 控制台。当前角色：{roles.join(' / ') || '—'}
        </Paragraph>
      </Card>

      <Row gutter={[16, 16]} style={{ marginTop: 16 }}>
        <Col xs={12} md={6}>
          <Card className="stat-card">
            <Statistic title="我的角色" value={roles.length} prefix={<SafetyOutlined />} />
          </Card>
        </Col>
        <Col xs={12} md={6}>
          <Card className="stat-card">
            <Statistic title="我的权限" value={permissions.length} prefix={<TeamOutlined />} />
          </Card>
        </Col>
        <Col xs={12} md={6}>
          <Card className="stat-card">
            <Statistic title="可见菜单" value={menus.length} prefix={<DashboardOutlined />} />
          </Card>
        </Col>
        <Col xs={12} md={6}>
          <Card className="stat-card">
            <Statistic title="用户 ID" value={user?.id ?? 0} prefix={<UserOutlined />} />
          </Card>
        </Col>
      </Row>
    </div>
  )
}
