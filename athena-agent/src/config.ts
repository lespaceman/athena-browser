/**
 * Configuration management for Athena Agent
 */

import type { AthenaAgentConfig } from './types.js';

/**
 * Load configuration from environment variables and defaults
 */
export function loadConfig(): AthenaAgentConfig {
  const uid = process.getuid?.() ?? 1000;

  const config: AthenaAgentConfig = {
    socketPath: process.env.ATHENA_SOCKET_PATH || `/tmp/athena-${uid}.sock`,
    cwd: process.env.CWD || process.cwd(),
    model: process.env.CLAUDE_MODEL || 'claude-haiku-4-5-20251001',
    permissionMode: (process.env.PERMISSION_MODE as any) || 'bypassPermissions',
    logLevel: (process.env.LOG_LEVEL as any) || 'info',
    apiKey: process.env.ANTHROPIC_API_KEY,
    maxThinkingTokens: process.env.MAX_THINKING_TOKENS ? parseInt(process.env.MAX_THINKING_TOKENS, 10) : undefined,
    maxTurns: process.env.MAX_TURNS ? parseInt(process.env.MAX_TURNS, 10) : undefined
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
