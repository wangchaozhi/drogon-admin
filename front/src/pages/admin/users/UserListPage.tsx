// 用户管理：分页 + 搜索 + 新增 + 编辑 + 删除 + 分配角色
import { useCallback, useEffect, useState } from 'react'
import {
  App as AntApp,
  Button,
  Card,
  Form,
  Input,
  Modal,
  Popconfirm,
  Select,
  Space,
  Switch,
  Table,
  Tag,
  Typography,
} from 'antd'
import type { TableProps } from 'antd'
import {
  DeleteOutlined,
  EditOutlined,
  PlusOutlined,
  SafetyOutlined,
  SearchOutlined,
  StopOutlined,
  UnlockOutlined,
} from '@ant-design/icons'
import { userApi, type UserCreateReq, type UserUpdateReq } from '../../../api/user'
import { roleApi } from '../../../api/role'
import type { Role, User } from '../../../api/types'
import HasPerm from '../../../components/HasPerm'
import { useAuth } from '../../../hooks/useAuth'

const { Title } = Typography

type FormValues = {
  name: string
  email: string
  password?: string
  disabled?: boolean
}

export default function UserListPage() {
  const { user: me } = useAuth()
  const { message } = AntApp.useApp()

  const [data, setData] = useState<User[]>([])
  const [total, setTotal] = useState(0)
  const [page, setPage] = useState(1)
  const [pageSize, setPageSize] = useState(10)
  const [keyword, setKeyword] = useState('')
  const [loading, setLoading] = useState(false)

  // 新建 / 编辑
  const [editing, setEditing] = useState<User | null>(null)
  const [formOpen, setFormOpen] = useState(false)
  const [saving, setSaving] = useState(false)
  const [form] = Form.useForm<FormValues>()

  // 分配角色
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

  // Initial remote-data loading intentionally owns this page's loading state.
  // eslint-disable-next-line react-hooks/set-state-in-effect
  useEffect(() => { void load() }, [load])
  useEffect(() => { roleApi.list().then((r) => setRoles(r.data)).catch(() => {}) }, [])

  const openCreate = () => {
    setEditing(null)
    form.resetFields()
    setFormOpen(true)
  }

  const openEdit = (u: User) => {
    setEditing(u)
    form.resetFields()
    form.setFieldsValue({
      name: u.name,
      email: u.email,
      password: '',
      disabled: u.disabled,
    })
    setFormOpen(true)
  }

  const submitForm = async () => {
    const v = await form.validateFields()
    setSaving(true)
    try {
      if (editing) {
        const req: UserUpdateReq = {
          name: v.name,
          email: v.email,
          disabled: v.disabled ?? false,
        }
        if (v.password && v.password.length > 0) req.password = v.password
        await userApi.update(editing.id, req)
      } else {
        const req: UserCreateReq = {
          name: v.name,
          email: v.email,
          password: v.password ?? '',
        }
        await userApi.create(req)
      }
      message.success('保存成功')
      setFormOpen(false)
      void load()
    } catch (e) {
      message.error(e instanceof Error ? e.message : String(e))
    } finally {
      setSaving(false)
    }
  }

  const handleDelete = async (id: number) => {
    try {
      await userApi.remove(id)
      message.success('删除成功')
      void load()
    } catch (e) {
      message.error(e instanceof Error ? e.message : String(e))
    }
  }

  const revokeSessions = async (id: number) => {
    try {
      await userApi.revokeSessions(id)
      message.success('该用户的现有会话已全部吊销')
    } catch (e) {
      message.error(e instanceof Error ? e.message : String(e))
    }
  }

  const unlockLogin = async (id: number) => {
    try {
      await userApi.unlock(id)
      message.success('登录锁定已解除')
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
      title: '状态',
      width: 90,
      render: (_v, row) => {
        const locked = row.locked_until > Math.floor(Date.now() / 1000)
        return (
          <Tag color={row.disabled ? 'error' : locked ? 'warning' : 'success'}>
            {row.disabled ? '已禁用' : locked ? '已锁定' : '正常'}
          </Tag>
        )
      },
    },
    {
      title: '注册时间',
      dataIndex: 'created_at',
      render: (v: number) => new Date(v * 1000).toLocaleString(),
      responsive: ['md'],
    },
    {
      title: '操作',
      key: 'op',
      width: 540,
      render: (_v, row) => {
        const locked = row.locked_until > Math.floor(Date.now() / 1000)
        return (
          <Space>
            <HasPerm code="user:assign-role">
              <Button size="small" icon={<SafetyOutlined />} onClick={() => openAssign(row)}>
                分配角色
              </Button>
            </HasPerm>
            <HasPerm code="user:update">
              <Button size="small" icon={<EditOutlined />} onClick={() => openEdit(row)}>
                编辑
              </Button>
              <Popconfirm
                title={`确认强制下线用户 ${row.name}?`}
                description="该用户所有现有 Token 将立即失效"
                okText="强制下线"
                cancelText="取消"
                disabled={row.id === me?.id}
                onConfirm={() => revokeSessions(row.id)}
              >
                <Button
                  size="small"
                  icon={<StopOutlined />}
                  disabled={row.id === me?.id}
                >
                  强制下线
                </Button>
              </Popconfirm>
              <Popconfirm
                title={`确认解除用户 ${row.name} 的登录锁定?`}
                okText="解除锁定"
                cancelText="取消"
                disabled={!locked}
                onConfirm={() => unlockLogin(row.id)}
              >
                <Button size="small" icon={<UnlockOutlined />} disabled={!locked}>
                  解除锁定
                </Button>
              </Popconfirm>
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
        )
      },
    },
  ]

  return (
    <Card className="page-card" styles={{ body: { padding: 16 } }}>
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', flexWrap: 'wrap', gap: 8, marginBottom: 12 }}>
        <Title level={4} style={{ margin: 0 }}>用户管理</Title>
        <Space wrap>
          <Input.Search
            placeholder="按昵称/邮箱搜索"
            allowClear
            enterButton={<SearchOutlined />}
            onSearch={(v) => { setKeyword(v); setPage(1) }}
            style={{ maxWidth: 320 }}
          />
          <HasPerm code="user:create">
            <Button type="primary" icon={<PlusOutlined />} onClick={openCreate}>新建用户</Button>
          </HasPerm>
        </Space>
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
        open={formOpen}
        title={editing ? '编辑用户' : '新建用户'}
        onCancel={() => setFormOpen(false)}
        onOk={submitForm}
        confirmLoading={saving}
        okText="保存"
        cancelText="取消"
        destroyOnHidden
      >
        <Form<FormValues> form={form} layout="vertical">
          <Form.Item name="name" label="昵称" rules={[{ required: true, message: '请输入昵称' }]}>
            <Input placeholder="例如 张三" />
          </Form.Item>
          <Form.Item
            name="email"
            label="邮箱"
            rules={[
              { required: true, message: '请输入邮箱' },
              { type: 'email', message: '邮箱格式不正确' },
            ]}
          >
            <Input placeholder="例如 user@example.com" />
          </Form.Item>
          <Form.Item
            name="password"
            label={editing ? '密码（留空不修改）' : '密码'}
            rules={editing
              ? [{ min: 12, max: 256, message: '密码长度需为 12–256 位', validateTrigger: 'onBlur' }]
              : [{ required: true, message: '请输入密码' }, { min: 12, max: 256, message: '密码长度需为 12–256 位' }]}
          >
            <Input.Password placeholder={editing ? '不修改可留空' : '至少 12 位'} autoComplete="new-password" />
          </Form.Item>
          {editing && (
            <Form.Item
              name="disabled"
              label="禁用账号"
              valuePropName="checked"
              extra={editing.id === me?.id ? '不能禁用当前登录账号' : '禁用后，该用户的现有会话会立即失效'}
            >
              <Switch disabled={editing.id === me?.id} />
            </Form.Item>
          )}
        </Form>
      </Modal>

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
