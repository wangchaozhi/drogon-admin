// 用户管理：分页 + 搜索 + 删除 + 分配角色
import { useCallback, useEffect, useState } from 'react'
import {
  App as AntApp,
  Button,
  Card,
  Input,
  Modal,
  Popconfirm,
  Select,
  Space,
  Table,
  Tag,
  Typography,
} from 'antd'
import type { TableProps } from 'antd'
import { DeleteOutlined, SafetyOutlined, SearchOutlined } from '@ant-design/icons'
import { userApi } from '../../../api/user'
import { roleApi } from '../../../api/role'
import type { Role, User } from '../../../api/types'
import HasPerm from '../../../components/HasPerm'
import { useAuth } from '../../../hooks/useAuth'

const { Title } = Typography

export default function UserListPage() {
  const { user: me } = useAuth()
  const { message } = AntApp.useApp()

  const [data, setData] = useState<User[]>([])
  const [total, setTotal] = useState(0)
  const [page, setPage] = useState(1)
  const [pageSize, setPageSize] = useState(10)
  const [keyword, setKeyword] = useState('')
  const [loading, setLoading] = useState(false)

  const [roles, setRoles] = useState<Role[]>([])
  const [assigning, setAssigning] = useState<User | null>(null)
  const [selectedRoleIds, setSelectedRoleIds] = useState<number[]>([])
  const [assignLoading, setAssignLoading] = useState(false)

  const load = useCallback(async () => {
    setLoading(true)
    try {
      const r = await userApi.list(page, pageSize, keyword)
      setData(r.data.items)
      setTotal(r.data.total)
    } catch (e) {
      message.error(e instanceof Error ? e.message : String(e))
    } finally {
      setLoading(false)
    }
  }, [page, pageSize, keyword, message])

  useEffect(() => { void load() }, [load])
  useEffect(() => { roleApi.list().then((r) => setRoles(r.data)).catch(() => {}) }, [])

  const handleDelete = async (id: number) => {
    try {
      await userApi.remove(id)
      message.success('删除成功')
      void load()
    } catch (e) {
      message.error(e instanceof Error ? e.message : String(e))
    }
  }

  const openAssign = async (u: User) => {
    setAssigning(u)
    try {
      const r = await userApi.getRoles(u.id)
      setSelectedRoleIds(r.data)
    } catch {
      setSelectedRoleIds([])
    }
  }

  const submitAssign = async () => {
    if (!assigning) return
    setAssignLoading(true)
    try {
      await userApi.setRoles(assigning.id, selectedRoleIds)
      message.success('角色已更新')
      setAssigning(null)
    } catch (e) {
      message.error(e instanceof Error ? e.message : String(e))
    } finally {
      setAssignLoading(false)
    }
  }

  const columns: TableProps<User>['columns'] = [
    { title: 'ID', dataIndex: 'id', width: 80 },
    { title: '昵称', dataIndex: 'name' },
    { title: '邮箱', dataIndex: 'email' },
    {
      title: '注册时间',
      dataIndex: 'created_at',
      render: (v: number) => new Date(v * 1000).toLocaleString(),
      responsive: ['md'],
    },
    {
      title: '操作',
      key: 'op',
      width: 220,
      render: (_v, row) => (
        <Space>
          <HasPerm code="user:assign-role">
            <Button size="small" icon={<SafetyOutlined />} onClick={() => openAssign(row)}>
              分配角色
            </Button>
          </HasPerm>
          <HasPerm code="user:delete">
            <Popconfirm
              title={`确认删除用户 ${row.name}?`}
              okText="删除"
              okButtonProps={{ danger: true }}
              cancelText="取消"
              disabled={row.id === me?.id}
              onConfirm={() => handleDelete(row.id)}
            >
              <Button size="small" danger icon={<DeleteOutlined />} disabled={row.id === me?.id}>
                删除
              </Button>
            </Popconfirm>
          </HasPerm>
        </Space>
      ),
    },
  ]

  return (
    <Card className="page-card" styles={{ body: { padding: 16 } }}>
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', flexWrap: 'wrap', gap: 8, marginBottom: 12 }}>
        <Title level={4} style={{ margin: 0 }}>用户管理</Title>
        <Input.Search
          placeholder="按昵称/邮箱搜索"
          allowClear
          enterButton={<SearchOutlined />}
          onSearch={(v) => { setKeyword(v); setPage(1) }}
          style={{ maxWidth: 320 }}
        />
      </div>
      <Table<User>
        rowKey="id"
        columns={columns}
        dataSource={data}
        loading={loading}
        scroll={{ x: 'max-content' }}
        pagination={{
          current: page,
          pageSize,
          total,
          showSizeChanger: true,
          onChange: (p, ps) => { setPage(p); setPageSize(ps) },
        }}
      />

      <Modal
        open={!!assigning}
        title={assigning ? `为「${assigning.name}」分配角色` : ''}
        onCancel={() => setAssigning(null)}
        onOk={submitAssign}
        confirmLoading={assignLoading}
        okText="保存"
        cancelText="取消"
      >
        <Select
          mode="multiple"
          style={{ width: '100%' }}
          placeholder="选择角色"
          value={selectedRoleIds}
          onChange={(v) => setSelectedRoleIds(v)}
          options={roles.map((r) => ({ label: `${r.name} (${r.code})`, value: r.id }))}
          optionFilterProp="label"
        />
        <div style={{ marginTop: 12 }}>
          {selectedRoleIds.map((id) => {
            const r = roles.find((x) => x.id === id)
            return r ? <Tag color="geekblue" key={id}>{r.name}</Tag> : null
          })}
        </div>
      </Modal>
    </Card>
  )
}
