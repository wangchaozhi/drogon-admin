// Ant Design 主题配置集中管理，便于后续主题切换 / 深色模式等扩展。
import type { ThemeConfig } from 'antd'
import { theme } from 'antd'

export const appTheme: ThemeConfig = {
  algorithm: theme.defaultAlgorithm,
  token: {
    colorPrimary: '#1677ff',
    borderRadius: 8,
    fontFamily:
      "system-ui, -apple-system, 'Segoe UI', Roboto, 'PingFang SC', 'Microsoft YaHei', Arial, sans-serif",
  },
  components: {
    Layout: {
      headerBg: '#fff',
      bodyBg: '#f5f7fa',
    },
  },
}
