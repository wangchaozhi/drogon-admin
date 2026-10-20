import { render, screen } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { MemoryRouter, Route, Routes, useLocation } from 'react-router-dom'
import { describe, expect, it, vi } from 'vitest'
import { AuthContext, type AuthValue } from '../context/AuthContext'
import type { User } from '../api/types'
import ProtectedRoute from './ProtectedRoute'

const user: User = {
  id: 1,
  name: 'Admin',
  email: 'admin@example.com',
  disabled: false,
  locked_until: 0,
  created_at: 1,
}

function authValue(overrides: Partial<AuthValue> = {}): AuthValue {
  return {
    user,
    roles: ['admin'],
    permissions: [],
    menus: [],
    loading: false,
    error: null,
    login: vi.fn() as unknown as AuthValue['login'],
    logout: vi.fn(),
    refresh: vi.fn() as unknown as AuthValue['refresh'],
    hasPerm: vi.fn(() => true),
    hasRole: vi.fn(() => true),
    ...overrides,
  }
}

function LoginTarget() {
  const location = useLocation()
  const from = (location.state as { from?: string } | null)?.from
  return <div>Login from {from}</div>
}

function renderRoute(value: AuthValue, routeProps: { perm?: string; role?: string } = {}) {
  render(
    <AuthContext.Provider value={value}>
      <MemoryRouter initialEntries={['/system/users']}>
        <Routes>
          <Route element={<ProtectedRoute {...routeProps} />}>
            <Route path="/system/users" element={<div>User administration</div>} />
          </Route>
          <Route path="/login" element={<LoginTarget />} />
          <Route path="/403" element={<div>Forbidden target</div>} />
        </Routes>
      </MemoryRouter>
    </AuthContext.Provider>,
  )
}

describe('ProtectedRoute', () => {
  it('redirects anonymous users to login and preserves the source path', () => {
    renderRoute(authValue({ user: null, roles: [] }))
    expect(screen.getByText('Login from /system/users')).toBeVisible()
  })

  it('redirects authenticated users without permission to forbidden', () => {
    renderRoute(
      authValue({ hasPerm: vi.fn(() => false) }),
      { perm: 'user:view' },
    )
    expect(screen.getByText('Forbidden target')).toBeVisible()
  })

  it('offers a retry instead of logging out on a transient restore error', async () => {
    const refresh = vi.fn().mockResolvedValue(undefined)
    renderRoute(authValue({
      user: null,
      roles: [],
      error: 'network unavailable',
      refresh,
    }))

    expect(screen.getByText('暂时无法恢复登录状态')).toBeVisible()
    await userEvent.click(screen.getByRole('button', { name: '重试恢复登录状态' }))
    expect(refresh).toHaveBeenCalledOnce()
  })

  it('renders the nested route when permission and role checks pass', () => {
    renderRoute(authValue(), { perm: 'user:view', role: 'admin' })
    expect(screen.getByText('User administration')).toBeVisible()
  })
})
