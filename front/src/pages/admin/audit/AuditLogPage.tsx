import { useCallback, useEffect, useState } from 'react'
import {
  App as AntApp,
  Card,
  Select,
  Space,
  Table,
  Tag,
  Typography,
} from 'antd'
import type { TableProps } from 'antd'
import { auditApi } from '../../../api/audit'
import type { AuditLog } from '../../../api/types'

const { Title, Text } = Typography

const actionLabels: Record<string, string> = {
  'user.create': '创建用户',
  'user.update': '更新用户',
  'user.delete': '删除用户',
  'user.assign_roles': '分配用户角色',
  'user.change_password': '用户自助改密',
  'role.create': '创建角色',
  'role.update': '更新角色',
  'role.delete': '删除角色',
  'role.assign_permissions': '分配角色权限',
  'menu.create': '创建菜单',
  'menu.update': '更新菜单',
  'menu.delete': '删除菜单',
}

const actionOptions = Object.entries(actionLabels).map(([value, label]) => ({
  value,
  label,
}))

export default function AuditLogPage() {
  const { message } = AntApp.useApp()
  const [items, setItems] = useState<AuditLog[]>([])
  const [total, setTotal] = useState(0)
  const [page, setPage] = useState(1)
  const [pageSize, setPageSize] = useState(20)
  const [action, setAction] = useState<string>()
  const [loading, setLoading] = useState(false)

  const load = useCallback(async () => {
    setLoading(true)
    try {
      const response = await auditApi.list(page, pageSize, action ?? '')
      setItems(response.data.items)
      setTotal(response.data.total)
    } catch (error) {
      message.error(error instanceof Error ? error.message : String(error))
    } finally {
      setLoading(false)
    }
  }, [action, message, page, pageSize])

  // Initial remote-data loading intentionally owns this page's loading state.
  // eslint-disable-next-line react-hooks/set-state-in-effect
  useEffect(() => { void load() }, [load])

  const columns: TableProps<AuditLog>['columns'] = [
    {
      title: '时间',
      dataIndex: 'created_at',
      width: 180,
      render: (value: number) => new Date(value * 1000).toLocaleString(),
    },
    {
      title: '操作',
      dataIndex: 'action',
      width: 180,
      render: (value: string) => (
        <Space direction="vertical" size={0}>
          <Tag color="blue">{actionLabels[value] ?? value}</Tag>
          <Text type="secondary" copyable>{value}</Text>
        </Space>
      ),
    },
    { title: '操作者 ID', dataIndex: 'actor_user_id', width: 110 },
    {
      title: '目标',
      width: 160,
      render: (_value, row) => `${row.target_type} #${row.target_id}`,
    },
    {
      title: '安全详情',
      dataIndex: 'details',
      render: (value: Record<string, unknown>) => (
        <Text code style={{ whiteSpace: 'pre-wrap', wordBreak: 'break-all' }}>
          {JSON.stringify(value)}
        </Text>
      ),
    },
    {
      title: '请求 ID',
      dataIndex: 'request_id',
      width: 220,
      responsive: ['lg'],
      render: (value: string) => <Text copyable>{value}</Text>,
    },
  ]

  return (
    <Card className="page-card" styles={{ body: { padding: 16 } }}>
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        flexWrap: 'wrap',
        gap: 8,
        marginBottom: 12,
      }}>
        <Title level={4} style={{ margin: 0 }}>审计日志</Title>
        <Select
          allowClear
          showSearch
          placeholder="按操作类型筛选"
          options={actionOptions}
          value={action}
          style={{ minWidth: 220 }}
          onChange={(value) => {
            setAction(value)
            setPage(1)
          }}
        />
      </div>
      <Table<AuditLog>
        rowKey="id"
        columns={columns}
        dataSource={items}
        loading={loading}
        scroll={{ x: 'max-content' }}
        pagination={{
          current: page,
          pageSize,
          total,
          showSizeChanger: true,
          onChange: (nextPage, nextPageSize) => {
            setPage(nextPage)
            setPageSize(nextPageSize)
          },
        }}
      />
    </Card>
  )
}
