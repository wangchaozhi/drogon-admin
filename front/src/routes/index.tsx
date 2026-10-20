// 根路由：BrowserRouter + 基于权限的嵌套路由。
import { lazy, Suspense } from 'react'
import { BrowserRouter, Navigate, Route, Routes } from 'react-router-dom'

const AdminLayout = lazy(() => import('../layouts/AdminLayout'))
const ProtectedRoute = lazy(() => import('./ProtectedRoute'))
const LoginPage = lazy(() => import('../pages/auth/LoginPage'))
const DashboardPage = lazy(() => import('../pages/dashboard/DashboardPage'))
const ProfilePage = lazy(() => import('../pages/profile/ProfilePage'))
const UserListPage = lazy(() => import('../pages/admin/users/UserListPage'))
const RoleListPage = lazy(() => import('../pages/admin/roles/RoleListPage'))
const MenuListPage = lazy(() => import('../pages/admin/menus/MenuListPage'))
const ForbiddenPage = lazy(() => import('../pages/error/ForbiddenPage'))
const NotFoundPage = lazy(() => import('../pages/error/NotFoundPage'))

function RouteFallback() {
  return (
    <div
      role="status"
      aria-label="正在加载页面"
      style={{
        minHeight: '100vh',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
      }}
    >
      正在加载…
    </div>
  )
}

export default function AppRouter() {
  return (
    <BrowserRouter>
      <Suspense fallback={<RouteFallback />}>
        <Routes>
          <Route path="/login" element={<LoginPage />} />
          <Route path="/403" element={<ForbiddenPage />} />

          <Route element={<ProtectedRoute />}>
            <Route element={<AdminLayout />}>
              <Route path="/" element={<Navigate to="/dashboard" replace />} />
              <Route path="/dashboard" element={<DashboardPage />} />
              <Route path="/profile" element={<ProfilePage />} />

              <Route element={<ProtectedRoute perm="user:view" />}>
                <Route path="/system/users" element={<UserListPage />} />
              </Route>
              <Route element={<ProtectedRoute perm="role:view" />}>
                <Route path="/system/roles" element={<RoleListPage />} />
              </Route>
              <Route element={<ProtectedRoute perm="menu:view" />}>
                <Route path="/system/menus" element={<MenuListPage />} />
              </Route>

              <Route path="*" element={<NotFoundPage />} />
            </Route>
          </Route>
        </Routes>
      </Suspense>
    </BrowserRouter>
  )
}
