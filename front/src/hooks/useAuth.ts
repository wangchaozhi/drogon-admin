import { useContext } from 'react'
import { AuthContext } from '../context/AuthContext'
import type { AuthValue } from '../context/AuthContext'

export function useAuth(): AuthValue {
  const ctx = useContext(AuthContext)
  if (!ctx) throw new Error('useAuth must be used within <AuthProvider>')
  return ctx
}
