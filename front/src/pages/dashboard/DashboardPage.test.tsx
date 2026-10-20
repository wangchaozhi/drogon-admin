import { render, screen } from '@testing-library/react'
import { beforeEach, describe, expect, it, vi } from 'vitest'
import DashboardPage from './DashboardPage'

const mocks = vi.hoisted(() => ({
  stats: vi.fn(),
  auth: {
    user: {
      id: 1,
      name: 'Admin',
      email: 'admin@example.com',
      disabled: false,
      locked_until: 0,
      created_at: 1,
    },
    roles: ['admin'],
    permissions: ['dashboard:view', 'dashboard:stats'],
    menus: [{ id: 1 }],
    hasPerm: vi.fn((code: string) => code === 'dashboard:stats'),
  },
}))

vi.mock('../../api/dashboard', () => ({
  dashboardApi: { stats: mocks.stats },
}))

vi.mock('../../hooks/useAuth', () => ({
  useAuth: () => mocks.auth,
}))

describe('DashboardPage', () => {
  beforeEach(() => {
    mocks.stats.mockReset()
    mocks.auth.hasPerm.mockReset()
    mocks.auth.hasPerm.mockImplementation(
      (code: string) => code === 'dashboard:stats',
    )
  })

  it('loads the protected system overview for an authorized user', async () => {
    mocks.stats.mockResolvedValue({
      data: {
        total_users: 12,
        enabled_users: 10,
        disabled_users: 2,
        locked_users: 1,
        total_roles: 3,
        visible_menus: 7,
        audit_events_24h: 9,
        generated_at: 1_800_000_000,
      },
    })

    render(<DashboardPage />)

    expect(await screen.findByText('系统概览')).toBeVisible()
    expect(await screen.findByText('用户总数')).toBeVisible()
    expect(screen.getByText('近 24 小时操作')).toBeVisible()
    expect(mocks.stats).toHaveBeenCalledTimes(1)
  })

  it('does not request or render system statistics without permission', () => {
    mocks.auth.hasPerm.mockReturnValue(false)

    render(<DashboardPage />)

    expect(screen.queryByText('系统概览')).not.toBeInTheDocument()
    expect(screen.getByText('我的访问范围')).toBeVisible()
    expect(mocks.stats).not.toHaveBeenCalled()
  })
})
