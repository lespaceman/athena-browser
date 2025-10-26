/**
 * Session Resumption Tests
 *
 * Tests for session resumption functionality including:
 * - Claude session ID lookup
 * - Session metadata management
 * - Session lifecycle with Claude SDK integration
 */

import { describe, it, expect, beforeEach } from 'vitest';
import { SessionManager } from '../src/session/manager.js';
import type { Session, SessionMetadata } from '../src/session/types.js';
import { existsSync, mkdirSync, rmSync } from 'fs';

const TEST_DATA_DIR = '.test-sessions-resumption';

describe('Session Resumption', () => {
  let sessionManager: SessionManager;

  beforeEach(() => {
    // Clean up test directory
    if (existsSync(TEST_DATA_DIR)) {
      rmSync(TEST_DATA_DIR, { recursive: true, force: true });
    }
    mkdirSync(TEST_DATA_DIR, { recursive: true });

    sessionManager = new SessionManager(TEST_DATA_DIR);
  });

  describe('Claude Session ID Lookup', () => {
    it('should find session by claudeSessionId in metadata', async () => {
      // Create a session with a Claude session ID
      const session = await sessionManager.createSession({
        title: 'Test Session',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5',
        tags: []
      });

      const claudeSessionId = 'claude_ses_abc123';

      // Update metadata with Claude session ID
      await sessionManager.updateMetadata(session.metadata.sessionId, {
        claudeSessionId
      });

      // BUG: getSessionByClaudeId() currently checks wrong field
      // This test SHOULD pass but currently FAILS
      const foundSession = await sessionManager.getSessionByClaudeId(claudeSessionId);

      expect(foundSession).not.toBeNull();
      expect(foundSession?.metadata.claudeSessionId).toBe(claudeSessionId);
      expect(foundSession?.metadata.sessionId).toBe(session.metadata.sessionId);
    });

    it('should return null for non-existent Claude session ID', async () => {
      const foundSession = await sessionManager.getSessionByClaudeId('non_existent_id');

      expect(foundSession).toBeNull();
    });

    it('should handle sessions without Claude session ID', async () => {
      // Create session without Claude ID
      await sessionManager.createSession({
        title: 'No Claude ID',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5',
        tags: []
      });

      const foundSession = await sessionManager.getSessionByClaudeId('some_claude_id');

      expect(foundSession).toBeNull();
    });
  });

  describe('Session Metadata Persistence', () => {
    it('should maintain Claude session ID across updates', async () => {
      // Create session
      const session = await sessionManager.createSession({
        title: 'Consistency Test',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5',
        tags: []
      });

      const claudeSessionId = 'claude_ses_xyz789';

      // First update: Set Claude session ID
      await sessionManager.updateMetadata(session.metadata.sessionId, {
        claudeSessionId
      });

      // Verify it's saved
      let loadedSession = await sessionManager.getSession(session.metadata.sessionId);
      expect(loadedSession?.metadata.claudeSessionId).toBe(claudeSessionId);

      // Second update: Update other fields
      await sessionManager.updateMetadata(session.metadata.sessionId, {
        lastUsedAt: Date.now() + 1000,
        title: 'Updated Title'
      });

      // Verify Claude session ID is still there
      loadedSession = await sessionManager.getSession(session.metadata.sessionId);
      expect(loadedSession?.metadata.claudeSessionId).toBe(claudeSessionId);
      expect(loadedSession?.metadata.title).toBe('Updated Title');
    });

    it('should allow updating Claude session ID', async () => {
      const session = await sessionManager.createSession({
        title: 'Update Test',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5',
        tags: []
      });

      // Set initial Claude session ID
      await sessionManager.updateMetadata(session.metadata.sessionId, {
        claudeSessionId: 'claude_ses_old'
      });

      // Update to new Claude session ID
      await sessionManager.updateMetadata(session.metadata.sessionId, {
        claudeSessionId: 'claude_ses_new'
      });

      const loadedSession = await sessionManager.getSession(session.metadata.sessionId);
      expect(loadedSession?.metadata.claudeSessionId).toBe('claude_ses_new');
    });
  });

  describe('Session lifecycle with Claude SDK integration', () => {
    it('should support session creation → Claude ID assignment → resumption flow', async () => {
      // Step 1: Create new session (simulates first user message)
      const newSession = await sessionManager.createSession({
        title: 'User starts conversation',
        createdAt: Date.now(),
        lastUsedAt: Date.now(),
        messageCount: 0,
        totalTokens: { input: 0, output: 0 },
        totalCost: 0,
        model: 'claude-sonnet-4-5',
        tags: []
      });

      expect(newSession.metadata.claudeSessionId).toBeUndefined();

      // Step 2: After Claude SDK responds, save its session ID
      const claudeSessionId = 'claude_sdk_session_123';
      await sessionManager.updateMetadata(newSession.metadata.sessionId, {
        claudeSessionId,
        messageCount: 1,
        lastUsedAt: Date.now()
      });

      // Step 3: User sends another message (resumption)
      const resumedSession = await sessionManager.getSession(newSession.metadata.sessionId);
      expect(resumedSession?.metadata.claudeSessionId).toBe(claudeSessionId);

      // Step 4: Verify we can find by Claude ID for debugging
      const foundByClaudeId = await sessionManager.getSessionByClaudeId(claudeSessionId);
      expect(foundByClaudeId?.metadata.sessionId).toBe(newSession.metadata.sessionId);
    });
  });
});
