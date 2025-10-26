/**
 * Session Manager Tests
 *
 * Tests for persistent session storage and conversation memory management.
 */

import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import { SessionManager } from '../src/session/manager.js';
import { existsSync, rmSync } from 'fs';
import type { Message } from '../src/session/types.js';

const TEST_DATA_DIR = '.test-athena-sessions';

describe('SessionManager', () => {
  let sessionManager: SessionManager;

  beforeEach(() => {
    // Clean up test data directory before each test
    if (existsSync(TEST_DATA_DIR)) {
      rmSync(TEST_DATA_DIR, { recursive: true, force: true });
    }
    sessionManager = new SessionManager(TEST_DATA_DIR);
  });

  afterEach(() => {
    // Clean up after each test
    if (existsSync(TEST_DATA_DIR)) {
      rmSync(TEST_DATA_DIR, { recursive: true, force: true });
    }
  });

  describe('Session Creation', () => {
    it('should create a new session', async () => {
      const session = await sessionManager.createSession({
        title: 'Test Session',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5',
        tags: ['test']
      });

      expect(session).toBeDefined();
      expect(session.metadata.sessionId).toBeTruthy();
      expect(session.metadata.title).toBe('Test Session');
      // Phase 2: messages array removed - SDK manages conversation history
      expect(session.messages).toBeUndefined();
    });

    it('should generate unique session IDs', async () => {
      const session1 = await sessionManager.createSession({
        title: 'Session 1',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5'
      });

      const session2 = await sessionManager.createSession({
        title: 'Session 2',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5'
      });

      expect(session1.metadata.sessionId).not.toBe(session2.metadata.sessionId);
    });
  });

  describe('Session Retrieval', () => {
    it('should retrieve an existing session', async () => {
      const created = await sessionManager.createSession({
        title: 'Retrieve Test',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5'
      });

      const retrieved = await sessionManager.getSession(created.metadata.sessionId);

      expect(retrieved).toBeDefined();
      expect(retrieved?.metadata.sessionId).toBe(created.metadata.sessionId);
      expect(retrieved?.metadata.title).toBe('Retrieve Test');
    });

    it('should return null for non-existent session', async () => {
      const session = await sessionManager.getSession('non-existent-id');
      expect(session).toBeNull();
    });
  });

  describe('Message Management', () => {
    it('should add messages to a session', async () => {
      const session = await sessionManager.createSession({
        title: 'Message Test',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5'
      });

      const message: Message = {
        role: 'user',
        content: 'Hello, Claude!',
        timestamp: Date.now()
      };

      await sessionManager.addMessage(session.metadata.sessionId, message);

      const updated = await sessionManager.getSession(session.metadata.sessionId);
      // Phase 2: Messages not stored locally - only metadata updated
      expect(updated?.metadata.messageCount).toBe(1);
      expect(updated?.metadata.firstMessage).toBe('Hello, Claude!');
    });

    it('should update token counts when adding messages', async () => {
      const session = await sessionManager.createSession({
        title: 'Token Test',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5'
      });

      const message: Message = {
        role: 'assistant',
        content: 'Response',
        timestamp: Date.now(),
        tokens: { input: 100, output: 50 },
        cost: 0.0015
      };

      await sessionManager.addMessage(session.metadata.sessionId, message);

      const updated = await sessionManager.getSession(session.metadata.sessionId);
      expect(updated?.metadata.totalTokens.input).toBe(100);
      expect(updated?.metadata.totalTokens.output).toBe(50);
      expect(updated?.metadata.totalCost).toBe(0.0015);
    });

    it('should retrieve messages with pagination', async () => {
      const session = await sessionManager.createSession({
        title: 'Pagination Test',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5'
      });

      // Add 10 messages
      for (let i = 0; i < 10; i++) {
        await sessionManager.addMessage(session.metadata.sessionId, {
          role: 'user',
          content: `Message ${i}`,
          timestamp: Date.now()
        });
      }

      // Phase 2: getMessages returns empty array - SDK manages history
      const messages = await sessionManager.getMessages(session.metadata.sessionId, 5, 0);
      expect(messages).toEqual([]);

      // But metadata should still track the count
      const updated = await sessionManager.getSession(session.metadata.sessionId);
      expect(updated?.metadata.messageCount).toBe(10);
    });
  });

  describe('Session Listing and Search', () => {
    it('should list all sessions', async () => {
      await sessionManager.createSession({
        title: 'Session 1',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5'
      });

      await sessionManager.createSession({
        title: 'Session 2',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5'
      });

      const sessions = await sessionManager.listSessions();
      expect(sessions).toHaveLength(2);
    });

    it('should sort sessions by lastUsedAt descending by default', async () => {
      const session1 = await sessionManager.createSession({
        title: 'Old Session',
        createdAt: Date.now() - 10000,
        lastUsedAt: Date.now() - 10000,
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5'
      });

      const session2 = await sessionManager.createSession({
        title: 'New Session',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5'
      });

      const sessions = await sessionManager.listSessions();
      expect(sessions[0].title).toBe('New Session');
      expect(sessions[1].title).toBe('Old Session');
    });

    it('should search sessions by title', async () => {
      await sessionManager.createSession({
        title: 'Browser Testing',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5'
      });

      await sessionManager.createSession({
        title: 'API Development',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5'
      });

      const results = await sessionManager.searchSessions('browser');
      expect(results).toHaveLength(1);
      expect(results[0].title).toBe('Browser Testing');
    });

    it('should filter sessions by tags', async () => {
      await sessionManager.createSession({
        title: 'Tagged Session 1',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5',
        tags: ['test', 'debug']
      });

      await sessionManager.createSession({
        title: 'Tagged Session 2',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5',
        tags: ['production']
      });

      const results = await sessionManager.listSessions({ tags: ['test'] });
      expect(results).toHaveLength(1);
      expect(results[0].title).toBe('Tagged Session 1');
    });
  });

  describe('Session Update and Delete', () => {
    it('should update session metadata', async () => {
      const session = await sessionManager.createSession({
        title: 'Original Title',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5'
      });

      await sessionManager.updateMetadata(session.metadata.sessionId, {
        title: 'Updated Title',
        tags: ['updated']
      });

      const updated = await sessionManager.getSession(session.metadata.sessionId);
      expect(updated?.metadata.title).toBe('Updated Title');
      expect(updated?.metadata.tags).toContain('updated');
    });

    it('should delete a session', async () => {
      const session = await sessionManager.createSession({
        title: 'To Delete',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5'
      });

      await sessionManager.deleteSession(session.metadata.sessionId);

      const retrieved = await sessionManager.getSession(session.metadata.sessionId);
      expect(retrieved).toBeNull();
    });
  });

  describe('Session Pruning', () => {
    it('should prune old sessions', async () => {
      // Create old session
      await sessionManager.createSession({
        title: 'Old Session',
        createdAt: Date.now() - (40 * 24 * 60 * 60 * 1000), // 40 days ago
        lastUsedAt: Date.now() - (40 * 24 * 60 * 60 * 1000),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5'
      });

      // Create recent session
      await sessionManager.createSession({
        title: 'Recent Session',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5'
      });

      const prunedCount = await sessionManager.pruneOldSessions(30);
      expect(prunedCount).toBe(1);

      const sessions = await sessionManager.listSessions();
      expect(sessions).toHaveLength(1);
      expect(sessions[0].title).toBe('Recent Session');
    });
  });

  describe('Storage Statistics', () => {
    it('should calculate storage size', async () => {
      await sessionManager.createSession({
        title: 'Test Session',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5'
      });

      const size = await sessionManager.getStorageSize();
      expect(size).toBeGreaterThan(0);
    });

    it('should count sessions', async () => {
      expect(sessionManager.getSessionCount()).toBe(0);

      await sessionManager.createSession({
        title: 'Session 1',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5'
      });

      expect(sessionManager.getSessionCount()).toBe(1);

      await sessionManager.createSession({
        title: 'Session 2',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5'
      });

      expect(sessionManager.getSessionCount()).toBe(2);
    });
  });
});
