/**
 * Session Management Routes
 *
 * REST API endpoints for managing conversation sessions.
 */

import type { Request, Response } from 'express';
import { Logger } from '../logger.js';
import type { SessionManager } from '../session-manager.js';

const logger = new Logger('SessionsRouter');

/**
 * GET /v1/sessions
 * List all sessions with optional filtering and pagination
 */
export function createListHandler(sessionManager: SessionManager) {
  return async (req: Request, res: Response) => {
    try {
      const {
        limit,
        offset,
        sortBy,
        sortOrder,
        tags
      } = req.query;

      const sessions = await sessionManager.listSessions({
        limit: limit ? parseInt(limit as string) : undefined,
        offset: offset ? parseInt(offset as string) : undefined,
        sortBy: sortBy as any,
        sortOrder: sortOrder as any,
        tags: tags ? (tags as string).split(',') : undefined
      });

      res.json({
        success: true,
        sessions,
        count: sessions.length
      });
    } catch (error) {
      logger.error('Failed to list sessions', {
        error: error instanceof Error ? error.message : 'Unknown error'
      });

      res.status(500).json({
        success: false,
        error: 'Failed to list sessions',
        message: error instanceof Error ? error.message : 'Unknown error'
      });
    }
  };
}

/**
 * GET /v1/sessions/:sessionId
 * Get a specific session with full message history
 */
export function createGetHandler(sessionManager: SessionManager) {
  return async (req: Request, res: Response) => {
    try {
      const { sessionId } = req.params;

      const session = await sessionManager.getSession(sessionId);

      if (!session) {
        res.status(404).json({
          success: false,
          error: 'Session not found',
          sessionId
        });
        return;
      }

      // NOTE: Messages are not stored locally - Claude SDK manages conversation history
      // Session only contains metadata (title, timestamps, token counts, etc.)

      res.json({
        success: true,
        session
      });
    } catch (error) {
      logger.error('Failed to get session', {
        error: error instanceof Error ? error.message : 'Unknown error'
      });

      res.status(500).json({
        success: false,
        error: 'Failed to get session',
        message: error instanceof Error ? error.message : 'Unknown error'
      });
    }
  };
}

/**
 * GET /v1/sessions/:sessionId/messages
 * Get messages from a session with pagination
 */
export function createGetMessagesHandler(sessionManager: SessionManager) {
  return async (req: Request, res: Response) => {
    try {
      const { sessionId } = req.params;
      const { limit, offset } = req.query;

      const messages = await sessionManager.getMessages(
        sessionId,
        limit ? parseInt(limit as string) : undefined,
        offset ? parseInt(offset as string) : 0
      );

      res.json({
        success: true,
        sessionId,
        messages,
        count: messages.length
      });
    } catch (error) {
      logger.error('Failed to get messages', {
        error: error instanceof Error ? error.message : 'Unknown error'
      });

      res.status(500).json({
        success: false,
        error: 'Failed to get messages',
        message: error instanceof Error ? error.message : 'Unknown error'
      });
    }
  };
}

/**
 * PATCH /v1/sessions/:sessionId
 * Update session metadata
 */
export function createUpdateHandler(sessionManager: SessionManager) {
  return async (req: Request, res: Response) => {
    try {
      const { sessionId } = req.params;
      const updates = req.body;

      // Validate that session exists
      const session = await sessionManager.getSession(sessionId);
      if (!session) {
        res.status(404).json({
          success: false,
          error: 'Session not found',
          sessionId
        });
        return;
      }

      await sessionManager.updateMetadata(sessionId, updates);

      res.json({
        success: true,
        sessionId,
        message: 'Session updated successfully'
      });
    } catch (error) {
      logger.error('Failed to update session', {
        error: error instanceof Error ? error.message : 'Unknown error'
      });

      res.status(500).json({
        success: false,
        error: 'Failed to update session',
        message: error instanceof Error ? error.message : 'Unknown error'
      });
    }
  };
}

/**
 * DELETE /v1/sessions/:sessionId
 * Delete a session
 */
export function createDeleteHandler(sessionManager: SessionManager) {
  return async (req: Request, res: Response) => {
    try {
      const { sessionId } = req.params;

      await sessionManager.deleteSession(sessionId);

      res.json({
        success: true,
        sessionId,
        message: 'Session deleted successfully'
      });
    } catch (error) {
      logger.error('Failed to delete session', {
        error: error instanceof Error ? error.message : 'Unknown error'
      });

      res.status(500).json({
        success: false,
        error: 'Failed to delete session',
        message: error instanceof Error ? error.message : 'Unknown error'
      });
    }
  };
}

/**
 * GET /v1/sessions/search
 * Search sessions by title or tags
 */
export function createSearchHandler(sessionManager: SessionManager) {
  return async (req: Request, res: Response) => {
    try {
      const { q } = req.query;

      if (!q || typeof q !== 'string') {
        res.status(400).json({
          success: false,
          error: 'Missing or invalid search query parameter "q"'
        });
        return;
      }

      const sessions = await sessionManager.searchSessions(q);

      res.json({
        success: true,
        query: q,
        sessions,
        count: sessions.length
      });
    } catch (error) {
      logger.error('Failed to search sessions', {
        error: error instanceof Error ? error.message : 'Unknown error'
      });

      res.status(500).json({
        success: false,
        error: 'Failed to search sessions',
        message: error instanceof Error ? error.message : 'Unknown error'
      });
    }
  };
}

/**
 * POST /v1/sessions/prune
 * Prune old sessions
 */
export function createPruneHandler(sessionManager: SessionManager) {
  return async (req: Request, res: Response) => {
    try {
      const { olderThanDays = 30 } = req.body;

      const prunedCount = await sessionManager.pruneOldSessions(olderThanDays);

      res.json({
        success: true,
        prunedCount,
        olderThanDays,
        message: `Pruned ${prunedCount} sessions older than ${olderThanDays} days`
      });
    } catch (error) {
      logger.error('Failed to prune sessions', {
        error: error instanceof Error ? error.message : 'Unknown error'
      });

      res.status(500).json({
        success: false,
        error: 'Failed to prune sessions',
        message: error instanceof Error ? error.message : 'Unknown error'
      });
    }
  };
}

/**
 * GET /v1/sessions/stats
 * Get session storage statistics
 */
export function createStatsHandler(sessionManager: SessionManager) {
  return async (_req: Request, res: Response) => {
    try {
      const storageSize = await sessionManager.getStorageSize();
      const sessionCount = sessionManager.getSessionCount();

      res.json({
        success: true,
        stats: {
          sessionCount,
          storageSizeBytes: storageSize,
          storageSizeMB: (storageSize / (1024 * 1024)).toFixed(2)
        }
      });
    } catch (error) {
      logger.error('Failed to get session stats', {
        error: error instanceof Error ? error.message : 'Unknown error'
      });

      res.status(500).json({
        success: false,
        error: 'Failed to get session stats',
        message: error instanceof Error ? error.message : 'Unknown error'
      });
    }
  };
}
