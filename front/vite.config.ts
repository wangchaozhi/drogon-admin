import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    proxy: {
      // 把 /api 透传到后端 Drogon 服务，避免跨域 / 端口处理
      '/api': {
        // target: 'http://192.168.159.128:8080',
        target: 'http://127.0.0.1:8080',
        changeOrigin: true,
      },
    },
  },
})
