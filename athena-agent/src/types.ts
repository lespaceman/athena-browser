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
  /** Screenshot operation timeout in milliseconds (default: 90000 = 90s) */
  screenshotTimeoutMs?: number;
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
  /** Image quality (1-100, default: 85). Lower values reduce file size and transfer time. */
  quality?: number;
  /** Maximum width in pixels. Image will be scaled down proportionally if larger. */
  maxWidth?: number;
  /** Maximum height in pixels. Image will be scaled down proportionally if larger. */
  maxHeight?: number;
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
  result?: unknown;
  error?: string;
}

// Browser API Response Types
export interface BrowserApiResponse {
  success?: boolean;
  error?: string;
  message?: string;
  [key: string]: unknown;
}

export interface PageSummary {
  title: string;
  url: string;
  headings: string[];
  forms: number;
  links: number;
  buttons: number;
  inputs: number;
  images: number;
  mainText: string;
}

export interface InteractiveElement {
  index: number;
  tag: string;
  type: string;
  id?: string;
  className?: string;
  text?: string;
  href?: string;
  name?: string;
  placeholder?: string;
  value?: string;
  ariaLabel?: string;
  role?: string;
  disabled?: boolean;
  checked?: boolean;
  bounds: {
    x: number;
    y: number;
    width: number;
    height: number;
  };
}

export interface AccessibilityNode {
  role: string;
  name?: string;
  level?: number;
  children?: AccessibilityNode[];
}

export interface AnnotatedScreenshotElement {
  index: number;
  x: number;
  y: number;
  width: number;
  height: number;
  tag: string;
  text: string;
  type: string;
}
