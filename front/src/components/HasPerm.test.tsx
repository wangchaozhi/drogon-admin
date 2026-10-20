import { render, screen } from '@testing-library/react'
import { describe, expect, it, vi } from 'vitest'
import { AuthContext, type AuthValue } from '../context/AuthContext'
import HasPerm from './HasPerm'

function authValue(hasPermission: boolean): AuthValue {
  return {
    user: null,
    roles: [],
    permissions: [],
    menus: [],
    loading: false,
    login: vi.fn(),
    logout: vi.fn(),
    refresh: vi.fn(),
    hasPerm: vi.fn(() => hasPermission),
    hasRole: vi.fn(() => false),
  }
}

describe('HasPerm', () => {
  it('renders protected content when permission is granted', () => {
    render(
      <AuthContext.Provider value={authValue(true)}>
        <HasPerm code="user:create">
          <button type="button">Create user</button>
        </HasPerm>
      </AuthContext.Provider>,
    )

    expect(screen.getByRole('button', { name: 'Create user' })).toBeVisible()
  })

  it('renders fallback when permission is denied', () => {
    render(
      <AuthContext.Provider value={authValue(false)}>
        <HasPerm code={['user:update', 'user:delete']} fallback={<span>Denied</span>}>
          <button type="button">Edit user</button>
        </HasPerm>
      </AuthContext.Provider>,
    )

    expect(screen.queryByRole('button', { name: 'Edit user' })).not.toBeInTheDocument()
    expect(screen.getByText('Denied')).toBeVisible()
  })
})
