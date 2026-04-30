// 顶层入口：ConfigProvider（主题 + 中文）+ antd App（提供全局 message / modal 上下文） + AuthProvider + AppRoutes。
import { App as AntApp, ConfigProvider } from 'antd'
import zhCN from 'antd/locale/zh_CN'
import AppRoutes from './pages/AppRoutes'
import { AuthProvider } from './context/AuthContext'
import { appTheme } from './theme'
import './App.css'

export default function App() {
  return (
    <ConfigProvider locale={zhCN} theme={appTheme}>
      <AntApp>
        <AuthProvider>
          <AppRoutes />
        </AuthProvider>
      </AntApp>
    </ConfigProvider>
  )
}
