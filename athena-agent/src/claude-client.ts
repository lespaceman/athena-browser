/**
 * Claude Agent SDK Integration
 *
 * Provides a high-level wrapper around the Claude Agent SDK for
 * managing conversations and streaming responses.
 */

import { query } from '@anthropic-ai/claude-agent-sdk';
import type {
  ChatResponse,
  AthenaAgentConfig
} from './types.js';
import { Logger } from './logger.js';
import type { McpSdkServerConfigWithInstance } from '@anthropic-ai/claude-agent-sdk';

const logger = new Logger('ClaudeClient');

export class ClaudeClient {
  private config: AthenaAgentConfig;
  private currentSessionId: string | null = null;
  private mcpServer: McpSdkServerConfigWithInstance | null = null;

  constructor(config: AthenaAgentConfig, mcpServer?: McpSdkServerConfigWithInstance) {
    this.config = config;
    this.mcpServer = mcpServer || null;
    logger.info('Claude client initialized', {
      model: config.model,
      permissionMode: config.permissionMode
    });
  }

  /**
   * Send a message and get the full response
   */
  async sendMessage(prompt: string): Promise<ChatResponse> {
    const startTime = Date.now();

    try {
      // Validate API key before making request
      // if (!this.config.apiKey) {
      //   throw new Error('ANTHROPIC_API_KEY is not set. Please set the ANTHROPIC_API_KEY environment variable to use Claude chat.');
      // }

      logger.debug('Sending message to Claude', {
        promptLength: prompt.length,
        sessionId: this.currentSessionId
      });

      const mcpServers = this.mcpServer ? {
        'athena-browser': this.mcpServer
      } : undefined;

      const stream = query({
        prompt,
        options: {
          cwd: this.config.cwd,
          model: this.config.model,
          permissionMode: this.config.permissionMode,
          systemPrompt: {
            type: 'preset',
            preset: 'claude_code'
          },
          settingSources: ['project'], // Load CLAUDE.md
          allowedTools: [
            'Read', 'Write', 'Edit', 'Glob', 'Grep',
            'Bash', 'WebFetch', 'WebSearch'
          ],
          maxThinkingTokens: this.config.maxThinkingTokens,
          maxTurns: this.config.maxTurns,
          continue: !!this.currentSessionId,
          resume: this.currentSessionId || undefined,
          mcpServers
        }
      });

      let resultMessage: any = null;

      for await (const message of stream) {
        if (message.type === 'system' && message.subtype === 'init') {
          this.currentSessionId = message.session_id;
        }

        if (message.type === 'result') {
          resultMessage = message;
          this.currentSessionId = message.session_id;
        }
      }

      const durationMs = Date.now() - startTime;

      if (!resultMessage) {
        throw new Error('No result message received from Claude');
      }

      const response: ChatResponse = {
        success: resultMessage.subtype === 'success',
        response: resultMessage.result || 'No response',
        sessionId: this.currentSessionId || '',
        usage: resultMessage.usage,
        cost: resultMessage.total_cost_usd,
        durationMs
      };

      logger.info('Claude response received', {
        success: response.success,
        sessionId: response.sessionId,
        durationMs,
        inputTokens: response.usage?.input_tokens,
        outputTokens: response.usage?.output_tokens,
        cost: response.cost
      });

      return response;

    } catch (error) {
      const durationMs = Date.now() - startTime;
      const errorMessage = error instanceof Error ? error.message : 'Unknown error';

      logger.error('Claude request failed', {
        error: errorMessage,
        durationMs
      });

      return {
        success: false,
        response: '',
        sessionId: this.currentSessionId || '',
        error: errorMessage,
        durationMs
      };
    }
  }

  /**
   * Continue the current conversation
   */
  async continueConversation(prompt: string): Promise<ChatResponse> {
    return this.sendMessage(prompt);
  }

  /**
   * Clear the conversation and start fresh
   */
  clearConversation(): void {
    this.currentSessionId = null;
    logger.info('Conversation cleared');
  }

  /**
   * Get current session ID
   */
  getSessionId(): string | null {
    return this.currentSessionId;
  }
}
