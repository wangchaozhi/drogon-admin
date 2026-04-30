// 角色管理：列表 + CRUD + 分配权限（按菜单分组的 Tree）
import { useCallback, useEffect, useMemo, useState } from 'react'
import {
  App as AntApp,
  Button,
  Card,
  Drawer,
  Form,
  Input,
  Modal,
  Popconfirm,
  Space,
  Table,
  Tree,
  Typography,
} from 'antd'
import type { TableProps } from 'antd'
import type { DataNode } from 'antd/es/tree'
import { DeleteOutlined, EditOutlined, PlusOutlined, SafetyOutlined } from '@ant-design/icons'
import { roleApi, type RoleUpsertReq } from '../../../api/role'
import { permissionApi } from '../../../api/permission'
import { menuApi } from '../../../api/menu'
import type { Menu, Permission, Role } from '../../../api/types'
import HasPerm from '../../../components/HasPerm'

const { Title } = Typography

function buildPermTree(menus: Menu[], perms: Permission[]): DataNode[] {
  const permsByMenu = new Map<number | 'none', Permission[]>()
  for (const p of perms) {
    const mid = p.menu_id
    const k: number | 'none' = !mid ? 'none' : mid
    const arr = permsByMenu.get(k) ?? []
    arr.push(p)
    permsByMenu.set(k, arr)
  }
  const toNode = (m: Menu): DataNode => {
    const children: DataNode[] = []
    if (m.children?.length) children.push(...m.children.map(toNode))
    const ps = permsByMenu.get(m.id) ?? []
    for (const p of ps) {
      children.push({ key: `p-${p.id}`, title: `${p.name} · ${p.code}`, isLeaf: true })
    }
    return { key: `m-${m.id}`, title: m.name, children }
  }
  const nodes = menus.map(toNode)
  const orphans = permsByMenu.get('none') ?? []
  if (orphans.length) {
    nodes.push({
      key: 'orphan',
      title: '未归属菜单',
      children: orphans.map((p) => ({ key: `p-${p.id}`, title: `${p.name} · ${p.code}`, isLeaf: true })),
    })
  }
  return nodes
}

