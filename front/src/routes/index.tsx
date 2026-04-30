// 根路由：BrowserRouter + 基于权限的嵌套路由。
import { BrowserRouter, Navigate, Route, Routes } from 'react-router-dom'
import AdminLayout from '../layouts/AdminLayout'
import ProtectedRoute from './ProtectedRoute'
import LoginPage from '../pages/auth/LoginPage'
import DashboardPage from '../pages/dashboard/DashboardPage'
import ProfilePage from '../pages/profile/ProfilePage'
import UserListPage from '../pages/admin/users/UserListPage'
import RoleListPage from '../pages/admin/roles/RoleListPage'
import MenuListPage from '../pages/admin/menus/MenuListPage'
import ForbiddenPage from '../pages/error/ForbiddenPage'
import NotFoundPage from '../pages/error/NotFoundPage'

export default function AppRouter() {
  return (
    <BrowserRouter>
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
    </BrowserRouter>
  )
}
