/**
 * Configuration management for Athena Agent
 */

import type { AthenaAgentConfig } from './types';

/**
 * Load configuration from environment variables and defaults
 */
export function loadConfig(): AthenaAgentConfig {
  const uid = process.getuid?.() ?? 1000;

  const config: AthenaAgentConfig = {
    socketPath: process.env.ATHENA_SOCKET_PATH || `/tmp/athena-${uid}.sock`,
    cwd: process.env.CWD || process.cwd(),
    // Use Sonnet 4.5 by default for better reasoning and tool use
    model: process.env.CLAUDE_MODEL || 'claude-sonnet-4-5',
    permissionMode: (process.env.PERMISSION_MODE as any) || 'default',
    logLevel: (process.env.LOG_LEVEL as any) || 'info',
    apiKey: process.env.ANTHROPIC_API_KEY,
    // Enable extended thinking for complex browser automation tasks
    maxThinkingTokens: process.env.MAX_THINKING_TOKENS ? parseInt(process.env.MAX_THINKING_TOKENS, 10) : 8000,
    // Limit conversation turns to prevent runaway costs
    maxTurns: process.env.MAX_TURNS ? parseInt(process.env.MAX_TURNS, 10) : 20,
    // Screenshot timeout: large screenshots (1-5MB base64) need more time to transfer
    screenshotTimeoutMs: process.env.SCREENSHOT_TIMEOUT_MS ? parseInt(process.env.SCREENSHOT_TIMEOUT_MS, 10) : 90000
  };

  return config;
}

/**
 * Validate configuration
 * Note: API key is not required at startup - it will be checked when chat is first used
 */
export function validateConfig(config: AthenaAgentConfig): void {
  if (!config.socketPath) {
    throw new Error('Socket path is required');
  }

  if (!config.cwd) {
    throw new Error('Working directory is required');
  }
}

// Export singleton config
export const config = loadConfig();
