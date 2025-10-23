import React from 'react'

interface SidebarProps {
  isOpen: boolean
  onToggle: () => void
}

export function Sidebar({ isOpen, onToggle }: SidebarProps) {
  return (
    <>
      {/* Sidebar Container */}
      <div
        style={{
          position: 'fixed',
          top: 0,
          left: isOpen ? 0 : -280,
          width: 280,
          height: '100vh',
          backgroundColor: '#1e1e1e',
          color: '#ffffff',
          transition: 'left 0.3s ease',
          zIndex: 1000,
          display: 'flex',
          flexDirection: 'column',
          boxShadow: isOpen ? '2px 0 8px rgba(0,0,0,0.2)' : 'none',
        }}
      >
        {/* Sidebar Header */}
        <div
          style={{
            padding: '20px',
            borderBottom: '1px solid #333',
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
          }}
        >
          <h2 style={{ margin: 0, fontSize: '18px' }}>Sidebar</h2>
          <button
            onClick={onToggle}
            style={{
              background: 'transparent',
              border: 'none',
              color: '#999',
              fontSize: '24px',
              cursor: 'pointer',
              padding: 0,
              width: '30px',
              height: '30px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
            }}
            aria-label="Close sidebar"
          >
            ×
          </button>
        </div>

        {/* Sidebar Content */}
        <div
          style={{
            flex: 1,
            padding: '20px',
            overflowY: 'auto',
          }}
        >
          {/* Navigation Items */}
          <nav>
            <ul
              style={{
                listStyle: 'none',
                padding: 0,
                margin: 0,
              }}
            >
              <li style={{ marginBottom: '10px' }}>
                <a
                  href="#"
                  style={{
                    color: '#ffffff',
                    textDecoration: 'none',
                    display: 'block',
                    padding: '10px',
                    borderRadius: '4px',
                    transition: 'background-color 0.2s',
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = '#333'
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = 'transparent'
                  }}
                >
                  🏠 Home
                </a>
              </li>
              <li style={{ marginBottom: '10px' }}>
                <a
                  href="#"
                  style={{
                    color: '#ffffff',
                    textDecoration: 'none',
                    display: 'block',
                    padding: '10px',
                    borderRadius: '4px',
                    transition: 'background-color 0.2s',
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = '#333'
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = 'transparent'
                  }}
                >
                  📑 Bookmarks
                </a>
              </li>
              <li style={{ marginBottom: '10px' }}>
                <a
                  href="#"
                  style={{
                    color: '#ffffff',
                    textDecoration: 'none',
                    display: 'block',
                    padding: '10px',
                    borderRadius: '4px',
                    transition: 'background-color 0.2s',
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = '#333'
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = 'transparent'
                  }}
                >
                  📜 History
                </a>
              </li>
              <li style={{ marginBottom: '10px' }}>
                <a
                  href="#"
                  style={{
                    color: '#ffffff',
                    textDecoration: 'none',
                    display: 'block',
                    padding: '10px',
                    borderRadius: '4px',
                    transition: 'background-color 0.2s',
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = '#333'
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = 'transparent'
                  }}
                >
                  ⬇️ Downloads
                </a>
              </li>
              <li style={{ marginBottom: '10px' }}>
                <a
                  href="#"
                  style={{
                    color: '#ffffff',
                    textDecoration: 'none',
                    display: 'block',
                    padding: '10px',
                    borderRadius: '4px',
                    transition: 'background-color 0.2s',
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = '#333'
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = 'transparent'
                  }}
                >
                  ⚙️ Settings
                </a>
              </li>
            </ul>
          </nav>

          {/* Additional Content Section */}
          <div
            style={{
              marginTop: '40px',
              padding: '20px',
              backgroundColor: '#2a2a2a',
              borderRadius: '8px',
            }}
          >
            <h3 style={{ fontSize: '14px', marginBottom: '10px' }}>Quick Actions</h3>
            <button
              style={{
                width: '100%',
                padding: '10px',
                marginBottom: '10px',
                backgroundColor: '#0066cc',
                color: 'white',
                border: 'none',
                borderRadius: '4px',
                cursor: 'pointer',
                transition: 'background-color 0.2s',
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.backgroundColor = '#0052a3'
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.backgroundColor = '#0066cc'
              }}
            >
              New Tab
            </button>
            <button
              style={{
                width: '100%',
                padding: '10px',
                backgroundColor: '#333',
                color: 'white',
                border: 'none',
                borderRadius: '4px',
                cursor: 'pointer',
                transition: 'background-color 0.2s',
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.backgroundColor = '#444'
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.backgroundColor = '#333'
              }}
            >
              Private Mode
            </button>
          </div>
        </div>

        {/* Sidebar Footer */}
        <div
          style={{
            padding: '20px',
            borderTop: '1px solid #333',
            fontSize: '12px',
            color: '#666',
          }}
        >
          Athena Browser v0.1.0
        </div>
      </div>

      {/* Toggle Button (visible when sidebar is closed) */}
      {!isOpen && (
        <button
          onClick={onToggle}
          style={{
            position: 'fixed',
            top: '20px',
            left: '20px',
            width: '40px',
            height: '40px',
            backgroundColor: '#1e1e1e',
            color: 'white',
            border: 'none',
            borderRadius: '4px',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            fontSize: '20px',
            zIndex: 999,
            boxShadow: '0 2px 4px rgba(0,0,0,0.2)',
            transition: 'background-color 0.2s',
          }}
          aria-label="Open sidebar"
          onMouseEnter={(e) => {
            e.currentTarget.style.backgroundColor = '#333'
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = '#1e1e1e'
          }}
        >
          ☰
        </button>
      )}
    </>
  )
}