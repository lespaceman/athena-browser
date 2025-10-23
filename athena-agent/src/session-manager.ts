/**
 * Session Manager
 *
 * Provides persistent session storage and conversation memory management.
 * Uses file-based storage for simplicity and reliability.
 */

import { promises as fs } from 'fs';
import { join } from 'path';
import { existsSync, mkdirSync, readFileSync } from 'fs';
import { randomBytes } from 'crypto';
import { Logger } from './logger';
import type {
  Session,
  SessionMetadata,
  SessionSummary,
  SessionListOptions,
  SessionStore,
  Message
} from './session-types';

const logger = new Logger('SessionManager');

export class SessionManager implements SessionStore {
  private sessionsDir: string;
  private indexPath: string;
  private sessionIndex: Map<string, SessionMetadata> = new Map();

  constructor(dataDir: string = '.athena-sessions') {
    this.sessionsDir = join(dataDir, 'sessions');
    this.indexPath = join(dataDir, 'index.json');

    // Initialize storage directory
    this.initializeStorage();
  }

  /**
   * Initialize storage directory and load session index
   */
  private initializeStorage(): void {
    try {
      // Create directories if they don't exist
      if (!existsSync(this.sessionsDir)) {
        mkdirSync(this.sessionsDir, { recursive: true, mode: 0o700 });
        logger.info('Created sessions directory', { path: this.sessionsDir });
      }

      // Load session index
      if (existsSync(this.indexPath)) {
        const indexData = readFileSync(this.indexPath, 'utf-8');
        const index = JSON.parse(indexData);
        this.sessionIndex = new Map(Object.entries(index));
        logger.info('Loaded session index', { count: this.sessionIndex.size });
      } else {
        logger.info('No existing session index found, starting fresh');
      }
    } catch (error) {
      logger.error('Failed to initialize storage', {
        error: error instanceof Error ? error.message : 'Unknown error'
      });
      throw error;
    }
  }

  /**
   * Save session index to disk
   */
  private async saveIndex(): Promise<void> {
    try {
      const indexObj = Object.fromEntries(this.sessionIndex);
      await fs.writeFile(
        this.indexPath,
        JSON.stringify(indexObj, null, 2),
        { mode: 0o600 }
      );
      logger.debug('Session index saved', { count: this.sessionIndex.size });
    } catch (error) {
      logger.error('Failed to save index', {
        error: error instanceof Error ? error.message : 'Unknown error'
      });
      throw error;
    }
  }

  /**
   * Get session file path
   */
  private getSessionPath(sessionId: string): string {
    return join(this.sessionsDir, `${sessionId}.json`);
  }

  /**
   * Generate unique session ID
   */
  private generateSessionId(): string {
    return `ses_${Date.now()}_${randomBytes(8).toString('hex')}`;
  }

  /**
   * Create a new session
   */
  async createSession(metadataInput: Omit<SessionMetadata, 'sessionId'>): Promise<Session> {
    const sessionId = this.generateSessionId();
    const metadata: SessionMetadata = {
      sessionId,
      ...metadataInput
    };

    const session: Session = {
      metadata
      // messages array removed - Claude SDK manages conversation history
    };

    try {
      // Save session file
      await fs.writeFile(
        this.getSessionPath(sessionId),
        JSON.stringify(session, null, 2),
        { mode: 0o600 }
      );

      // Update index
      this.sessionIndex.set(sessionId, metadata);
      await this.saveIndex();

      logger.info('Session created', {
        sessionId,
        title: metadata.title,
        model: metadata.model
      });

      return session;
    } catch (error) {
      logger.error('Failed to create session', {
        sessionId,
        error: error instanceof Error ? error.message : 'Unknown error'
      });
      throw error;
    }
  }

  /**
   * Get a session by ID
   */
  async getSession(sessionId: string): Promise<Session | null> {
    try {
      const sessionPath = this.getSessionPath(sessionId);

      if (!existsSync(sessionPath)) {
        logger.warn('Session not found', { sessionId });
        return null;
      }

      const data = await fs.readFile(sessionPath, 'utf-8');
      const session = JSON.parse(data) as Session;

      logger.debug('Session loaded', {
        sessionId,
        messageCount: session.metadata.messageCount
      });

      return session;
    } catch (error) {
      logger.error('Failed to load session', {
        sessionId,
        error: error instanceof Error ? error.message : 'Unknown error'
      });
      return null;
    }
  }

