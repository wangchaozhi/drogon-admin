// 403 无权限页
import { Button, Result } from 'antd'
import { useNavigate } from 'react-router-dom'

export default function ForbiddenPage() {
  const navigate = useNavigate()
  return (
    <Result
      status="403"
      title="403"
      subTitle="抱歉，你没有访问此页面的权限。"
      extra={
        <Button type="primary" onClick={() => navigate('/dashboard')}>返回首页</Button>
      }
    />
  )
}
