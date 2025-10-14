/**
 * Type definitions for Athena Agent
 */

// ============================================================================
// Configuration Types
// ============================================================================

export interface AthenaAgentConfig {
  /** Unix socket path for IPC */
  socketPath: string;
  /** Current working directory */
  cwd: string;
  /** Claude model to use */
  model: string;
  /** Permission mode for Claude operations */
  permissionMode: 'default' | 'acceptEdits' | 'bypassPermissions' | 'plan';
  /** Log level */
  logLevel: 'debug' | 'info' | 'warn' | 'error';
  /** Anthropic API key */
  apiKey?: string;
  /** Max thinking tokens */
  maxThinkingTokens?: number;
  /** Max conversation turns */
  maxTurns?: number;
}

// ============================================================================
// Chat Types
// ============================================================================

export interface ChatMessage {
  role: 'user' | 'assistant';
  content: string;
  timestamp: number;
}

export interface ChatSession {
  id: string;
  messages: ChatMessage[];
  createdAt: number;
  updatedAt: number;
}

export interface ChatRequest {
  message: string;
  sessionId?: string;
}

export interface ChatResponse {
  success: boolean;
  response: string;
  sessionId: string;
  usage?: TokenUsage;
  cost?: number;
  durationMs?: number;
  error?: string;
}

export interface StreamChunk {
  type: 'chunk' | 'done' | 'error';
  content?: string;
  sessionId?: string;
  error?: string;
}

export interface TokenUsage {
  input_tokens: number;
  output_tokens: number;
  cache_creation_input_tokens?: number;
  cache_read_input_tokens?: number;
}

// ============================================================================
// MCP Types
// ============================================================================

export interface BrowserNavigateInput {
  url: string;
  tabIndex?: number;
}

export interface BrowserBackInput {
  tabIndex?: number;
}

export interface BrowserForwardInput {
  tabIndex?: number;
}

export interface BrowserReloadInput {
  tabIndex?: number;
  ignoreCache?: boolean;
}

export interface BrowserGetUrlInput {
  tabIndex?: number;
}

export interface BrowserScreenshotInput {
  tabIndex?: number;
  fullPage?: boolean;
}

export interface BrowserExecuteJsInput {
  code: string;
  tabIndex?: number;
}

export interface BrowserGetHtmlInput {
  tabIndex?: number;
}

export interface WindowCreateTabInput {
  url: string;
}

export interface WindowCloseTabInput {
  tabIndex: number;
}

export interface WindowSwitchTabInput {
  tabIndex: number;
}

export interface MCPToolResult {
  content: Array<{
    type: 'text' | 'image' | 'resource';
    text?: string;
    data?: string;
    mimeType?: string;
  }>;
  isError?: boolean;
}

// ============================================================================
// API Response Types
// ============================================================================

export interface HealthResponse {
  status: 'healthy';
  ready: boolean;
  uptime: number;
  version: string;
  features: string[];
}

export interface CapabilitiesResponse {
  version: string;
  features: string[];
  mcp_tools: string[];
  model: string;
}

export interface ErrorResponse {
  error: string;
  message: string;
  requestId?: string;
}

// ============================================================================
// Logging Types
// ============================================================================

export interface LogEntry {
  timestamp: string;
  level: string;
  module: string;
  message: string;
  requestId?: string;
  [key: string]: any;
}

// ============================================================================
// Browser Control Types (for C++ communication)
// ============================================================================

export interface BrowserControlRequest {
  action: 'navigate' | 'back' | 'forward' | 'reload' | 'get_url' |
          'screenshot' | 'execute_js' | 'get_html' | 'create_tab' |
          'close_tab' | 'switch_tab';
  params?: Record<string, any>;
}

export interface BrowserControlResponse {
  success: boolean;
  result?: any;
  error?: string;
}