export default function RoleListPage() {
  const { message } = AntApp.useApp()

  const [data, setData] = useState<Role[]>([])
  const [loading, setLoading] = useState(false)

  const [editing, setEditing] = useState<Role | null>(null)
  const [modalOpen, setModalOpen] = useState(false)
  const [form] = Form.useForm<RoleUpsertReq>()
  const [saving, setSaving] = useState(false)

  const [assigning, setAssigning] = useState<Role | null>(null)
  const [perms, setPerms] = useState<Permission[]>([])
  const [menus, setMenus] = useState<Menu[]>([])
  const [checkedPermIds, setCheckedPermIds] = useState<number[]>([])
  const [assignSaving, setAssignSaving] = useState(false)

  const load = useCallback(async () => {
    setLoading(true)
    try {
      const r = await roleApi.list()
      setData(r.data)
    } catch (e) {
      message.error(e instanceof Error ? e.message : String(e))
    } finally {
      setLoading(false)
    }
  }, [message])

  useEffect(() => { void load() }, [load])

  const openCreate = () => {
    setEditing(null)
    form.resetFields()
    setModalOpen(true)
  }
  const openEdit = (r: Role) => {
    setEditing(r)
    form.setFieldsValue({ code: r.code, name: r.name, description: r.description })
    setModalOpen(true)
  }
  const submitForm = async () => {
    const v = await form.validateFields()
    setSaving(true)
    try {
      if (editing) await roleApi.update(editing.id, v)
      else await roleApi.create(v)
      message.success('保存成功')
      setModalOpen(false)
      void load()
    } catch (e) {
      message.error(e instanceof Error ? e.message : String(e))
    } finally {
      setSaving(false)
    }
  }

  const handleDelete = async (id: number) => {
    try {
      await roleApi.remove(id)
      message.success('删除成功')
      void load()
    } catch (e) {
      message.error(e instanceof Error ? e.message : String(e))
    }
  }

  const openAssign = async (r: Role) => {
    setAssigning(r)
    try {
      const [mr, pr, cur] = await Promise.all([
        menuApi.tree(),
        permissionApi.list(),
        roleApi.getPermissions(r.id),
      ])
      setMenus(mr.data)
      setPerms(pr.data)
      setCheckedPermIds(cur.data)
    } catch (e) {
      message.error(e instanceof Error ? e.message : String(e))
    }
  }

  const treeData = useMemo(() => buildPermTree(menus, perms), [menus, perms])
  const checkedKeys = useMemo(() => checkedPermIds.map((id) => `p-${id}`), [checkedPermIds])

  const submitAssign = async () => {
    if (!assigning) return
    setAssignSaving(true)
    try {
      await roleApi.setPermissions(assigning.id, checkedPermIds)
      message.success('权限已更新')
      setAssigning(null)
    } catch (e) {
      message.error(e instanceof Error ? e.message : String(e))
    } finally {
      setAssignSaving(false)
    }
  }

  const columns: TableProps<Role>['columns'] = [
    { title: 'ID', dataIndex: 'id', width: 80 },
    { title: '编码', dataIndex: 'code' },
    { title: '名称', dataIndex: 'name' },
    { title: '描述', dataIndex: 'description', responsive: ['md'] },
    {
      title: '操作',
      key: 'op',
      width: 260,
      render: (_v, row) => (
        <Space>
          <HasPerm code="role:assign">
            <Button size="small" icon={<SafetyOutlined />} onClick={() => openAssign(row)}>权限</Button>
          </HasPerm>
          <HasPerm code="role:update">
            <Button size="small" icon={<EditOutlined />} onClick={() => openEdit(row)}>编辑</Button>
          </HasPerm>
          <HasPerm code="role:delete">
            <Popconfirm
              title={`确认删除角色 ${row.name}?`}
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
        <Title level={4} style={{ margin: 0 }}>角色管理</Title>
        <HasPerm code="role:create">
          <Button type="primary" icon={<PlusOutlined />} onClick={openCreate}>新建角色</Button>
        </HasPerm>
      </div>
      <Table<Role>
        rowKey="id"
        columns={columns}
        dataSource={data}
        loading={loading}
        scroll={{ x: 'max-content' }}
        pagination={false}
      />

      <Modal
        open={modalOpen}
        title={editing ? '编辑角色' : '新建角色'}
        onCancel={() => setModalOpen(false)}
        onOk={submitForm}
        confirmLoading={saving}
        okText="保存"
        cancelText="取消"
        destroyOnHidden
      >
        <Form form={form} layout="vertical">
          <Form.Item name="code" label="编码" rules={[{ required: true }]}>
            <Input placeholder="例如 admin" disabled={!!editing} />
          </Form.Item>
          <Form.Item name="name" label="名称" rules={[{ required: true }]}>
            <Input placeholder="例如 管理员" />
          </Form.Item>
          <Form.Item name="description" label="描述">
            <Input.TextArea rows={3} />
          </Form.Item>
        </Form>
      </Modal>

      <Drawer
        open={!!assigning}
        onClose={() => setAssigning(null)}
        title={assigning ? `为「${assigning.name}」分配权限` : ''}
        width={480}
        extra={
          <Space>
            <Button onClick={() => setAssigning(null)}>取消</Button>
            <Button type="primary" loading={assignSaving} onClick={submitAssign}>保存</Button>
          </Space>
        }
      >
        <Tree
          checkable
          selectable={false}
          defaultExpandAll
          treeData={treeData}
          checkedKeys={checkedKeys}
          onCheck={(keys) => {
            const arr = Array.isArray(keys) ? keys : keys.checked
            const permIds: number[] = []
            for (const k of arr) {
              const s = String(k)
              if (s.startsWith('p-')) permIds.push(Number(s.slice(2)))
            }
            setCheckedPermIds(permIds)
          }}
        />
      </Drawer>
    </Card>
  )
}
