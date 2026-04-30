// 根据后端返回的 menus 树渲染 antd Menu；图标名字符串 → React 节点。
import { useMemo } from 'react'
import { Menu } from 'antd'
import type { MenuProps } from 'antd'
import { useLocation, useNavigate } from 'react-router-dom'
import {
  AppstoreOutlined,
  DashboardOutlined,
  SettingOutlined,
  TeamOutlined,
  UserOutlined,
  MenuOutlined,
  SafetyOutlined,
  ProfileOutlined,
} from '@ant-design/icons'
import type { Menu as RbacMenu } from '../api/types'

// 支持后端下发的 PascalCase 名（如 DashboardOutlined）或小写别名
const iconMap: Record<string, React.ReactNode> = {
  DashboardOutlined: <DashboardOutlined />,
  SettingOutlined: <SettingOutlined />,
  TeamOutlined: <TeamOutlined />,
  UserOutlined: <UserOutlined />,
  MenuOutlined: <MenuOutlined />,
  SafetyOutlined: <SafetyOutlined />,
  ProfileOutlined: <ProfileOutlined />,
  // 小写别名
  dashboard: <DashboardOutlined />,
  setting: <SettingOutlined />,
  user: <UserOutlined />,
  team: <TeamOutlined />,
  menu: <MenuOutlined />,
  safety: <SafetyOutlined />,
  profile: <ProfileOutlined />,
}

type ItemType = NonNullable<MenuProps['items']>[number]

function toItem(m: RbacMenu): ItemType {
  const icon = m.icon ? iconMap[m.icon] ?? <AppstoreOutlined /> : undefined
  const hasChildren = m.children && m.children.length > 0
  if (hasChildren) {
    return {
      key: m.path || `g-${m.id}`,
      icon,
      label: m.name,
      children: m.children!.filter((c) => c.visible !== 0).map(toItem),
    } as ItemType
  }
  return {
    key: m.path || `m-${m.id}`,
    icon,
    label: m.name,
  } as ItemType
}

// 根据当前 pathname 回溯选中的 leaf key + 展开的父 key。
function findSelection(menus: RbacMenu[], pathname: string): { selected: string[]; opened: string[] } {
  const opened: string[] = []
  let selected: string[] = []
  const walk = (list: RbacMenu[], parents: string[]): boolean => {
    for (const m of list) {
      const key = m.path || `m-${m.id}`
      if (m.children && m.children.length) {
        const gkey = m.path || `g-${m.id}`
        if (walk(m.children, [...parents, gkey])) {
          opened.push(gkey)
          return true
        }
      } else if (m.path && pathname.startsWith(m.path)) {
        selected = [key]
        opened.push(...parents)
        return true
      }
    }
    return false
  }
  walk(menus, [])
  return { selected, opened }
}

type Props = {
  menus: RbacMenu[]
  mode?: MenuProps['mode']
  theme?: MenuProps['theme']
  inlineCollapsed?: boolean
}

export default function DynamicMenu({ menus, mode = 'inline', theme = 'dark', inlineCollapsed }: Props) {
  const navigate = useNavigate()
  const location = useLocation()

  const items = useMemo(
    () => menus.filter((m) => m.visible !== 0).map(toItem),
    [menus],
  )
  const { selected, opened } = useMemo(
    () => findSelection(menus, location.pathname),
    [menus, location.pathname],
  )

  return (
    <Menu
      mode={mode}
      theme={theme}
      inlineCollapsed={inlineCollapsed}
      items={items}
      selectedKeys={selected}
      defaultOpenKeys={opened}
      onClick={(info) => {
        if (typeof info.key === 'string' && info.key.startsWith('/')) navigate(info.key)
      }}
    />
  )
}
