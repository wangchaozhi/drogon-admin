import { render, screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { AuthProvider } from './AuthContext'
import { useAuth } from '../hooks/useAuth'

const authMocks = vi.hoisted(() => ({
  getToken: vi.fn(),
  saveToken: vi.fn(),
  clear: vi.fn(),
  me: vi.fn(),
  login: vi.fn(),
}))

vi.mock('../api/auth', () => ({
  auth: {
    getToken: authMocks.getToken,
    saveToken: authMocks.saveToken,
    clear: authMocks.clear,
  },
  authApi: {
    me: authMocks.me,
    login: authMocks.login,
  },
}))

const user = {
  id: 1,
  name: 'Admin',
  email: 'admin@example.com',
  disabled: false,
  locked_until: 0,
  created_at: 1,
}

function Probe() {
  const { user: currentUser, error, refresh, hasPerm } = useAuth()
  return (
    <div>
      <span>{currentUser?.email ?? 'anonymous'}</span>
      <span>{error ?? 'no-error'}</span>
      <span>{hasPerm('user:delete') ? 'can-delete' : 'cannot-delete'}</span>
      <button type="button" onClick={() => void refresh().catch(() => {})}>
        Refresh session
      </button>
    </div>
  )
}

describe('AuthProvider', () => {
  beforeEach(() => {
    authMocks.getToken.mockReset()
    authMocks.saveToken.mockReset()
    authMocks.clear.mockReset()
    authMocks.me.mockReset()
    authMocks.login.mockReset()
  })

  it('keeps the token after a transient restore failure and can retry', async () => {
    authMocks.getToken.mockReturnValue('existing-token')
    authMocks.me
      .mockRejectedValueOnce(new Error('network unavailable'))
      .mockResolvedValueOnce({
        data: {
          user,
          roles: ['admin'],
          permissions: [],
          menus: [],
        },
      })

    render(
      <AuthProvider>
        <Probe />
      </AuthProvider>,
    )

    expect(await screen.findByText('network unavailable')).toBeVisible()
    expect(authMocks.clear).not.toHaveBeenCalled()

    await userEvent.click(screen.getByRole('button', { name: 'Refresh session' }))
    await waitFor(() => {
      expect(screen.getByText('admin@example.com')).toBeVisible()
    })
    expect(screen.getByText('can-delete')).toBeVisible()
    expect(screen.getByText('no-error')).toBeVisible()
  })
})
