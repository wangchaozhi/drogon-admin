// Ant Design 主题配置集中管理。
// 品牌蓝 #2f54eb，暗色 Sider，柔和圆角，现代字体栈。
import type { ThemeConfig } from 'antd'
import { theme } from 'antd'

export const appTheme: ThemeConfig = {
  algorithm: theme.defaultAlgorithm,
  token: {
    colorPrimary: '#2f54eb',
    colorInfo: '#2f54eb',
    colorSuccess: '#52c41a',
    borderRadius: 10,
    wireframe: false,
    fontFamily:
      "system-ui, -apple-system, 'Segoe UI', Roboto, 'PingFang SC', 'Microsoft YaHei', Arial, sans-serif",
  },
  components: {
    Layout: {
      headerBg: 'rgba(255,255,255,0.85)',
      bodyBg: '#f4f6fb',
      siderBg: '#0f1c3f',
      triggerBg: '#0a1530',
    },
    Menu: {
      darkItemBg: '#0f1c3f',
      darkSubMenuItemBg: '#0b1633',
      darkItemSelectedBg: '#2f54eb',
      darkItemHoverBg: 'rgba(47,84,235,0.25)',
      itemBorderRadius: 8,
    },
    Card: {
      borderRadiusLG: 14,
      boxShadowTertiary: '0 2px 8px rgba(15,28,63,0.06)',
    },
    Button: {
      controlHeight: 36,
      borderRadius: 8,
    },
    Table: {
      headerBg: '#f0f3fa',
      headerColor: '#1f2a4a',
      rowHoverBg: '#f5f8ff',
      borderRadius: 10,
    },
    Statistic: {
      titleFontSize: 13,
    },
  },
}
