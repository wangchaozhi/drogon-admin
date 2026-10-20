import { render, screen } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { describe, expect, it, vi } from 'vitest'
import AppErrorBoundary from './AppErrorBoundary'

function BrokenPage(): never {
  throw new Error('render failed')
}

describe('AppErrorBoundary', () => {
  it('shows a recoverable fallback when a child throws', async () => {
    const onError = vi.fn()
    const onReset = vi.fn()
    render(
      <AppErrorBoundary onError={onError} onReset={onReset}>
        <BrokenPage />
      </AppErrorBoundary>,
    )

    expect(screen.getByRole('alert')).toHaveTextContent('页面暂时无法显示')
    expect(onError).toHaveBeenCalledOnce()
    expect(onError.mock.calls[0][0]).toMatchObject({
      message: 'render failed',
    })

    await userEvent.click(screen.getByRole('button', { name: '刷新页面' }))
    expect(onReset).toHaveBeenCalledOnce()
  })
})
