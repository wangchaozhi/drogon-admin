import { render, screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { MemoryRouter } from 'react-router-dom'
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { userApi } from '../../api/user'
import { useAuth } from '../../hooks/useAuth'
import { useDevice } from '../../hooks/useDevice'
import ProfilePage from './ProfilePage'

vi.mock('../../api/user', () => ({
  userApi: {
    changePassword: vi.fn(),
  },
}))
vi.mock('../../hooks/useAuth', () => ({ useAuth: vi.fn() }))
vi.mock('../../hooks/useDevice', () => ({ useDevice: vi.fn() }))

describe('ProfilePage', () => {
  const logout = vi.fn()

  beforeEach(() => {
    vi.clearAllMocks()
    vi.mocked(useDevice).mockReturnValue({
      kind: 'desktop',
      isMobile: false,
      isTablet: false,
      isDesktop: true,
    })
    vi.mocked(useAuth).mockReturnValue({
      user: {
        id: 7,
        name: 'Operator',
        email: 'operator@example.com',
        disabled: false,
        locked_until: 0,
        created_at: 1_700_000_000,
      },
      roles: ['user'],
      permissions: ['dashboard:view'],
      menus: [],
      loading: false,
      error: null,
      login: vi.fn(),
      logout,
      refresh: vi.fn(),
      hasPerm: vi.fn(() => false),
      hasRole: vi.fn(() => false),
    })
    vi.mocked(userApi.changePassword).mockResolvedValue({
      code: 0,
      message: 'ok',
      data: null,
    })
  })

  it('changes the password and clears the local session', async () => {
    const user = userEvent.setup()
    render(
      <MemoryRouter>
        <ProfilePage />
      </MemoryRouter>,
    )

    await user.type(screen.getByLabelText('当前密码'), 'Current-Horse-2026!')
    await user.type(screen.getByLabelText('新密码'), 'Updated-Horse-2026!')
    await user.type(screen.getByLabelText('确认新密码'), 'Updated-Horse-2026!')
    await user.click(screen.getByRole('button', { name: '修改密码' }))

    await waitFor(() => {
      expect(userApi.changePassword).toHaveBeenCalledWith({
        current_password: 'Current-Horse-2026!',
        new_password: 'Updated-Horse-2026!',
      })
      expect(logout).toHaveBeenCalledOnce()
    })
  })
})
