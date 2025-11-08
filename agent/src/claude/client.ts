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
} from '../server/types';
import { Logger } from '../server/logger';
import { SessionManager } from '../session/manager';
import type { Message } from '../session/types';
import { ClaudeQueryBuilder } from './query-builder';

const logger = new Logger('ClaudeClient');

export class ClaudeClient {
  private config: AthenaAgentConfig;
  private currentSessionId: string | null = null;
  private sessionManager: SessionManager;

  constructor(
    config: AthenaAgentConfig,
    sessionManager?: SessionManager
  ) {
    this.config = config;
    this.sessionManager = sessionManager || new SessionManager();
    logger.info('Claude client initialized', {
      model: config.model,
      permissionMode: config.permissionMode,
      maxThinkingTokens: config.maxThinkingTokens,
      maxTurns: config.maxTurns,
      sessionStorageEnabled: true,
    });
  }


  /**
   * Create a new session or get existing one
   */
  private async getOrCreateSession(title?: string): Promise<string> {
    if (this.currentSessionId) {
      return this.currentSessionId;
    }

    // Create new session
    const session = await this.sessionManager.createSession({
      title: title || `Session ${new Date().toISOString()}`,
      createdAt: Date.now(),
      lastUsedAt: Date.now(),
      messageCount: 0,
      totalTokens: { input: 0, output: 0 },
      totalCost: 0,
      model: this.config.model,
      tags: []
    });

    this.currentSessionId = session.metadata.sessionId;
    // Claude session ID will be set by SDK on first query and stored in metadata
    logger.info('New session created', {
      sessionId: this.currentSessionId
    });

    return this.currentSessionId;
  }

  /**
   * Save message to session
   */
  private async saveMessage(sessionId: string, message: Message): Promise<void> {
    if (!sessionId) {
      logger.warn('Cannot save message: no session ID provided');
      return;
    }

    try {
      await this.sessionManager.addMessage(sessionId, message);
      logger.debug('Message saved to session', {
        sessionId,
        role: message.role
      });
    } catch (error) {
      logger.error('Failed to save message', {
        sessionId,
        error: error instanceof Error ? error.message : 'Unknown error'
      });
    }
  }

