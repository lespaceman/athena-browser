import React from 'react'

declare global {
  interface Window {
    Native?: {
      getVersion?: () => Promise<string>
    }
  }
}

export function App() {
  const [version, setVersion] = React.useState<string>('')

  React.useEffect(() => {
    if (window.Native?.getVersion) {
      window.Native.getVersion().then(setVersion).catch(() => setVersion(''))
    }
  }, [])

  return (
    <div style={{ fontFamily: 'system-ui, sans-serif', padding: 24 }}>
      <h1>Athena Browser</h1>
      <p>Vite + React scaffold is running.</p>
      {version && <p>Native version: {version}</p>}
      <p style={{ color: '#666' }}>URL: {window.location.href}</p>
    </div>
  )
}

