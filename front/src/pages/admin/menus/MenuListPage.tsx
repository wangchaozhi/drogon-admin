// 菜单管理：树形展示 + CRUD
import { useCallback, useEffect, useMemo, useState } from 'react'
import {
  App as AntApp,
  Button,
  Card,
  Form,
  Input,
  InputNumber,
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
import { DeleteOutlined, EditOutlined, PlusOutlined } from '@ant-design/icons'
import { menuApi, type MenuUpsertReq } from '../../../api/menu'
import type { Menu } from '../../../api/types'
import HasPerm from '../../../components/HasPerm'

const { Title } = Typography

// 表单内部使用：visible 用 boolean，提交时转为 0/1
type MenuFormValues = {
  parent_id: number
  name: string
  type: 'dir' | 'menu'
  path?: string
  icon?: string
  component?: string
  sort?: number
  visible: boolean
}

type FlatMenu = Menu & { depth: number }
function flatten(tree: Menu[], depth = 0, out: FlatMenu[] = []): FlatMenu[] {
  for (const m of tree) {
    out.push({ ...m, depth })
    if (m.children?.length) flatten(m.children, depth + 1, out)
  }
  return out
}

const iconOptions = [
  'DashboardOutlined', 'SettingOutlined', 'TeamOutlined', 'UserOutlined',
  'MenuOutlined', 'SafetyOutlined', 'ProfileOutlined', 'AppstoreOutlined',
].map((v) => ({ label: v, value: v }))

const typeOptions = [
  { label: '目录（dir）', value: 'dir' },
  { label: '菜单（menu）', value: 'menu' },
]

export default function MenuListPage() {
  const { message } = AntApp.useApp()
  const [tree, setTree] = useState<Menu[]>([])
  const [loading, setLoading] = useState(false)
  const [editing, setEditing] = useState<Menu | null>(null)
  const [open, setOpen] = useState(false)
  const [saving, setSaving] = useState(false)
  const [form] = Form.useForm<MenuFormValues>()

  const load = useCallback(async () => {
    setLoading(true)
    try {
      const r = await menuApi.tree()
      setTree(r.data)
    } catch (e) {
      message.error(e instanceof Error ? e.message : String(e))
    } finally {
      setLoading(false)
    }
  }, [message])

  useEffect(() => { void load() }, [load])

  const list = useMemo(() => flatten(tree), [tree])

  const parentOptions = useMemo(
    () => [
      { label: '— 顶级 —', value: 0 },
      ...list.map((m) => ({ label: `${'　'.repeat(m.depth)}${m.name}`, value: m.id })),
    ],
    [list],
  )

  const openCreate = (parent?: Menu) => {
    setEditing(null)
    form.resetFields()
    form.setFieldsValue({
      parent_id: parent?.id ?? 0,
      sort: 0,
      visible: true,
      type: 'menu',
    })
    setOpen(true)
  }
  const openEdit = (m: Menu) => {
    setEditing(m)
    form.setFieldsValue({
      parent_id: m.parent_id ?? 0,
      name: m.name,
      type: m.type,
      path: m.path,
      icon: m.icon,
      component: m.component,
      sort: m.sort,
      visible: m.visible === 1,
    })
    setOpen(true)
  }
  const submit = async () => {
    const v = await form.validateFields()
    const req: MenuUpsertReq = {
      parent_id: Number(v.parent_id) || 0,
      name: v.name,
      type: v.type,
      path: v.path,
      icon: v.icon,
      component: v.component,
      sort: v.sort ?? 0,
      visible: v.visible ? 1 : 0,
    }
    setSaving(true)
    try {
      if (editing) await menuApi.update(editing.id, req)
      else await menuApi.create(req)
      message.success('保存成功')
      setOpen(false)
      void load()
    } catch (e) {
      message.error(e instanceof Error ? e.message : String(e))
    } finally {
      setSaving(false)
    }
  }
  const handleDelete = async (id: number) => {
    try {
      await menuApi.remove(id)
      message.success('删除成功')
      void load()
    } catch (e) {
      message.error(e instanceof Error ? e.message : String(e))
    }
  }

  const columns: TableProps<FlatMenu>['columns'] = [
    {
      title: '名称',
      dataIndex: 'name',
      render: (v: string, row) => (
        <span>
          {'　'.repeat(row.depth)}
          {row.depth > 0 ? '└ ' : ''}
          <strong>{v}</strong>
        </span>
      ),
    },
    {
      title: '类型',
      dataIndex: 'type',
      width: 90,
      render: (v: string) => (v === 'dir' ? <Tag color="blue">目录</Tag> : <Tag color="geekblue">菜单</Tag>),
    },
    { title: '路径', dataIndex: 'path', responsive: ['md'] },
    { title: '图标', dataIndex: 'icon', responsive: ['md'] },
    { title: '排序', dataIndex: 'sort', width: 80 },
    {
      title: '可见',
      dataIndex: 'visible',
      width: 80,
      render: (v: number) => (v === 1 ? <Tag color="green">显示</Tag> : <Tag>隐藏</Tag>),
    },
    {
      title: '操作',
      key: 'op',
      width: 260,
      render: (_v, row) => (
        <Space>
          <HasPerm code="menu:create">
            <Button size="small" icon={<PlusOutlined />} onClick={() => openCreate(row)}>子菜单</Button>
          </HasPerm>
          <HasPerm code="menu:update">
            <Button size="small" icon={<EditOutlined />} onClick={() => openEdit(row)}>编辑</Button>
          </HasPerm>
          <HasPerm code="menu:delete">
            <Popconfirm
              title={`删除菜单 ${row.name}?`}
              okText="删除"
              okButtonProps={{ danger: true }}
              cancelText="取消"
              onConfirm={() => handleDelete(row.id)}
            >
              <Button size="small" danger icon={<DeleteOutlined />}>删除</Button>
            </Popconfirm>
          </HasPerm>
        </Space>
      ),
    },
  ]

  return (
    <Card className="page-card" styles={{ body: { padding: 16 } }}>
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: 12 }}>
        <Title level={4} style={{ margin: 0 }}>菜单管理</Title>
        <HasPerm code="menu:create">
          <Button type="primary" icon={<PlusOutlined />} onClick={() => openCreate()}>新建顶级</Button>
        </HasPerm>
      </div>
      <Table<FlatMenu>
        rowKey="id"
        columns={columns}
        dataSource={list}
        loading={loading}
        scroll={{ x: 'max-content' }}
        pagination={false}
      />

      <Modal
        open={open}
        title={editing ? '编辑菜单' : '新建菜单'}
        onCancel={() => setOpen(false)}
        onOk={submit}
        confirmLoading={saving}
        okText="保存"
        cancelText="取消"
        destroyOnHidden
      >
        <Form<MenuFormValues> form={form} layout="vertical">
          <Form.Item name="parent_id" label="父菜单">
            <Select options={parentOptions} />
          </Form.Item>
          <Form.Item name="name" label="名称" rules={[{ required: true }]}>
            <Input />
          </Form.Item>
          <Form.Item name="type" label="类型" rules={[{ required: true }]}>
            <Select options={typeOptions} />
          </Form.Item>
          <Form.Item name="path" label="前端路径">
            <Input placeholder="例如 /admin/users" />
          </Form.Item>
          <Form.Item name="icon" label="图标">
            <Select options={iconOptions} allowClear showSearch />
          </Form.Item>
          <Form.Item name="component" label="组件标识">
            <Input placeholder="例如 UserListPage" />
          </Form.Item>
          <Form.Item name="sort" label="排序">
            <InputNumber min={0} style={{ width: '100%' }} />
          </Form.Item>
          <Form.Item name="visible" label="可见" valuePropName="checked">
            <Switch />
          </Form.Item>
        </Form>
      </Modal>
    </Card>
  )
}