  /**
   * Send a message and get the full response
   * @param prompt The message to send
   * @param sessionId Optional session ID to continue (if not provided, creates new session)
   * @param sessionTitle Optional title for new sessions
   */
  async sendMessage(prompt: string, sessionId?: string, sessionTitle?: string): Promise<ChatResponse> {
    const startTime = Date.now();

    try {
      // Get or create session
      let currentSessionId: string;
      let claudeSessionId: string | undefined;

      if (sessionId) {
        // Load existing session
        const session = await this.sessionManager.getSession(sessionId);
        if (!session) {
          throw new Error(`Session ${sessionId} not found`);
        }
        currentSessionId = sessionId;
        claudeSessionId = session.metadata.claudeSessionId;
        logger.info('Resuming existing session', {
          sessionId: currentSessionId,
          claudeSessionId: claudeSessionId || 'none',
          willResume: !!claudeSessionId
        });
      } else {
        // Create new session
        currentSessionId = await this.getOrCreateSession(sessionTitle);
        claudeSessionId = undefined; // New session has no Claude session ID yet
        logger.info('Created new session', {
          sessionId: currentSessionId,
          clearedClaudeSession: true
        });
      }

      // Save user message
      await this.saveMessage(currentSessionId, {
        role: 'user',
        content: prompt,
        timestamp: Date.now()
      });

      logger.debug('Sending message to Claude', {
        promptLength: prompt.length,
        sessionId: currentSessionId,
        claudeSessionId: claudeSessionId || 'none',
        willResume: !!claudeSessionId,
        model: this.config.model
      });

      // Build query configuration using ClaudeQueryBuilder
      const queryConfig = ClaudeQueryBuilder.buildQueryConfig(
        this.config,
        prompt,
        claudeSessionId
      );

      const queryOptions = ClaudeQueryBuilder.buildQueryOptions(queryConfig);

      const stream = query({
        prompt,
        options: queryOptions
      });

      let resultMessage: any = null;

      for await (const message of stream) {
        if (message.type === 'system' && message.subtype === 'init') {
          claudeSessionId = message.session_id;
          // Save Claude session ID to metadata
          await this.sessionManager.updateMetadata(currentSessionId, {
            claudeSessionId: claudeSessionId
          });
          logger.debug('Claude session initialized', {
            claudeSessionId: claudeSessionId,
            athenaSessionId: currentSessionId,
            savedToMetadata: true
          });
        }

        if (message.type === 'result') {
          resultMessage = message;
          claudeSessionId = message.session_id;
          // Save Claude session ID to metadata
          await this.sessionManager.updateMetadata(currentSessionId, {
            claudeSessionId: claudeSessionId
          });
        }
      }

      const durationMs = Date.now() - startTime;

      if (!resultMessage) {
        throw new Error('No result message received from Claude');
      }

      const response: ChatResponse = {
        success: resultMessage.subtype === 'success',
        response: resultMessage.result || 'No response',
        sessionId: currentSessionId,
        usage: resultMessage.usage,
        cost: resultMessage.total_cost_usd,
        durationMs
      };

      // Save assistant response to session
      await this.saveMessage(currentSessionId, {
        role: 'assistant',
        content: response.response,
        timestamp: Date.now(),
        tokens: {
          input: response.usage?.input_tokens,
          output: response.usage?.output_tokens
        },
        cost: response.cost
      });

      logger.info('Claude response received', {
        success: response.success,
        sessionId: currentSessionId,
        claudeSessionId: claudeSessionId,
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
        sessionId: this.currentSessionId || 'unknown',
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
   * @param prompt The message to send
   * @param sessionId Optional session ID to continue (if not provided, creates new session)
   * @param sessionTitle Optional title for new sessions
   */
  async *streamMessage(prompt: string, sessionId?: string, sessionTitle?: string): AsyncGenerator<StreamChunk, void, unknown> {
    const startTime = Date.now();

    try {
      // Get or create session
      let currentSessionId: string;
      let claudeSessionId: string | undefined;

      if (sessionId) {
        // Load existing session
        const session = await this.sessionManager.getSession(sessionId);
        if (!session) {
          throw new Error(`Session ${sessionId} not found`);
        }
        currentSessionId = sessionId;
        claudeSessionId = session.metadata.claudeSessionId;
        logger.info('Resuming existing session', {
          sessionId: currentSessionId,
          claudeSessionId: claudeSessionId || 'none',
          willResume: !!claudeSessionId
        });
      } else {
        // Create new session
        currentSessionId = await this.getOrCreateSession(sessionTitle);
        claudeSessionId = undefined; // New session has no Claude session ID yet
        logger.info('Created new session', {
          sessionId: currentSessionId,
          clearedClaudeSession: true
        });
      }

      // Save user message
      await this.saveMessage(currentSessionId, {
        role: 'user',
        content: prompt,
        timestamp: Date.now()
      });

      logger.info('Streaming message to Claude', {
        promptLength: prompt.length,
        sessionId: currentSessionId,
        claudeSessionId: claudeSessionId || 'none',
        willResume: !!claudeSessionId,
        model: this.config.model
      });

      logger.info('Calling Claude SDK query', {
        resume: claudeSessionId,
        athenaSessionId: currentSessionId,
        loadedClaudeSessionId: claudeSessionId || 'none'
      });

      // Build query configuration using ClaudeQueryBuilder
      const queryConfig = ClaudeQueryBuilder.buildQueryConfig(
        this.config,
        prompt,
        claudeSessionId,
        { includePartialMessages: true } // Enable streaming
      );

      const queryOptions = ClaudeQueryBuilder.buildQueryOptions(queryConfig);

      const stream = query({
        prompt,
        options: queryOptions
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
          claudeSessionId = message.session_id;
          // Save Claude session ID to session metadata for future resumption
          await this.sessionManager.updateMetadata(currentSessionId, {
            claudeSessionId: claudeSessionId
          });
          logger.info('Claude session initialized (streaming)', {
            claudeSessionId: claudeSessionId,
            athenaSessionId: currentSessionId,
            savedToMetadata: true
          });
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
              sessionId: currentSessionId
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
                  sessionId: currentSessionId
                };
              }
            }
          }
        }

        // Handle final result
        if (message.type === 'result') {
          claudeSessionId = message.session_id;
          // Save Claude session ID to metadata for future resumption
          await this.sessionManager.updateMetadata(currentSessionId, {
            claudeSessionId: claudeSessionId
          });

          const durationMs = Date.now() - startTime;

          // If no text was accumulated from streaming, get it from the result
          if (accumulatedText.length === 0 && message.subtype === 'success') {
            accumulatedText = (message as any).result || '';

            // Send the final text as a chunk if we got it
            if (accumulatedText.length > 0) {
              yield {
                type: 'chunk',
                content: accumulatedText,
                sessionId: currentSessionId
              };
            }
          }

          // Save assistant response to session
          await this.saveMessage(currentSessionId, {
            role: 'assistant',
            content: accumulatedText,
            timestamp: Date.now(),
            tokens: {
              input: message.usage?.input_tokens,
              output: message.usage?.output_tokens
            },
            cost: message.total_cost_usd
          });

          logger.info('Claude streaming response completed', {
            success: message.subtype === 'success',
            sessionId: currentSessionId,
            claudeSessionId: claudeSessionId,
            durationMs,
            inputTokens: message.usage?.input_tokens,
            outputTokens: message.usage?.output_tokens,
            cost: message.total_cost_usd,
            textLength: accumulatedText.length
          });

          yield {
            type: 'done',
            content: accumulatedText,
            sessionId: currentSessionId
          };
          return;
        }
      }

    } catch (error) {
      const durationMs = Date.now() - startTime;
      const errorMessage = error instanceof Error ? error.message : 'Unknown error';

      logger.error('Claude streaming request failed', {
        error: errorMessage,
        durationMs,
        sessionId: this.currentSessionId || 'unknown'
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
   * Fork the current session to explore alternative paths
   * This creates a new conversation branch without affecting the original
   */
  async forkSession(prompt: string): Promise<ChatResponse> {
    if (!this.currentSessionId) {
      logger.warn('Cannot fork session: no active session');
      return this.sendMessage(prompt);
    }

    const originalSessionId = this.currentSessionId;
    const startTime = Date.now();

    try {
      logger.info('Forking session', { originalSessionId });

      // Build query configuration using ClaudeQueryBuilder
      const queryConfig = ClaudeQueryBuilder.buildQueryConfig(
        this.config,
        prompt,
        originalSessionId,
        { forkSession: true } // Fork the session for exploratory path
      );

      const queryOptions = ClaudeQueryBuilder.buildQueryOptions(queryConfig);

      const stream = query({
        prompt,
        options: queryOptions
      });

      let resultMessage: any = null;
      let forkedSessionId: string | null = null;

      for await (const message of stream) {
        if (message.type === 'system' && message.subtype === 'init') {
          forkedSessionId = message.session_id;
        }

        if (message.type === 'result') {
          resultMessage = message;
          forkedSessionId = message.session_id;
        }
      }

      const durationMs = Date.now() - startTime;

      if (!resultMessage) {
        throw new Error('No result message received from forked session');
      }

      const response: ChatResponse = {
        success: resultMessage.subtype === 'success',
        response: resultMessage.result || 'No response',
        sessionId: forkedSessionId || '',
        usage: resultMessage.usage,
        cost: resultMessage.total_cost_usd,
        durationMs
      };

      logger.info('Session forked successfully', {
        originalSessionId,
        forkedSessionId,
        durationMs
      });

      // Don't update currentSessionId - keep the original session active
      return response;

    } catch (error) {
      const durationMs = Date.now() - startTime;
      const errorMessage = error instanceof Error ? error.message : 'Unknown error';

      logger.error('Failed to fork session', {
        originalSessionId,
        error: errorMessage,
        durationMs
      });

      return {
        success: false,
        response: '',
        sessionId: originalSessionId,
        error: errorMessage,
        durationMs
      };
    }
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

  /**
   * Get Claude SDK session ID from current session metadata
   */
  async getClaudeSessionId(): Promise<string | null> {
    if (!this.currentSessionId) {
      return null;
    }

    const session = await this.sessionManager.getSession(this.currentSessionId);
    return session?.metadata.claudeSessionId || null;
  }

  /**
   * Resume a specific session by ID
   */
  async resumeSession(sessionId: string): Promise<void> {
    const session = await this.sessionManager.getSession(sessionId);
    if (!session) {
      throw new Error(`Session ${sessionId} not found`);
    }

    this.currentSessionId = sessionId;
    logger.info('Session resumed', {
      sessionId,
      hasClaudeSessionId: !!session.metadata.claudeSessionId
    });
  }

  /**
   * Get session manager for external use
   */
  getSessionManager(): SessionManager {
    return this.sessionManager;
  }

}
