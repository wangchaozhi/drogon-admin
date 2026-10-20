import { Component, type ErrorInfo, type ReactNode } from 'react'

type Props = {
  children: ReactNode
  onError?: (error: Error, info: ErrorInfo) => void
  onReset?: () => void
}

type State = {
  failed: boolean
}

export default class AppErrorBoundary extends Component<Props, State> {
  state: State = { failed: false }

  static getDerivedStateFromError(): State {
    return { failed: true }
  }

  componentDidCatch(error: Error, info: ErrorInfo) {
    if (this.props.onError) {
      this.props.onError(error, info)
      return
    }
    console.error('Uncaught application error', error, info.componentStack)
  }

  private reset = () => {
    if (this.props.onReset) {
      this.props.onReset()
      return
    }
    window.location.reload()
  }

  render() {
    if (!this.state.failed) return this.props.children

    return (
      <main
        role="alert"
        style={{
          minHeight: '100vh',
          display: 'grid',
          placeItems: 'center',
          padding: 24,
          background: '#f5f7fa',
        }}
      >
        <section style={{ maxWidth: 520, textAlign: 'center' }}>
          <h1>页面暂时无法显示</h1>
          <p>应用遇到了意外错误，请刷新后重试。</p>
          <button type="button" onClick={this.reset}>
            刷新页面
          </button>
        </section>
      </main>
    )
  }
}
