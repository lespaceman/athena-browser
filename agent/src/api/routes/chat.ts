/**
 * Chat endpoints for Claude conversations
 */

import type { Request, Response } from 'express';
import type { CapabilitiesResponse } from '../../server/types';
import type { ClaudeClient } from '../../claude/client';
import { Logger } from '../../server/logger';
import { z } from 'zod';

const logger = new Logger('ChatRoutes');

// Validation schemas
const ChatRequestSchema = z.object({
  message: z.string().min(1).max(50000),
  sessionId: z.string().optional() // Optional: auto-create if not provided
});

/**
 * POST /v1/chat/send
 * Send a message and get the full response
 */
export function createSendHandler(claudeClient: ClaudeClient) {
  return async (req: Request, res: Response): Promise<void> => {
    try {
      // Validate request
      const validation = ChatRequestSchema.safeParse(req.body);
      if (!validation.success) {
        res.status(400).json({
          error: 'Bad Request',
          message: 'Invalid request body',
          details: validation.error.errors
        });
        return;
      }

      const { message, sessionId } = validation.data;
      const requestId = req.headers['x-request-id'] as string || `req-${Date.now()}`;

      logger.info('Chat request received', {
        requestId,
        sessionId: sessionId || 'new',
        messageLength: message.length
      });

      // Send message to Claude with session context
      const response = await claudeClient.sendMessage(message, sessionId);

      logger.info('Chat response sent', {
        requestId,
        success: response.success,
        durationMs: response.durationMs
      });

      res.json(response);

    } catch (error) {
      logger.error('Chat request failed', {
        error: error instanceof Error ? error.message : 'Unknown error'
      });

      res.status(500).json({
        success: false,
        response: '',
        sessionId: '',
        error: error instanceof Error ? error.message : 'Internal server error'
      });
    }
  };
}

/**
 * POST /v1/chat/stream
 * Stream a message response using Server-Sent Events
 */
export function createStreamHandler(claudeClient: ClaudeClient) {
  return async (req: Request, res: Response): Promise<void> => {
    try {
      // Validate request
      const validation = ChatRequestSchema.safeParse(req.body);
      if (!validation.success) {
        res.status(400).json({
          error: 'Bad Request',
          message: 'Invalid request body',
          details: validation.error.errors
        });
        return;
      }

      const { message, sessionId } = validation.data;
      const requestId = req.headers['x-request-id'] as string || `req-${Date.now()}`;

      logger.info('Chat stream request received', {
        requestId,
        sessionId: sessionId || 'new',
        messageLength: message.length
      });

      // Set up SSE headers
      res.setHeader('Content-Type', 'text/event-stream');
      res.setHeader('Cache-Control', 'no-cache');
      res.setHeader('Connection', 'keep-alive');
      res.setHeader('X-Accel-Buffering', 'no'); // Disable nginx buffering

      // Stream the response
      try {
        for await (const chunk of claudeClient.streamMessage(message, sessionId)) {
          // Send SSE event
          const data = JSON.stringify(chunk);
          logger.debug('Sending SSE chunk', { chunk });
          res.write(`data: ${data}\n\n`);

          // If this is the done or error event, we can close the stream
          if (chunk.type === 'done' || chunk.type === 'error') {
            logger.info('Chat stream completed', {
              requestId,
              type: chunk.type
            });
            break;
          }
        }
      } catch (streamError) {
        logger.error('Streaming error', {
          requestId,
          error: streamError instanceof Error ? streamError.message : 'Unknown error'
        });

        // Send error event
        const errorChunk = {
          type: 'error',
          error: streamError instanceof Error ? streamError.message : 'Streaming error'
        };
        res.write(`data: ${JSON.stringify(errorChunk)}\n\n`);
      }

      // End the response
      res.end();

    } catch (error) {
      logger.error('Chat stream request failed', {
        error: error instanceof Error ? error.message : 'Unknown error'
      });

      // If headers not sent yet, send error response
      if (!res.headersSent) {
        res.status(500).json({
          success: false,
          response: '',
          sessionId: '',
          error: error instanceof Error ? error.message : 'Internal server error'
        });
      } else {
        // Headers already sent, send error via SSE
        const errorChunk = {
          type: 'error',
          error: error instanceof Error ? error.message : 'Internal server error'
        };
        res.write(`data: ${JSON.stringify(errorChunk)}\n\n`);
        res.end();
      }
    }
  };
}

/**
 * POST /v1/chat/continue
 * Continue the current conversation
 * @deprecated Use POST /v1/chat/send with sessionId instead
 */
export function createContinueHandler(claudeClient: ClaudeClient) {
  return async (req: Request, res: Response): Promise<void> => {
    try {
      const validation = ChatRequestSchema.safeParse(req.body);
      if (!validation.success) {
        res.status(400).json({
          error: 'Bad Request',
          message: 'Invalid request body',
          details: validation.error.errors
        });
        return;
      }

      const { message, sessionId } = validation.data;
      const requestId = req.headers['x-request-id'] as string || `req-${Date.now()}`;

      logger.info('Continue conversation request (deprecated)', {
        requestId,
        sessionId: sessionId || 'none provided'
      });

      // Just call sendMessage with sessionId - same functionality
      const response = await claudeClient.sendMessage(message, sessionId);

      res.json(response);

    } catch (error) {
      logger.error('Continue conversation failed', {
        error: error instanceof Error ? error.message : 'Unknown error'
      });

      res.status(500).json({
        success: false,
        response: '',
        sessionId: '',
        error: error instanceof Error ? error.message : 'Internal server error'
      });
    }
  };
}

/**
 * POST /v1/chat/clear
 * Clear the conversation history
 */
export function createClearHandler(claudeClient: ClaudeClient) {
  return (_req: Request, res: Response) => {
    claudeClient.clearConversation();

    logger.info('Conversation cleared');

    res.json({
      success: true,
      message: 'Conversation cleared'
    });
  };
}

/**
 * GET /v1/capabilities
 * Get agent capabilities
 */
export function capabilitiesHandler(_req: Request, res: Response): void {
  const response: CapabilitiesResponse = {
    version: '1.0.0',
    features: [
      'chat',
      'streaming',
      'mcp-tools',
      'browser-control'
    ],
    mcp_tools: [
      'browser_navigate',
      'browser_back',
      'browser_forward',
      'browser_reload',
      'browser_get_url',
      'browser_get_html',
      'browser_execute_js',
      'browser_screenshot',
      'window_create_tab',
      'window_close_tab',
      'window_switch_tab'
    ],
    model: process.env.CLAUDE_MODEL || 'claude-sonnet-4-5-haiku'
  };

  res.json(response);
}
