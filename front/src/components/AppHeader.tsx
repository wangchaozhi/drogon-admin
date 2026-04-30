// 响应式顶部栏：桌面显示完整昵称 + 文字按钮；手机只显示头像图标 + 图标按钮。
import { Avatar, Button, Space, Typography } from 'antd'
import { LogoutOutlined, UserOutlined } from '@ant-design/icons'
import { useDevice } from '../hooks/useDevice'
import type { User } from '../api'

const { Title, Text } = Typography

type Props = {
  user: User
  onLogout: () => void
}

export default function AppHeader({ user, onLogout }: Props) {
  const { isMobile } = useDevice()

  return (
    <div className="app-header">
      <Title
        level={isMobile ? 5 : 4}
        style={{ margin: 0, whiteSpace: 'nowrap' }}
      >
        c_web {isMobile ? '' : '控制台'}
      </Title>

      <Space size={isMobile ? 8 : 16}>
        <Space size={6}>
          <Avatar size={isMobile ? 'small' : 'default'} icon={<UserOutlined />} />
          {!isMobile && <Text strong>{user.name}</Text>}
        </Space>

        <Button
          icon={<LogoutOutlined />}
          size={isMobile ? 'small' : 'middle'}
          onClick={onLogout}
        >
          {isMobile ? '' : '退出登录'}
        </Button>
      </Space>
    </div>
  )
}
