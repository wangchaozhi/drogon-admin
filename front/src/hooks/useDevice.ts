// 响应式断点 hook：基于 antd Grid 判断当前是手机 / 平板 / 桌面。
// Ant Design 断点:
//   xs < 576   sm ≥ 576   md ≥ 768   lg ≥ 992   xl ≥ 1200   xxl ≥ 1600
import { Grid } from 'antd'

const { useBreakpoint } = Grid

export type DeviceKind = 'mobile' | 'tablet' | 'desktop'

export function useDevice(): {
  kind: DeviceKind
  isMobile: boolean
  isTablet: boolean
  isDesktop: boolean
} {
  const screens = useBreakpoint()

  // md 开始算平板（≥768），lg 开始算桌面（≥992）
  const isDesktop = !!screens.lg
  const isTablet = !isDesktop && !!screens.md
  const isMobile = !isDesktop && !isTablet

  const kind: DeviceKind = isDesktop ? 'desktop' : isTablet ? 'tablet' : 'mobile'
  return { kind, isMobile, isTablet, isDesktop }
}
