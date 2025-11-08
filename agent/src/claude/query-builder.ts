/**
 * Claude Query Builder
 *
 * Builds query options for the Claude SDK, reducing duplication across
 * sendMessage, streamMessage, and forkSession methods.
 */

import type { AthenaAgentConfig } from '../server/types';
import { BrowserPrompts } from './config/prompts';
import { SubAgentConfig } from './config/sub-agents';
import { ToolSelector } from './config/tool-selector';
import { PermissionHandler } from './config/permissions';

export interface QueryConfig {
  cwd: string;
  model: string;
  permissionMode: 'default' | 'acceptEdits' | 'bypassPermissions' | 'plan';
  maxThinkingTokens?: number;
  maxTurns?: number;
  prompt: string;
  claudeSessionId?: string;
  forkSession?: boolean;
  includePartialMessages?: boolean;
  enableMcp?: boolean;  // Whether to enable athena-browser-mcp
}

export interface McpServerConfig {
  command: string;
  args: string[];
  env?: Record<string, string>;
}

export interface QueryOptions {
  cwd: string;
  model: string;
  permissionMode: 'default' | 'acceptEdits' | 'bypassPermissions' | 'plan';
  systemPrompt: { type: 'preset'; preset: 'claude_code' } | string;
  settingSources: ('user' | 'project' | 'local')[];
  allowedTools: string[];
  maxThinkingTokens?: number;
  maxTurns?: number;
  resume?: string;
  forkSession?: boolean;
  mcpServers?: Record<string, McpServerConfig>;
  agents?: any;
  canUseTool?: (toolName: string, input: any) => Promise<any>;
  includePartialMessages?: boolean;
}

/**
 * Query builder for Claude SDK
 */
export class ClaudeQueryBuilder {
  /**
   * Build query options from config
   */
  static buildQueryOptions(config: QueryConfig): QueryOptions {
    // Configure athena-browser-mcp server if enabled
    const mcpServers = config.enableMcp ? {
      'athena-browser': {
        command: 'npx',
        args: ['-y', 'athena-browser-mcp'],
        env: {
          CEF_BRIDGE_HOST: '127.0.0.1',
          CEF_BRIDGE_PORT: '9222',
          ALLOWED_FILE_DIRS: '/home/user/downloads,/tmp',
          DEFAULT_TIMEOUT_MS: '30000'
        }
      }
    } : undefined;

    // Build system prompt: combine preset with browser-specific instructions
    const systemPrompt = this.buildSystemPrompt(config.enableMcp);

    // Dynamically select tools based on prompt complexity
    const allowedTools = ToolSelector.selectTools(config.prompt, config.enableMcp || false);

    // Build options object
    const options: QueryOptions = {
      cwd: config.cwd,
      model: config.model,
      permissionMode: config.permissionMode,
      systemPrompt,
      settingSources: ['user', 'project', 'local'], // Load all settings for comprehensive context
      allowedTools,
      maxThinkingTokens: config.maxThinkingTokens,
      maxTurns: config.maxTurns,
      resume: config.claudeSessionId,
      mcpServers,
      // Add sub-agents for specialized browser tasks (only if MCP enabled)
      agents: config.enableMcp ? SubAgentConfig.getAgents() : undefined,
      // Use custom permission callback instead of permissionMode
      canUseTool: config.permissionMode === 'default'
        ? PermissionHandler.canUseTool.bind(PermissionHandler)
        : undefined
    };

    // Add optional properties
    if (config.forkSession !== undefined) {
      options.forkSession = config.forkSession;
    }

    if (config.includePartialMessages !== undefined) {
      options.includePartialMessages = config.includePartialMessages;
    }

    return options;
  }

  /**
   * Build system prompt based on MCP enablement
   */
  private static buildSystemPrompt(enableMcp?: boolean): { type: 'preset'; preset: 'claude_code' } | string {
    if (enableMcp) {
      const browserPrompt = BrowserPrompts.getSystemPrompt();
      return `${browserPrompt}\n\n---\n\nFollow the project's coding standards and practices as defined in CLAUDE.md.`;
    }

    return {
      type: 'preset',
      preset: 'claude_code'
    };
  }

  /**
   * Build query configuration from AthenaAgentConfig
   */
  static buildQueryConfig(
    agentConfig: AthenaAgentConfig,
    prompt: string,
    claudeSessionId?: string,
    options?: {
      forkSession?: boolean;
      includePartialMessages?: boolean;
      enableMcp?: boolean;
    }
  ): QueryConfig {
    return {
      cwd: agentConfig.cwd,
      model: agentConfig.model,
      permissionMode: agentConfig.permissionMode,
      maxThinkingTokens: agentConfig.maxThinkingTokens,
      maxTurns: agentConfig.maxTurns,
      prompt,
      claudeSessionId,
      forkSession: options?.forkSession,
      includePartialMessages: options?.includePartialMessages,
      enableMcp: options?.enableMcp ?? true  // Enable MCP by default
    };
  }
}
