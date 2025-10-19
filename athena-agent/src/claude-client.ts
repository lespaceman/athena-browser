/**
 * Claude Agent SDK Integration
 *
 * Provides a high-level wrapper around the Claude Agent SDK for
 * managing conversations and streaming responses.
 */

import { query } from '@anthropic-ai/claude-agent-sdk';
import type {
  ChatResponse,
  AthenaAgentConfig,
  StreamChunk
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
   * Send a message and stream the response
   */
  async *streamMessage(prompt: string): AsyncGenerator<StreamChunk, void, unknown> {
    const startTime = Date.now();

    try {
      logger.debug('Streaming message to Claude', {
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
          mcpServers,
          includePartialMessages: true  // Enable streaming
        }
      });

      let accumulatedText = '';

      for await (const message of stream) {
        // Debug: log message types
        logger.debug('Stream message received', {
          type: message.type,
          subtype: (message as any).subtype
        });

        // Handle session initialization
        if (message.type === 'system' && message.subtype === 'init') {
          this.currentSessionId = message.session_id;
          continue;
        }

        // Handle streaming events (partial messages)
        if (message.type === 'stream_event') {
          const event = message.event;

          // Extract text from content_block_delta events
          if (event.type === 'content_block_delta' && event.delta?.type === 'text_delta') {
            const textDelta = event.delta.text;
            accumulatedText += textDelta;

            yield {
              type: 'chunk',
              content: textDelta,
              sessionId: this.currentSessionId || undefined
            };
          }
        }

        // Handle complete assistant messages (contains the full text response)
        if (message.type === 'assistant') {
          const content = message.message.content;

          // Extract text from content blocks
          for (const block of content) {
            if (block.type === 'text') {
              const text = block.text;

              // If we haven't accumulated this text yet (no streaming happened), send it all at once
              if (accumulatedText.length === 0) {
                accumulatedText = text;

                // Send the entire response as one chunk
                yield {
                  type: 'chunk',
                  content: text,
                  sessionId: this.currentSessionId || undefined
                };
              }
            }
          }
        }

        // Handle final result
        if (message.type === 'result') {
          this.currentSessionId = message.session_id;

          const durationMs = Date.now() - startTime;

          // If no text was accumulated from streaming, get it from the result
          if (accumulatedText.length === 0 && message.subtype === 'success') {
            accumulatedText = (message as any).result || '';

            // Send the final text as a chunk if we got it
            if (accumulatedText.length > 0) {
              yield {
                type: 'chunk',
                content: accumulatedText,
                sessionId: this.currentSessionId || undefined
              };
            }
          }

          logger.info('Claude streaming response completed', {
            success: message.subtype === 'success',
            sessionId: this.currentSessionId,
            durationMs,
            inputTokens: message.usage?.input_tokens,
            outputTokens: message.usage?.output_tokens,
            cost: message.total_cost_usd,
            textLength: accumulatedText.length
          });

          yield {
            type: 'done',
            content: accumulatedText,
            sessionId: this.currentSessionId
          };
          return;
        }
      }

    } catch (error) {
      const durationMs = Date.now() - startTime;
      const errorMessage = error instanceof Error ? error.message : 'Unknown error';

      logger.error('Claude streaming request failed', {
        error: errorMessage,
        durationMs
      });

      yield {
        type: 'error',
        error: errorMessage,
        sessionId: this.currentSessionId || undefined
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