  /**
   * Update an existing session
   */
  async updateSession(session: Session): Promise<void> {
    try {
      // Update session file
      await fs.writeFile(
        this.getSessionPath(session.metadata.sessionId),
        JSON.stringify(session, null, 2),
        { mode: 0o600 }
      );

      // Update index
      this.sessionIndex.set(session.metadata.sessionId, session.metadata);
      await this.saveIndex();

      logger.debug('Session updated', {
        sessionId: session.metadata.sessionId,
        messageCount: session.metadata.messageCount
      });
    } catch (error) {
      logger.error('Failed to update session', {
        sessionId: session.metadata.sessionId,
        error: error instanceof Error ? error.message : 'Unknown error'
      });
      throw error;
    }
  }

  /**
   * Delete a session
   */
  async deleteSession(sessionId: string): Promise<void> {
    try {
      const sessionPath = this.getSessionPath(sessionId);

      if (existsSync(sessionPath)) {
        await fs.unlink(sessionPath);
      }

      this.sessionIndex.delete(sessionId);
      await this.saveIndex();

      logger.info('Session deleted', { sessionId });
    } catch (error) {
      logger.error('Failed to delete session', {
        sessionId,
        error: error instanceof Error ? error.message : 'Unknown error'
      });
      throw error;
    }
  }

  /**
   * List sessions with optional filtering and sorting
   */
  async listSessions(options: SessionListOptions = {}): Promise<SessionSummary[]> {
    const {
      limit,
      offset = 0,
      sortBy = 'lastUsedAt',
      sortOrder = 'desc',
      tags
    } = options;

    try {
      let sessions = Array.from(this.sessionIndex.values());

      // Filter by tags if specified
      if (tags && tags.length > 0) {
        sessions = sessions.filter(s =>
          s.tags?.some(tag => tags.includes(tag))
        );
      }

      // Sort sessions
      sessions.sort((a, b) => {
        const aVal = a[sortBy];
        const bVal = b[sortBy];
        const comparison = aVal < bVal ? -1 : aVal > bVal ? 1 : 0;
        return sortOrder === 'asc' ? comparison : -comparison;
      });

      // Apply pagination
      const start = offset;
      const end = limit ? offset + limit : sessions.length;
      const paginatedSessions = sessions.slice(start, end);

      // Convert to summaries
      const summaries: SessionSummary[] = paginatedSessions.map((metadata) => {
        return {
          sessionId: metadata.sessionId,
          title: metadata.title,
          createdAt: metadata.createdAt,
          lastUsedAt: metadata.lastUsedAt,
          messageCount: metadata.messageCount,
          preview: metadata.firstMessage?.substring(0, 100)
        };
      });

      logger.debug('Sessions listed', {
        total: sessions.length,
        returned: summaries.length,
        offset,
        limit
      });

      return summaries;
    } catch (error) {
      logger.error('Failed to list sessions', {
        error: error instanceof Error ? error.message : 'Unknown error'
      });
      return [];
    }
  }

  /**
   * Search sessions by title or content
   */
  async searchSessions(query: string): Promise<SessionSummary[]> {
    const lowerQuery = query.toLowerCase();

    try {
      const matchingSessions = Array.from(this.sessionIndex.values())
        .filter(metadata =>
          metadata.title.toLowerCase().includes(lowerQuery) ||
          metadata.tags?.some(tag => tag.toLowerCase().includes(lowerQuery))
        );

      const summaries: SessionSummary[] = matchingSessions.map((metadata) => {
        return {
          sessionId: metadata.sessionId,
          title: metadata.title,
          createdAt: metadata.createdAt,
          lastUsedAt: metadata.lastUsedAt,
          messageCount: metadata.messageCount,
          preview: metadata.firstMessage?.substring(0, 100)
        };
      });

      logger.debug('Session search completed', {
        query,
        results: summaries.length
      });

      return summaries;
    } catch (error) {
      logger.error('Failed to search sessions', {
        query,
        error: error instanceof Error ? error.message : 'Unknown error'
      });
      return [];
    }
  }

  /**
   * Add a message to a session
   * NOTE: Messages are NOT stored locally - Claude SDK manages conversation history.
   * This method only updates metadata (counts, tokens, cost) and captures the first user message for preview.
   */
  async addMessage(sessionId: string, message: Message): Promise<void> {
    try {
      const session = await this.getSession(sessionId);
      if (!session) {
        throw new Error(`Session ${sessionId} not found`);
      }

      // Capture first user message for preview/search
      if (message.role === 'user' && !session.metadata.firstMessage) {
        session.metadata.firstMessage = message.content;
      }

      // Update metadata only (no message storage)
      session.metadata.messageCount += 1;
      session.metadata.lastUsedAt = Date.now();

      if (message.tokens) {
        session.metadata.totalTokens.input += message.tokens.input || 0;
        session.metadata.totalTokens.output += message.tokens.output || 0;
      }

      if (message.cost) {
        session.metadata.totalCost += message.cost;
      }

      await this.updateSession(session);

      logger.debug('Message metadata updated', {
        sessionId,
        role: message.role,
        messageCount: session.metadata.messageCount
      });
    } catch (error) {
      logger.error('Failed to update message metadata', {
        sessionId,
        error: error instanceof Error ? error.message : 'Unknown error'
      });
      throw error;
    }
  }

