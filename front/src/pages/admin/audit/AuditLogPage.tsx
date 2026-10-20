import { useCallback, useEffect, useState } from 'react'
import {
  App as AntApp,
  Button,
  Card,
  Form,
  Input,
  InputNumber,
  Select,
  Space,
  Table,
  Tag,
  Typography,
} from 'antd'
import { ClearOutlined, SearchOutlined } from '@ant-design/icons'
import type { TableProps } from 'antd'
import { auditApi, type AuditLogFilters } from '../../../api/audit'
import type { AuditLog } from '../../../api/types'

const { Title, Text } = Typography

const actionLabels: Record<string, string> = {
  'user.create': '创建用户',
  'user.update': '更新用户',
  'user.delete': '删除用户',
  'user.assign_roles': '分配用户角色',
  'user.change_password': '用户自助改密',
  'user.revoke_sessions': '强制用户下线',
  'user.unlock': '解除登录锁定',
  'auth.logout': '退出登录',
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
  const [filterForm] = Form.useForm<AuditLogFilters>()
  const [items, setItems] = useState<AuditLog[]>([])
  const [total, setTotal] = useState(0)
  const [page, setPage] = useState(1)
  const [pageSize, setPageSize] = useState(20)
  const [filters, setFilters] = useState<AuditLogFilters>({})
  const [loading, setLoading] = useState(false)

  const load = useCallback(async () => {
    setLoading(true)
    try {
      const response = await auditApi.list(page, pageSize, filters)
      setItems(response.data.items)
      setTotal(response.data.total)
    } catch (error) {
      message.error(error instanceof Error ? error.message : String(error))
    } finally {
      setLoading(false)
    }
  }, [filters, message, page, pageSize])

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
      </div>
      <Form<AuditLogFilters>
        form={filterForm}
        layout="inline"
        onFinish={(values) => {
          setFilters({
            action: values.action || undefined,
            actor_user_id: values.actor_user_id || undefined,
            target_type: values.target_type?.trim() || undefined,
            target_id: values.target_id?.trim() || undefined,
            request_id: values.request_id?.trim() || undefined,
          })
          setPage(1)
        }}
        style={{ rowGap: 8, marginBottom: 16 }}
      >
        <Form.Item name="action">
          <Select
            allowClear
            showSearch
            placeholder="操作类型"
            options={actionOptions}
            style={{ minWidth: 210 }}
          />
        </Form.Item>
        <Form.Item name="actor_user_id">
          <InputNumber
            min={1}
            precision={0}
            placeholder="操作者 ID"
            style={{ width: 130 }}
          />
        </Form.Item>
        <Form.Item name="target_type">
          <Input placeholder="目标类型" maxLength={32} style={{ width: 130 }} />
        </Form.Item>
        <Form.Item name="target_id">
          <Input placeholder="目标 ID" maxLength={128} style={{ width: 130 }} />
        </Form.Item>
        <Form.Item name="request_id">
          <Input placeholder="请求 ID" maxLength={128} style={{ width: 210 }} />
        </Form.Item>
        <Form.Item>
          <Space>
            <Button type="primary" htmlType="submit" icon={<SearchOutlined />}>
              查询
            </Button>
            <Button
              icon={<ClearOutlined />}
              onClick={() => {
                filterForm.resetFields()
                setFilters({})
                setPage(1)
              }}
            >
              重置
            </Button>
          </Space>
        </Form.Item>
      </Form>
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
