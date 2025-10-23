import React from "react";

declare global {
  interface Window {
    Native?: {
      getVersion?: () => Promise<string>;
    };
  }
}

export function App() {
  const [version, setVersion] = React.useState<string>("");

  React.useEffect(() => {
    if (window.Native?.getVersion) {
      window.Native.getVersion()
        .then(setVersion)
        .catch(() => setVersion(""));
    }
  }, []);

  // Regular browser content
  return (
    <div
      style={{
        fontFamily: "system-ui, sans-serif",
        minHeight: "100vh",
        margin: 0,
        position: "relative",
      }}
    >
      {/* Main Content Area */}
      <div
        style={{
          marginLeft: 0,
          transition: "margin-left 0.3s ease",
          padding: 24,
          minHeight: "100vh",
          backgroundColor: "#f5f5f5",
        }}
      >
        {/* Main Content */}
        <div
          style={{
            backgroundColor: "white",
            borderRadius: "8px",
            padding: "24px",
            boxShadow: "0 1px 3px rgba(0,0,0,0.1)",
          }}
        >
          <h1>Athena Browser</h1>
          <p>Vite + React scaffold is running.</p>
          {version && <p>Native version: {version}</p>}
          <p style={{ color: "#666" }}>URL: {window.location.href}</p>

          <div style={{ marginTop: "30px" }}>
            <h2>Content Area</h2>
            <p>This is the main content area.</p>
          </div>
        </div>
      </div>
    </div>
  );
}
