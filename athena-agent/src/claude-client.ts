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
import { SessionManager } from './session-manager.js';
import type { Message } from './session-types.js';

const logger = new Logger('ClaudeClient');

export class ClaudeClient {
  private config: AthenaAgentConfig;
  private currentSessionId: string | null = null;
  private mcpServer: McpSdkServerConfigWithInstance | null = null;
  private sessionManager: SessionManager;

  constructor(
    config: AthenaAgentConfig,
    mcpServer?: McpSdkServerConfigWithInstance,
    sessionManager?: SessionManager
  ) {
    this.config = config;
    this.mcpServer = mcpServer || null;
    this.sessionManager = sessionManager || new SessionManager();
    logger.info('Claude client initialized', {
      model: config.model,
      permissionMode: config.permissionMode,
      maxThinkingTokens: config.maxThinkingTokens,
      maxTurns: config.maxTurns,
      sessionStorageEnabled: true
    });
  }

  /**
   * Custom permission callback for dynamic tool usage decisions.
   * Only handles dynamic logic (JavaScript execution confirmation).
   * Static tool permissions are handled by the allowedTools array.
   */
  private async canUseTool(toolName: string, input: any): Promise<any> {
    // Require confirmation for JavaScript execution
    if (toolName === 'mcp__athena-browser__browser_execute_js') {
      logger.warn('JavaScript execution requested', { code: input.code });
      return {
        behavior: 'ask',
        message: `Allow JavaScript execution: "${input.code?.substring(0, 100)}..."?`
      };
    }

    // All other tools are controlled by allowedTools array
    return { behavior: 'allow' };
  }

  /**
   * Get browser-specific system prompt
   */
  private getBrowserSystemPrompt(): string {
    return `You are an expert web browser automation agent with deep knowledge of web technologies and user interactions.

**IMPORTANT RESTRICTION:**
You have access ONLY to Athena browser MCP tools. You do NOT have access to:
- File system tools (Read, Write, Edit, Glob, Grep)
- CLI tools (Bash)
- Web fetching tools (WebFetch, WebSearch)
Your entire focus is on browser automation and web page interaction.

**Available Browser Tools:**
- Navigation: browser_navigate, browser_back, browser_forward, browser_reload
- Information: browser_get_url, browser_get_html, browser_get_page_summary, browser_get_interactive_elements, browser_get_accessibility_tree, browser_query_content
- Interaction: browser_execute_js
- Screenshots: browser_screenshot, browser_get_annotated_screenshot
- Tab Management: window_create_tab, window_close_tab, window_switch_tab, window_get_tab_info

**Best Practices:**
- ALWAYS use browser_get_page_summary before browser_get_html to save tokens (1-2KB vs 100KB+)
- Use browser_get_interactive_elements to find clickable items efficiently
- Use browser_query_content for targeted data extraction (forms, navigation, articles, tables, media)
- Verify navigation actions with browser_get_url after each navigation
- Use browser_get_annotated_screenshot for visual understanding of complex pages
- Prefer browser_get_accessibility_tree for semantic page structure

**Tool Usage Efficiency:**
1. For simple queries: page_summary + query_content
2. For interaction: interactive_elements → execute_js or screenshot
3. For data extraction: query_content with specific type
4. For debugging: accessibility_tree + annotated_screenshot

Always explain your reasoning before taking actions.`;
  }

