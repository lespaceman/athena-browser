/**
 * Claude Query Builder
 *
 * Builds query options for the Claude SDK, reducing duplication across
 * sendMessage, streamMessage, and forkSession methods.
 */

import type { McpSdkServerConfigWithInstance } from '@anthropic-ai/claude-agent-sdk';
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
  mcpServer?: McpSdkServerConfigWithInstance | null;
  prompt: string;
  claudeSessionId?: string;
  forkSession?: boolean;
  includePartialMessages?: boolean;
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
  mcpServers?: Record<string, McpSdkServerConfigWithInstance>;
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
    const mcpServers = config.mcpServer ? {
      'athena-browser': config.mcpServer
    } : undefined;

    // Build system prompt: combine preset with browser-specific instructions
    const systemPrompt = this.buildSystemPrompt(config.mcpServer);

    // Dynamically select tools based on prompt complexity
    const allowedTools = ToolSelector.selectTools(config.prompt, config.mcpServer || null);

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
      // Add sub-agents for specialized browser tasks
      agents: config.mcpServer ? SubAgentConfig.getAgents() : undefined,
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
   * Build system prompt based on MCP server configuration
   */
  private static buildSystemPrompt(mcpServer?: McpSdkServerConfigWithInstance | null): { type: 'preset'; preset: 'claude_code' } | string {
    if (mcpServer) {
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
    mcpServer: McpSdkServerConfigWithInstance | null,
    prompt: string,
    claudeSessionId?: string,
    options?: {
      forkSession?: boolean;
      includePartialMessages?: boolean;
    }
  ): QueryConfig {
    return {
      cwd: agentConfig.cwd,
      model: agentConfig.model,
      permissionMode: agentConfig.permissionMode,
      maxThinkingTokens: agentConfig.maxThinkingTokens,
      maxTurns: agentConfig.maxTurns,
      mcpServer,
      prompt,
      claudeSessionId,
      forkSession: options?.forkSession,
      includePartialMessages: options?.includePartialMessages
    };
  }
}
