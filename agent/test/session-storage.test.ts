/**
 * Session Storage Tests
 *
 * Tests for session storage behavior:
 * - Sessions store only metadata (not full message arrays)
 * - First message stored in metadata for preview
 * - Message history managed by Claude SDK
 * - Metadata updates (tokens, counts, timestamps)
 */

import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import { SessionManager } from '../src/session-manager.js';
import { existsSync, rmSync } from 'fs';
import type { Message } from '../src/session-types.js';

describe('Session Storage', () => {
  const testDataDir = '.test-sessions-storage';
  let sessionManager: SessionManager;

  beforeEach(() => {
    // Clean up test directory
    if (existsSync(testDataDir)) {
      rmSync(testDataDir, { recursive: true, force: true });
    }
    sessionManager = new SessionManager(testDataDir);
  });

  afterEach(() => {
    // Clean up after tests
    if (existsSync(testDataDir)) {
      rmSync(testDataDir, { recursive: true, force: true });
    }
  });

  describe('Session Structure', () => {
    it('should not store messages array in session', async () => {
      // Create a session
      const session = await sessionManager.createSession({
        title: 'Test Session',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5-20250929',
        tags: []
      });

      // Add a message
      const message: Message = {
        role: 'user',
        content: 'Hello, this is a test message',
        timestamp: Date.now()
      };
      await sessionManager.addMessage(session.metadata.sessionId, message);

      // Reload the session
      const reloadedSession = await sessionManager.getSession(session.metadata.sessionId);

      // Session should NOT have messages array (or it should be empty)
      expect(reloadedSession).toBeDefined();
      expect(reloadedSession!.messages).toBeUndefined();
    });

    it('should store first user message in metadata for preview', async () => {
      // Create a session
      const session = await sessionManager.createSession({
        title: 'Test Session',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5-20250929',
        tags: []
      });

      // Add first user message
      const firstMessage: Message = {
        role: 'user',
        content: 'This is my first question about browser automation',
        timestamp: Date.now()
      };
      await sessionManager.addMessage(session.metadata.sessionId, firstMessage);

      // Add assistant response
      const assistantMessage: Message = {
        role: 'assistant',
        content: 'Here is my response',
        timestamp: Date.now()
      };
      await sessionManager.addMessage(session.metadata.sessionId, assistantMessage);

      // Reload the session
      const reloadedSession = await sessionManager.getSession(session.metadata.sessionId);

      // First user message should be stored in metadata
      expect(reloadedSession).toBeDefined();
      expect(reloadedSession!.metadata.firstMessage).toBe(firstMessage.content);
    });

    it('should not overwrite firstMessage when adding more messages', async () => {
      // Create a session
      const session = await sessionManager.createSession({
        title: 'Test Session',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5-20250929',
        tags: []
      });

      // Add first user message
      const firstMessage: Message = {
        role: 'user',
        content: 'First message',
        timestamp: Date.now()
      };
      await sessionManager.addMessage(session.metadata.sessionId, firstMessage);

      // Add second user message
      const secondMessage: Message = {
        role: 'user',
        content: 'Second message',
        timestamp: Date.now()
      };
      await sessionManager.addMessage(session.metadata.sessionId, secondMessage);

      // Reload the session
      const reloadedSession = await sessionManager.getSession(session.metadata.sessionId);

      // First message should still be the first one
      expect(reloadedSession).toBeDefined();
      expect(reloadedSession!.metadata.firstMessage).toBe('First message');
    });
  });

  describe('getMessages()', () => {
    it('should return empty array since SDK manages history', async () => {
      // Create a session
      const session = await sessionManager.createSession({
        title: 'Test Session',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5-20250929',
        tags: []
      });

      // Add messages
      await sessionManager.addMessage(session.metadata.sessionId, {
        role: 'user',
        content: 'Test message 1',
        timestamp: Date.now()
      });

      await sessionManager.addMessage(session.metadata.sessionId, {
        role: 'assistant',
        content: 'Test response 1',
        timestamp: Date.now()
      });

      // getMessages should return empty array
      const messages = await sessionManager.getMessages(session.metadata.sessionId);
      expect(messages).toEqual([]);
    });
  });

  describe('addMessage() Metadata Updates', () => {
    it('should update messageCount when adding messages', async () => {
      // Create a session
      const session = await sessionManager.createSession({
        title: 'Test Session',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5-20250929',
        tags: []
      });

      // Add first message
      await sessionManager.addMessage(session.metadata.sessionId, {
        role: 'user',
        content: 'Test message',
        timestamp: Date.now()
      });

      // Check message count
      let reloadedSession = await sessionManager.getSession(session.metadata.sessionId);
      expect(reloadedSession!.metadata.messageCount).toBe(1);

      // Add second message
      await sessionManager.addMessage(session.metadata.sessionId, {
        role: 'assistant',
        content: 'Test response',
        timestamp: Date.now()
      });

      // Check message count again
      reloadedSession = await sessionManager.getSession(session.metadata.sessionId);
      expect(reloadedSession!.metadata.messageCount).toBe(2);
    });

    it('should update token counts when adding messages with token info', async () => {
      // Create a session
      const session = await sessionManager.createSession({
        title: 'Test Session',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5-20250929',
        tags: []
      });

      // Add message with tokens
      await sessionManager.addMessage(session.metadata.sessionId, {
        role: 'user',
        content: 'Test message',
        timestamp: Date.now(),
        tokens: { input: 100, output: 0 }
      });

      await sessionManager.addMessage(session.metadata.sessionId, {
        role: 'assistant',
        content: 'Test response',
        timestamp: Date.now(),
        tokens: { input: 0, output: 200 }
      });

      // Check token counts
      const reloadedSession = await sessionManager.getSession(session.metadata.sessionId);
      expect(reloadedSession!.metadata.totalTokens.input).toBe(100);
      expect(reloadedSession!.metadata.totalTokens.output).toBe(200);
    });

    it('should update cost when adding messages with cost info', async () => {
      // Create a session
      const session = await sessionManager.createSession({
        title: 'Test Session',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5-20250929',
        tags: []
      });

      // Add messages with costs
      await sessionManager.addMessage(session.metadata.sessionId, {
        role: 'user',
        content: 'Test message',
        timestamp: Date.now(),
        cost: 0.001
      });

      await sessionManager.addMessage(session.metadata.sessionId, {
        role: 'assistant',
        content: 'Test response',
        timestamp: Date.now(),
        cost: 0.002
      });

      // Check total cost
      const reloadedSession = await sessionManager.getSession(session.metadata.sessionId);
      expect(reloadedSession!.metadata.totalCost).toBe(0.003);
    });

    it('should update lastUsedAt when adding messages', async () => {
      // Create a session
      const session = await sessionManager.createSession({
        title: 'Test Session',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5-20250929',
        tags: []
      });

      const initialLastUsedAt = session.metadata.lastUsedAt;

      // Wait a bit to ensure timestamp changes
      await new Promise(resolve => setTimeout(resolve, 10));

      // Add message
      await sessionManager.addMessage(session.metadata.sessionId, {
        role: 'user',
        content: 'Test message',
        timestamp: Date.now()
      });

      // Check lastUsedAt was updated
      const reloadedSession = await sessionManager.getSession(session.metadata.sessionId);
      expect(reloadedSession!.metadata.lastUsedAt).toBeGreaterThan(initialLastUsedAt);
    });
  });

  describe('Session Listing and Search with firstMessage', () => {
    it('should include firstMessage preview in session summaries', async () => {
      // Create a session
      const session = await sessionManager.createSession({
        title: 'Test Session',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5-20250929',
        tags: []
      });

      // Add first message
      await sessionManager.addMessage(session.metadata.sessionId, {
        role: 'user',
        content: 'This is a very long message that should be truncated in the preview to only show the first 100 characters or so for display purposes',
        timestamp: Date.now()
      });

      // List sessions
      const summaries = await sessionManager.listSessions();
      expect(summaries).toHaveLength(1);
      expect(summaries[0].preview).toBeDefined();
      expect(summaries[0].preview).toBe('This is a very long message that should be truncated in the preview to only show the first 100 chara');
    });
  });
});