  /**
   * Get sub-agents configuration for specialized browser tasks
   * RESTRICTED: Only browser MCP tools, no file system access
   */
  private getSubAgents(): any {
    return {
      'web-analyzer': {
        description: 'Expert in analyzing web page structure, extracting information, and understanding semantic content. Use for page analysis, data extraction, and content understanding tasks.',
        prompt: `You specialize in web page analysis and data extraction.

Your expertise:
- Analyzing HTML structure and DOM trees
- Extracting structured data from pages
- Understanding semantic content and accessibility trees
- Identifying key page elements and their relationships
- Optimizing data extraction strategies

Always use the most efficient tools:
1. Start with page_summary for overview
2. Use query_content for targeted extraction
3. Use accessibility_tree for semantic structure
4. Only use full HTML when absolutely necessary

Provide clear, structured output.`,
        tools: [
          'mcp__athena-browser__browser_get_url',
          'mcp__athena-browser__browser_get_html',
          'mcp__athena-browser__browser_get_page_summary',
          'mcp__athena-browser__browser_get_interactive_elements',
          'mcp__athena-browser__browser_get_accessibility_tree',
          'mcp__athena-browser__browser_query_content',
          'mcp__athena-browser__browser_screenshot'
        ],
        model: 'sonnet' as const
      },

      'navigation-expert': {
        description: 'Expert in browser navigation, multi-step workflows, and tab management. Use for complex navigation tasks, site exploration, and workflow automation.',
        prompt: `You specialize in browser navigation and workflow automation.

Your expertise:
- Planning and executing multi-step navigation workflows
- Managing browser tabs efficiently
- Handling history navigation (back/forward)
- Recovering from navigation errors
- Optimizing navigation paths

Best practices:
1. Verify current URL before navigating
2. Check page load success after navigation
3. Use page_summary to confirm correct page
4. Manage tabs strategically (close unused tabs)
5. Handle redirects and errors gracefully

Always confirm successful navigation.`,
        tools: [
          'mcp__athena-browser__browser_navigate',
          'mcp__athena-browser__browser_back',
          'mcp__athena-browser__browser_forward',
          'mcp__athena-browser__browser_reload',
          'mcp__athena-browser__browser_get_url',
          'mcp__athena-browser__browser_get_page_summary',
          'mcp__athena-browser__window_create_tab',
          'mcp__athena-browser__window_close_tab',
          'mcp__athena-browser__window_switch_tab',
          'mcp__athena-browser__window_get_tab_info'
        ],
        model: 'haiku' as const
      },

      'form-automation': {
        description: 'Expert in web form interactions, input validation, and form submission. Use for filling forms, handling input fields, and automating form-based workflows.',
        prompt: `You specialize in web form automation and interaction.

Your expertise:
- Analyzing form structure and fields
- Identifying form inputs and their types
- Generating appropriate JavaScript for form filling
- Validating input before submission
- Handling form errors and validation

Process:
1. Use query_content with "forms" to get form structure
2. Analyze required fields and validation rules
3. Generate safe JavaScript for form filling
4. Verify form state before submission
5. Handle errors and edge cases

Always validate inputs and explain your approach.`,
        tools: [
          'mcp__athena-browser__browser_get_page_summary',
          'mcp__athena-browser__browser_query_content',
          'mcp__athena-browser__browser_get_interactive_elements',
          'mcp__athena-browser__browser_execute_js',
          'mcp__athena-browser__browser_screenshot'
        ],
        model: 'sonnet' as const
      },

      'screenshot-analyst': {
        description: 'Expert in analyzing visual content from screenshots, understanding UI layout, and identifying visual elements. Use for visual debugging and UI understanding.',
        prompt: `You specialize in visual analysis of web pages.

Your expertise:
- Analyzing screenshots for UI elements
- Understanding visual layout and design
- Identifying clickable elements from visual cues
- Debugging visual rendering issues
- Comparing before/after states

Tools:
1. Use annotated_screenshot for element identification
2. Use regular screenshot for visual debugging
3. Combine with accessibility_tree for semantic context
4. Use interactive_elements for clickability verification

Provide detailed visual analysis.`,
        tools: [
          'mcp__athena-browser__browser_screenshot',
          'mcp__athena-browser__browser_get_annotated_screenshot',
          'mcp__athena-browser__browser_get_interactive_elements',
          'mcp__athena-browser__browser_get_accessibility_tree'
        ],
        model: 'sonnet' as const
      }
    };
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

      const mcpServers = this.mcpServer ? {
        'athena-browser': this.mcpServer
      } : undefined;

      // Build system prompt: combine preset with browser-specific instructions
      const systemPrompt = this.mcpServer
        ? `${this.getBrowserSystemPrompt()}\n\n---\n\nFollow the project's coding standards and practices as defined in CLAUDE.md.`
        : undefined;

      // Dynamically select tools based on prompt complexity
      const allowedTools = this.getToolsForPrompt(prompt);

      const stream = query({
        prompt,
        options: {
          cwd: this.config.cwd,
          model: this.config.model,
          permissionMode: this.config.permissionMode,
          // Use custom system prompt for browser tasks, otherwise use preset
          systemPrompt: systemPrompt ? systemPrompt : {
            type: 'preset',
            preset: 'claude_code'
          },
          settingSources: ['user', 'project', 'local'], // Load all settings for comprehensive context
          allowedTools, // Use dynamically selected tools
          maxThinkingTokens: this.config.maxThinkingTokens,
          maxTurns: this.config.maxTurns,
          resume: claudeSessionId || undefined,
          mcpServers,
          // Add sub-agents for specialized browser tasks
          agents: this.mcpServer ? this.getSubAgents() : undefined,
          // Use custom permission callback instead of permissionMode
          canUseTool: this.config.permissionMode === 'default'
            ? this.canUseTool.bind(this)
            : undefined
        }
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

      const mcpServers = this.mcpServer ? {
        'athena-browser': this.mcpServer
      } : undefined;

      // Build system prompt: combine preset with browser-specific instructions
      const systemPrompt = this.mcpServer
        ? `${this.getBrowserSystemPrompt()}\n\n---\n\nFollow the project's coding standards and practices as defined in CLAUDE.md.`
        : undefined;

      // Dynamically select tools based on prompt complexity
      const allowedTools = this.getToolsForPrompt(prompt);

      // Use the Claude session ID from loaded session (not instance variable!)
      const resumeSessionId = claudeSessionId || undefined;

      logger.info('Calling Claude SDK query', {
        resume: resumeSessionId,
        athenaSessionId: currentSessionId,
        loadedClaudeSessionId: claudeSessionId || 'none'
      });

      const stream = query({
        prompt,
        options: {
          cwd: this.config.cwd,
          model: this.config.model,
          permissionMode: this.config.permissionMode,
          // Use custom system prompt for browser tasks, otherwise use preset
          systemPrompt: systemPrompt ? systemPrompt : {
            type: 'preset',
            preset: 'claude_code'
          },
          settingSources: ['user', 'project', 'local'], // Load all settings for comprehensive context
          allowedTools,
          maxThinkingTokens: this.config.maxThinkingTokens,
          maxTurns: this.config.maxTurns,
          resume: resumeSessionId,
          mcpServers,
          agents: this.mcpServer ? this.getSubAgents() : undefined,
          // Use custom permission callback
          canUseTool: this.config.permissionMode === 'default'
            ? this.canUseTool.bind(this)
            : undefined,
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

      const mcpServers = this.mcpServer ? {
        'athena-browser': this.mcpServer
      } : undefined;

      const systemPrompt = this.mcpServer
        ? `${this.getBrowserSystemPrompt()}\n\n---\n\nFollow the project's coding standards and practices as defined in CLAUDE.md.`
        : undefined;

      const stream = query({
        prompt,
        options: {
          cwd: this.config.cwd,
          model: this.config.model,
          permissionMode: this.config.permissionMode,
          systemPrompt: systemPrompt ? systemPrompt : {
            type: 'preset',
            preset: 'claude_code'
          },
          settingSources: ['user', 'project', 'local'],
          // RESTRICTED: Only browser MCP tools for forked sessions
          allowedTools: this.mcpServer ? [
            'mcp__athena-browser__browser_navigate',
            'mcp__athena-browser__browser_back',
            'mcp__athena-browser__browser_forward',
            'mcp__athena-browser__browser_reload',
            'mcp__athena-browser__browser_get_url',
            'mcp__athena-browser__browser_get_html',
            'mcp__athena-browser__browser_get_page_summary',
            'mcp__athena-browser__browser_get_interactive_elements',
            'mcp__athena-browser__browser_get_accessibility_tree',
            'mcp__athena-browser__browser_query_content',
            'mcp__athena-browser__browser_get_annotated_screenshot',
            'mcp__athena-browser__browser_execute_js',
            'mcp__athena-browser__browser_screenshot',
            'mcp__athena-browser__window_create_tab',
            'mcp__athena-browser__window_close_tab',
            'mcp__athena-browser__window_switch_tab',
            'mcp__athena-browser__window_get_tab_info'
          ] : [],
          maxThinkingTokens: this.config.maxThinkingTokens,
          maxTurns: this.config.maxTurns,
          resume: originalSessionId,
          forkSession: true, // Fork the session for exploratory path
          mcpServers,
          agents: this.mcpServer ? this.getSubAgents() : undefined,
          canUseTool: this.config.permissionMode === 'default'
            ? this.canUseTool.bind(this)
            : undefined
        }
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

  /**
   * Analyze prompt complexity and return appropriate tool set.
   * Only returns Athena browser MCP tools (no file system or CLI access).
   * This method provides the allowedTools array for static permission control.
   */
  private getToolsForPrompt(prompt: string): string[] {
    const lowerPrompt = prompt.toLowerCase();

    // Check for read-only vs. interactive tasks
    const isReadOnly =
      lowerPrompt.includes('show') ||
      lowerPrompt.includes('get') ||
      lowerPrompt.includes('find') ||
      lowerPrompt.includes('search') ||
      lowerPrompt.includes('analyze') ||
      lowerPrompt.includes('extract');

    const needsInteraction =
      lowerPrompt.includes('click') ||
      lowerPrompt.includes('fill') ||
      lowerPrompt.includes('submit') ||
      lowerPrompt.includes('execute') ||
      lowerPrompt.includes('navigate') ||
      lowerPrompt.includes('type');

    // ONLY return browser MCP tools - no file system or CLI tools allowed
    const tools: string[] = [];

    if (!this.mcpServer) {
      logger.warn('No MCP server configured - agent will have no tools available');
      return tools;
    }

    // For read-only browser tasks, only add read tools
    if (isReadOnly && !needsInteraction) {
      tools.push(
        'mcp__athena-browser__browser_get_url',
        'mcp__athena-browser__browser_get_html',
        'mcp__athena-browser__browser_get_page_summary',
        'mcp__athena-browser__browser_get_interactive_elements',
        'mcp__athena-browser__browser_get_accessibility_tree',
        'mcp__athena-browser__browser_query_content',
        'mcp__athena-browser__browser_screenshot',
        'mcp__athena-browser__browser_get_annotated_screenshot',
        'mcp__athena-browser__window_get_tab_info'
      );
      logger.debug('Using read-only browser tools', { toolCount: tools.length });
    } else {
      // For interactive tasks, add all browser tools (including navigation and JS execution)
      tools.push(
        'mcp__athena-browser__browser_navigate',
        'mcp__athena-browser__browser_back',
        'mcp__athena-browser__browser_forward',
        'mcp__athena-browser__browser_reload',
        'mcp__athena-browser__browser_get_url',
        'mcp__athena-browser__browser_get_html',
        'mcp__athena-browser__browser_get_page_summary',
        'mcp__athena-browser__browser_get_interactive_elements',
        'mcp__athena-browser__browser_get_accessibility_tree',
        'mcp__athena-browser__browser_query_content',
        'mcp__athena-browser__browser_get_annotated_screenshot',
        'mcp__athena-browser__browser_execute_js',
        'mcp__athena-browser__browser_screenshot',
        'mcp__athena-browser__window_create_tab',
        'mcp__athena-browser__window_close_tab',
        'mcp__athena-browser__window_switch_tab',
        'mcp__athena-browser__window_get_tab_info'
      );
      logger.debug('Using full browser tools for interactive task', { toolCount: tools.length });
    }

    logger.debug('Tool selection complete (browser-only mode)', {
      promptLength: prompt.length,
      totalTools: tools.length,
      readOnly: isReadOnly,
      needsInteraction
    });

    return tools;
  }
}
