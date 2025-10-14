/**
 * Chat endpoints for Claude conversations
 */

import type { Request, Response } from 'express';
import type { CapabilitiesResponse } from '../types.js';
import type { ClaudeClient } from '../claude-client.js';
import { Logger } from '../logger.js';
import { z } from 'zod';

const logger = new Logger('ChatRoutes');

// Validation schemas
const ChatRequestSchema = z.object({
  message: z.string().min(1).max(50000)
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

      const { message } = validation.data;
      const requestId = req.headers['x-request-id'] as string || `req-${Date.now()}`;

      logger.info('Chat request received', {
        requestId,
        messageLength: message.length
      });

      // Send message to Claude
      const response = await claudeClient.sendMessage(message);

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
 * POST /v1/chat/continue
 * Continue the current conversation
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

      const { message } = validation.data;
      const requestId = req.headers['x-request-id'] as string || `req-${Date.now()}`;

      logger.info('Continue conversation request', {
        requestId,
        sessionId: claudeClient.getSessionId()
      });

      const response = await claudeClient.continueConversation(message);

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
    model: process.env.CLAUDE_MODEL || 'claude-sonnet-4-5'
  };

  res.json(response);
}