  /**
   * Get messages from a session with pagination
   * NOTE: Returns empty array - Claude SDK manages conversation history internally.
   * Use session resumption to access conversation context.
   */
  async getMessages(
    sessionId: string,
    limit?: number,
    offset: number = 0
  ): Promise<Message[]> {
    try {
      const session = await this.getSession(sessionId);
      if (!session) {
        return [];
      }

      logger.debug('getMessages called - returning empty array (SDK manages history)', {
        sessionId,
        limit,
        offset
      });

      // Return empty array - Claude SDK manages conversation history
      return [];
    } catch (error) {
      logger.error('Failed to get messages', {
        sessionId,
        error: error instanceof Error ? error.message : 'Unknown error'
      });
      return [];
    }
  }

  /**
   * Update session metadata
   */
  async updateMetadata(
    sessionId: string,
    updates: Partial<SessionMetadata>
  ): Promise<void> {
    try {
      const session = await this.getSession(sessionId);
      if (!session) {
        throw new Error(`Session ${sessionId} not found`);
      }

      session.metadata = { ...session.metadata, ...updates };
      await this.updateSession(session);

      logger.debug('Session metadata updated', {
        sessionId,
        updates: Object.keys(updates)
      });
    } catch (error) {
      logger.error('Failed to update metadata', {
        sessionId,
        error: error instanceof Error ? error.message : 'Unknown error'
      });
      throw error;
    }
  }

  /**
   * Prune old sessions
   */
  async pruneOldSessions(olderThanDays: number): Promise<number> {
    const cutoffTime = Date.now() - (olderThanDays * 24 * 60 * 60 * 1000);
    let prunedCount = 0;

    try {
      const sessionsToDelete = Array.from(this.sessionIndex.entries())
        .filter(([_, metadata]) => metadata.lastUsedAt < cutoffTime)
        .map(([sessionId]) => sessionId);

      for (const sessionId of sessionsToDelete) {
        await this.deleteSession(sessionId);
        prunedCount++;
      }

      logger.info('Old sessions pruned', {
        count: prunedCount,
        olderThanDays
      });

      return prunedCount;
    } catch (error) {
      logger.error('Failed to prune sessions', {
        error: error instanceof Error ? error.message : 'Unknown error'
      });
      return prunedCount;
    }
  }

  /**
   * Get total storage size in bytes
   */
  async getStorageSize(): Promise<number> {
    try {
      let totalSize = 0;

      const files = await fs.readdir(this.sessionsDir);
      for (const file of files) {
        const filePath = join(this.sessionsDir, file);
        const stats = await fs.stat(filePath);
        totalSize += stats.size;
      }

      // Add index file size
      if (existsSync(this.indexPath)) {
        const indexStats = await fs.stat(this.indexPath);
        totalSize += indexStats.size;
      }

      logger.debug('Storage size calculated', {
        bytes: totalSize,
        mb: (totalSize / (1024 * 1024)).toFixed(2)
      });

      return totalSize;
    } catch (error) {
      logger.error('Failed to calculate storage size', {
        error: error instanceof Error ? error.message : 'Unknown error'
      });
      return 0;
    }
  }

  /**
   * Get session count
   */
  getSessionCount(): number {
    return this.sessionIndex.size;
  }

  /**
   * Get session by Claude SDK session ID
   * The Claude SDK generates its own session IDs, so we need to support both
   */
  async getSessionByClaudeId(claudeSessionId: string): Promise<Session | null> {
    try {
      // Search through all sessions to find one with matching Claude session ID
      const sessions = Array.from(this.sessionIndex.keys());

      for (const sessionId of sessions) {
        const session = await this.getSession(sessionId);
        if (session && session.metadata.claudeSessionId === claudeSessionId) {
          return session;
        }
      }

      logger.debug('No session found for Claude ID', { claudeSessionId });
      return null;
    } catch (error) {
      logger.error('Failed to find session by Claude ID', {
        claudeSessionId,
        error: error instanceof Error ? error.message : 'Unknown error'
      });
      return null;
    }
  }
}
