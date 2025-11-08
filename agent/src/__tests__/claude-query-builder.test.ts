/**
 * Claude Query Builder Tests
 */

import { describe, it, expect } from 'vitest';
import { ClaudeQueryBuilder } from '../claude/query-builder';
import type { AthenaAgentConfig } from '../server/types';

// Mock agent config
const mockConfig: AthenaAgentConfig = {
  socketPath: '/tmp/test.sock',
  cwd: '/test/path',
  model: 'claude-sonnet-4-20250514',
  permissionMode: 'default',
  logLevel: 'info'
};

describe('ClaudeQueryBuilder', () => {
  describe('buildQueryConfig', () => {
    it('should build basic query config', () => {
      const config = ClaudeQueryBuilder.buildQueryConfig(
        mockConfig,
        'test prompt'
      );

      expect(config.cwd).toBe('/test/path');
      expect(config.model).toBe('claude-sonnet-4-20250514');
      expect(config.permissionMode).toBe('default');
      expect(config.prompt).toBe('test prompt');
      expect(config.enableMcp).toBe(true); // Default is true
    });

    it('should include MCP enablement when specified', () => {
      const config = ClaudeQueryBuilder.buildQueryConfig(
        mockConfig,
        'test prompt',
        undefined,
        { enableMcp: true }
      );

      expect(config.enableMcp).toBe(true);
    });

    it('should disable MCP when specified', () => {
      const config = ClaudeQueryBuilder.buildQueryConfig(
        mockConfig,
        'test prompt',
        undefined,
        { enableMcp: false }
      );

      expect(config.enableMcp).toBe(false);
    });

    it('should include Claude session ID when provided', () => {
      const config = ClaudeQueryBuilder.buildQueryConfig(
        mockConfig,
        'test prompt',
        'session-123'
      );

      expect(config.claudeSessionId).toBe('session-123');
    });

    it('should include fork session option when provided', () => {
      const config = ClaudeQueryBuilder.buildQueryConfig(
        mockConfig,
        'test prompt',
        undefined,
        { forkSession: true }
      );

      expect(config.forkSession).toBe(true);
    });

    it('should include partial messages option when provided', () => {
      const config = ClaudeQueryBuilder.buildQueryConfig(
        mockConfig,
        'test prompt',
        undefined,
        { includePartialMessages: true }
      );

      expect(config.includePartialMessages).toBe(true);
    });

    it('should include max thinking tokens when configured', () => {
      const config = ClaudeQueryBuilder.buildQueryConfig(
        { ...mockConfig, maxThinkingTokens: 5000 },
        'test prompt'
      );

      expect(config.maxThinkingTokens).toBe(5000);
    });

    it('should include max turns when configured', () => {
      const config = ClaudeQueryBuilder.buildQueryConfig(
        { ...mockConfig, maxTurns: 10 },
        'test prompt'
      );

      expect(config.maxTurns).toBe(10);
    });
  });

  describe('buildQueryOptions', () => {
    it('should build basic query options', () => {
      const queryConfig = ClaudeQueryBuilder.buildQueryConfig(
        mockConfig,
        'test prompt'
      );

      const options = ClaudeQueryBuilder.buildQueryOptions(queryConfig);

      expect(options.cwd).toBe('/test/path');
      expect(options.model).toBe('claude-sonnet-4-20250514');
      expect(options.permissionMode).toBe('default');
      expect(options.settingSources).toEqual(['user', 'project', 'local']);
    });

    it('should use preset system prompt when MCP disabled', () => {
      const queryConfig = ClaudeQueryBuilder.buildQueryConfig(
        mockConfig,
        'test prompt',
        undefined,
        { enableMcp: false }
      );

      const options = ClaudeQueryBuilder.buildQueryOptions(queryConfig);

      expect(options.systemPrompt).toEqual({
        type: 'preset',
        preset: 'claude_code'
      });
    });

    it('should use custom system prompt with MCP enabled', () => {
      const queryConfig = ClaudeQueryBuilder.buildQueryConfig(
        mockConfig,
        'test prompt',
        undefined,
        { enableMcp: true }
      );

      const options = ClaudeQueryBuilder.buildQueryOptions(queryConfig);

      expect(typeof options.systemPrompt).toBe('string');
      expect(options.systemPrompt).toContain('browser automation');
      expect(options.systemPrompt).toContain('CLAUDE.md');
    });

    it('should include MCP servers when MCP enabled', () => {
      const queryConfig = ClaudeQueryBuilder.buildQueryConfig(
        mockConfig,
        'test prompt',
        undefined,
        { enableMcp: true }
      );

      const options = ClaudeQueryBuilder.buildQueryOptions(queryConfig);

      expect(options.mcpServers).toBeDefined();
      expect(options.mcpServers?.['athena-browser']).toBeDefined();
      expect(options.mcpServers?.['athena-browser'].command).toBe('npx');
      expect(options.mcpServers?.['athena-browser'].args).toEqual(['-y', 'athena-browser-mcp']);
      expect(options.mcpServers?.['athena-browser'].env?.CEF_BRIDGE_PORT).toBe('9222');
    });

    it('should not include MCP servers when MCP disabled', () => {
      const queryConfig = ClaudeQueryBuilder.buildQueryConfig(
        mockConfig,
        'test prompt',
        undefined,
        { enableMcp: false }
      );

      const options = ClaudeQueryBuilder.buildQueryOptions(queryConfig);

      expect(options.mcpServers).toBeUndefined();
    });

    it('should include sub-agents when MCP enabled', () => {
      const queryConfig = ClaudeQueryBuilder.buildQueryConfig(
        mockConfig,
        'test prompt',
        undefined,
        { enableMcp: true }
      );

      const options = ClaudeQueryBuilder.buildQueryOptions(queryConfig);

      expect(options.agents).toBeDefined();
      expect(options.agents?.['web-analyzer']).toBeDefined();
      expect(options.agents?.['navigation-expert']).toBeDefined();
      expect(options.agents?.['form-automation']).toBeDefined();
      expect(options.agents?.['screenshot-analyst']).toBeDefined();
    });

    it('should not include sub-agents when MCP disabled', () => {
      const queryConfig = ClaudeQueryBuilder.buildQueryConfig(
        mockConfig,
        'test prompt',
        undefined,
        { enableMcp: false }
      );

      const options = ClaudeQueryBuilder.buildQueryOptions(queryConfig);

      expect(options.agents).toBeUndefined();
    });

    it('should select tools based on prompt when MCP enabled', () => {
      const queryConfig = ClaudeQueryBuilder.buildQueryConfig(
        mockConfig,
        'navigate to google.com',
        undefined,
        { enableMcp: true }
      );

      const options = ClaudeQueryBuilder.buildQueryOptions(queryConfig);

      expect(Array.isArray(options.allowedTools)).toBe(true);
      expect(options.allowedTools.length).toBeGreaterThan(0);
    });

    it('should return empty tools array when MCP disabled', () => {
      const queryConfig = ClaudeQueryBuilder.buildQueryConfig(
        mockConfig,
        'navigate to google.com',
        undefined,
        { enableMcp: false }
      );

      const options = ClaudeQueryBuilder.buildQueryOptions(queryConfig);

      expect(Array.isArray(options.allowedTools)).toBe(true);
      expect(options.allowedTools.length).toBe(0);
    });

    it('should include permission callback for default mode', () => {
      const queryConfig = ClaudeQueryBuilder.buildQueryConfig(
        { ...mockConfig, permissionMode: 'default' },
        'test prompt'
      );

      const options = ClaudeQueryBuilder.buildQueryOptions(queryConfig);

      expect(options.canUseTool).toBeDefined();
      expect(typeof options.canUseTool).toBe('function');
    });

    it('should not include permission callback for non-default modes', () => {
      const queryConfig = ClaudeQueryBuilder.buildQueryConfig(
        { ...mockConfig, permissionMode: 'acceptEdits' },
        'test prompt'
      );

      const options = ClaudeQueryBuilder.buildQueryOptions(queryConfig);

      expect(options.canUseTool).toBeUndefined();
    });

    it('should include resume session ID when provided', () => {
      const queryConfig = ClaudeQueryBuilder.buildQueryConfig(
        mockConfig,
        'test prompt',
        'session-123'
      );

      const options = ClaudeQueryBuilder.buildQueryOptions(queryConfig);

      expect(options.resume).toBe('session-123');
    });

    it('should include fork session flag when set', () => {
      const queryConfig = ClaudeQueryBuilder.buildQueryConfig(
        mockConfig,
        'test prompt',
        undefined,
        { forkSession: true }
      );

      const options = ClaudeQueryBuilder.buildQueryOptions(queryConfig);

      expect(options.forkSession).toBe(true);
    });

    it('should include partial messages flag when set', () => {
      const queryConfig = ClaudeQueryBuilder.buildQueryConfig(
        mockConfig,
        'test prompt',
        undefined,
        { includePartialMessages: true }
      );

      const options = ClaudeQueryBuilder.buildQueryOptions(queryConfig);

      expect(options.includePartialMessages).toBe(true);
    });

    it('should include max thinking tokens when configured', () => {
      const queryConfig = ClaudeQueryBuilder.buildQueryConfig(
        { ...mockConfig, maxThinkingTokens: 5000 },
        'test prompt'
      );

      const options = ClaudeQueryBuilder.buildQueryOptions(queryConfig);

      expect(options.maxThinkingTokens).toBe(5000);
    });

    it('should include max turns when configured', () => {
      const queryConfig = ClaudeQueryBuilder.buildQueryConfig(
        { ...mockConfig, maxTurns: 10 },
        'test prompt'
      );

      const options = ClaudeQueryBuilder.buildQueryOptions(queryConfig);

      expect(options.maxTurns).toBe(10);
    });
  });
});
