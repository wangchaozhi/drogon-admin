// 仪表盘：个人访问范围 + 仅授权管理员可见的系统概览。
import { useCallback, useEffect, useState } from 'react'
import { Alert, Button, Card, Col, Row, Skeleton, Space, Statistic, Typography } from 'antd'
import {
  AppstoreOutlined,
  AuditOutlined,
  DashboardOutlined,
  LockOutlined,
  ReloadOutlined,
  SafetyOutlined,
  StopOutlined,
  TeamOutlined,
  UserOutlined,
} from '@ant-design/icons'
import { dashboardApi } from '../../api/dashboard'
import type { DashboardStats } from '../../api/types'
import { useAuth } from '../../hooks/useAuth'

const { Title, Paragraph } = Typography

export default function DashboardPage() {
  const { user, roles, permissions, menus, hasPerm } = useAuth()
  const canViewStats = hasPerm('dashboard:stats')
  const [stats, setStats] = useState<DashboardStats | null>(null)
  const [statsLoading, setStatsLoading] = useState(canViewStats)
  const [statsError, setStatsError] = useState<string | null>(null)

  const loadStats = useCallback(async () => {
    if (!canViewStats) return
    setStatsLoading(true)
    setStatsError(null)
    try {
      const response = await dashboardApi.stats()
      setStats(response.data)
    } catch (error) {
      setStatsError(error instanceof Error ? error.message : String(error))
    } finally {
      setStatsLoading(false)
    }
  }, [canViewStats])

  // Initial remote-data loading intentionally owns the overview loading state.
  // eslint-disable-next-line react-hooks/set-state-in-effect
  useEffect(() => { void loadStats() }, [loadStats])

  return (
    <div>
      <Card className="page-card" styles={{ body: { padding: 24 } }}>
        <Title level={3} style={{ margin: 0 }}>
          你好，{user?.name} 👋
        </Title>
        <Paragraph type="secondary" style={{ margin: '4px 0 0' }}>
          欢迎回到 drogon-admin 控制台。当前角色：{roles.join(' / ') || '—'}
        </Paragraph>
      </Card>

      {canViewStats && (
        <Card className="page-card" styles={{ body: { padding: 24 } }} style={{ marginTop: 16 }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            flexWrap: 'wrap',
            gap: 8,
            marginBottom: 16,
          }}>
            <Space direction="vertical" size={0}>
              <Title level={4} style={{ margin: 0 }}>系统概览</Title>
              {stats && (
                <Typography.Text type="secondary">
                  更新于 {new Date(stats.generated_at * 1000).toLocaleString()}
                </Typography.Text>
              )}
            </Space>
            <Button
              icon={<ReloadOutlined />}
              loading={statsLoading}
              onClick={() => void loadStats()}
            >
              刷新
            </Button>
          </div>

          {statsError && (
            <Alert
              type="error"
              showIcon
              message="系统概览加载失败"
              description={statsError}
              action={<Button size="small" onClick={() => void loadStats()}>重试</Button>}
              style={{ marginBottom: 16 }}
            />
          )}

          {statsLoading && !stats ? (
            <Skeleton active paragraph={{ rows: 3 }} />
          ) : stats && (
            <Row gutter={[16, 16]}>
              <Col xs={12} md={8} xl={6}>
                <Card className="stat-card">
                  <Statistic title="用户总数" value={stats.total_users} prefix={<TeamOutlined />} />
                </Card>
              </Col>
              <Col xs={12} md={8} xl={6}>
                <Card className="stat-card">
                  <Statistic title="启用账号" value={stats.enabled_users} prefix={<UserOutlined />} />
                </Card>
              </Col>
              <Col xs={12} md={8} xl={6}>
                <Card className="stat-card">
                  <Statistic title="禁用账号" value={stats.disabled_users} prefix={<StopOutlined />} />
                </Card>
              </Col>
              <Col xs={12} md={8} xl={6}>
                <Card className="stat-card">
                  <Statistic title="登录锁定" value={stats.locked_users} prefix={<LockOutlined />} />
                </Card>
              </Col>
              <Col xs={12} md={8} xl={6}>
                <Card className="stat-card">
                  <Statistic title="角色总数" value={stats.total_roles} prefix={<SafetyOutlined />} />
                </Card>
              </Col>
              <Col xs={12} md={8} xl={6}>
                <Card className="stat-card">
                  <Statistic title="可见菜单" value={stats.visible_menus} prefix={<AppstoreOutlined />} />
                </Card>
              </Col>
              <Col xs={12} md={8} xl={6}>
                <Card className="stat-card">
                  <Statistic title="近 24 小时操作" value={stats.audit_events_24h} prefix={<AuditOutlined />} />
                </Card>
              </Col>
            </Row>
          )}
        </Card>
      )}

      <Title level={4} style={{ margin: '20px 0 12px' }}>我的访问范围</Title>
      <Row gutter={[16, 16]} style={{ marginTop: 16 }}>
        <Col xs={12} md={8}>
          <Card className="stat-card">
            <Statistic title="我的角色" value={roles.length} prefix={<SafetyOutlined />} />
          </Card>
        </Col>
        <Col xs={12} md={8}>
          <Card className="stat-card">
            <Statistic title="我的权限" value={permissions.length} prefix={<TeamOutlined />} />
          </Card>
        </Col>
        <Col xs={12} md={8}>
          <Card className="stat-card">
            <Statistic title="可见菜单" value={menus.length} prefix={<DashboardOutlined />} />
          </Card>
        </Col>
      </Row>
    </div>
  )
}
