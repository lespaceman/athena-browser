import React from 'react'
import { Sidebar } from './components/Sidebar'

declare global {
  interface Window {
    Native?: {
      getVersion?: () => Promise<string>
    }
  }
}

export function App() {
  const [version, setVersion] = React.useState<string>('')
  const [sidebarOpen, setSidebarOpen] = React.useState(false)
  const isSidebar = window.location.hash === '#sidebar'

  React.useEffect(() => {
    if (window.Native?.getVersion) {
      window.Native.getVersion().then(setVersion).catch(() => setVersion(''))
    }
  }, [])

  const toggleSidebar = () => {
    setSidebarOpen(!sidebarOpen)
  }

  // If this is the sidebar window, show only the sidebar content
  if (isSidebar) {
    return (
      <div style={{ 
        fontFamily: 'system-ui, sans-serif',
        minHeight: '100vh',
        margin: 0,
        backgroundColor: '#1e1e1e',
        color: 'white',
        padding: '20px'
      }}>
        <h2>Browser Sidebar</h2>
        <p style={{ marginTop: '20px', color: '#999' }}>
          This is a separate browser window - part of the browser chrome, not the web content!
        </p>
        <div style={{ marginTop: '30px' }}>
          <button style={{ 
            display: 'block',
            width: '100%',
            padding: '10px',
            marginBottom: '10px',
            backgroundColor: '#0066cc',
            color: 'white',
            border: 'none',
            borderRadius: '4px',
            cursor: 'pointer'
          }}>New Tab</button>
          <button style={{ 
            display: 'block',
            width: '100%',
            padding: '10px',
            marginBottom: '10px',
            backgroundColor: '#333',
            color: 'white',
            border: 'none',
            borderRadius: '4px',
            cursor: 'pointer'
          }}>Bookmarks</button>
          <button style={{ 
            display: 'block',
            width: '100%',
            padding: '10px',
            backgroundColor: '#333',
            color: 'white',
            border: 'none',
            borderRadius: '4px',
            cursor: 'pointer'
          }}>History</button>
        </div>
        <p style={{ marginTop: '30px', fontSize: '12px', color: '#666' }}>
          Window: SIDEBAR | PID: {window.location.port || 'N/A'}
        </p>
      </div>
    )
  }

  // Regular browser content
  return (
    <div style={{ 
      fontFamily: 'system-ui, sans-serif',
      minHeight: '100vh',
      margin: 0,
      position: 'relative'
    }}>
      {/* Sidebar Component */}
      <Sidebar isOpen={sidebarOpen} onToggle={toggleSidebar} />
      
      {/* Main Content Area */}
      <div 
        style={{ 
          marginLeft: sidebarOpen ? 280 : 0,
          transition: 'margin-left 0.3s ease',
          padding: 24,
          minHeight: '100vh',
          backgroundColor: '#f5f5f5'
        }}
      >
        {/* Header with Tab-like Navigation (placeholder for tabs) */}
        <div style={{
          backgroundColor: 'white',
          borderRadius: '8px',
          padding: '16px',
          marginBottom: '20px',
          boxShadow: '0 1px 3px rgba(0,0,0,0.1)'
        }}>
          <div style={{ 
            display: 'flex', 
            gap: '10px',
            borderBottom: '1px solid #e0e0e0',
            paddingBottom: '10px'
          }}>
            <div style={{
              padding: '8px 16px',
              backgroundColor: '#0066cc',
              color: 'white',
              borderRadius: '4px 4px 0 0',
              cursor: 'pointer'
            }}>
              Tab 1
            </div>
            <div style={{
              padding: '8px 16px',
              backgroundColor: '#f0f0f0',
              borderRadius: '4px 4px 0 0',
              cursor: 'pointer'
            }}>
              Tab 2
            </div>
            <div style={{
              padding: '8px 16px',
              backgroundColor: '#f0f0f0',
              borderRadius: '4px 4px 0 0',
              cursor: 'pointer'
            }}>
              Tab 3
            </div>
          </div>
        </div>

        {/* Main Content */}
        <div style={{
          backgroundColor: 'white',
          borderRadius: '8px',
          padding: '24px',
          boxShadow: '0 1px 3px rgba(0,0,0,0.1)'
        }}>
          <h1>Athena Browser</h1>
          <p>Vite + React scaffold is running with sidebar support.</p>
          {version && <p>Native version: {version}</p>}
          <p style={{ color: '#666' }}>URL: {window.location.href}</p>
          
          <div style={{ marginTop: '30px' }}>
            <h2>Content Area</h2>
            <p>This is the main content area. The sidebar is independent from the tabs and can be toggled open/closed.</p>
            <p>Click the hamburger menu button in the top-left to toggle the sidebar.</p>
          </div>
        </div>
      </div>
    </div>
  )
}

