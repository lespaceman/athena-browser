/**
 * Tool Permission Tests
 *
 * Tests for tool permission management:
 * - Static permissions handled by allowedTools array
 * - Dynamic logic (JavaScript execution confirmation) in canUseTool()
 * - Browser-only tool restrictions
 */

import { describe, it, expect, beforeEach } from 'vitest';
import { ClaudeClient } from '../src/claude/client.js';
import type { AthenaAgentConfig } from '../src/server/types.js';
import { SessionManager } from '../src/session/manager.js';

describe('Tool Permissions', () => {
  let client: ClaudeClient;
  let config: AthenaAgentConfig;
  let sessionManager: SessionManager;

  beforeEach(async () => {
    config = {
      model: 'claude-sonnet-4-20250514',
      cwd: process.cwd(),
      permissionMode: 'default',
      maxThinkingTokens: 10000,
      maxTurns: 10
    };

    sessionManager = new SessionManager();
    client = new ClaudeClient(config, undefined, sessionManager);
  });

  describe('canUseTool() Simplification', () => {
    it('should only handle JavaScript execution confirmation (dynamic logic)', async () => {
      // Access private method via type assertion for testing
      const canUseTool = (client as any).canUseTool.bind(client);

      // JS execution should require confirmation
      const jsResult = await canUseTool('mcp__athena-browser__browser_execute_js', {
        code: 'console.log("test")'
      });

      expect(jsResult.behavior).toBe('ask');
      expect(jsResult.message).toContain('Allow JavaScript execution');
      expect(jsResult.message).toContain('console.log');
    });

    it('should allow all non-JS tools (static permissions handled by allowedTools)', async () => {
      const canUseTool = (client as any).canUseTool.bind(client);

      // All browser tools should be allowed by default
      const browserTools = [
        'mcp__athena-browser__browser_navigate',
        'mcp__athena-browser__browser_get_url',
        'mcp__athena-browser__browser_get_html',
        'mcp__athena-browser__browser_get_page_summary',
        'mcp__athena-browser__browser_get_interactive_elements',
        'mcp__athena-browser__browser_screenshot'
      ];

      for (const tool of browserTools) {
        const result = await canUseTool(tool, {});
        expect(result.behavior).toBe('allow');
      }
    });

    it('should not block file system tools in canUseTool (handled by allowedTools)', async () => {
      const canUseTool = (client as any).canUseTool.bind(client);

      // File system tools are NOT blocked in canUseTool anymore
      // They're excluded from allowedTools array instead
      const fileSystemTools = ['Read', 'Write', 'Edit', 'Bash'];

      for (const tool of fileSystemTools) {
        const result = await canUseTool(tool, {});
        // canUseTool allows everything except JS execution
        expect(result.behavior).toBe('allow');
      }
    });

    it('should truncate long JavaScript code in confirmation message', async () => {
      const canUseTool = (client as any).canUseTool.bind(client);

      const longCode = 'x'.repeat(200);
      const result = await canUseTool('mcp__athena-browser__browser_execute_js', {
        code: longCode
      });

      expect(result.behavior).toBe('ask');
      expect(result.message).toContain('...');
      expect(result.message.length).toBeLessThan(200); // Message is truncated
    });
  });

  describe('getToolsForPrompt() - Static Permission Control', () => {
    it('should return read-only tools for analysis prompts', () => {
      const getToolsForPrompt = (client as any).getToolsForPrompt.bind(client);

      // Create client with MCP server
      const mcpServer = {
        command: 'test',
        args: [],
        env: {},
        instance: {} as any
      };
      const clientWithMcp = new ClaudeClient(config, mcpServer, sessionManager);
      const getTools = (clientWithMcp as any).getToolsForPrompt.bind(clientWithMcp);

      const tools = getTools('show me the page summary and analyze the content');

      // Should include read-only tools
      expect(tools).toContain('mcp__athena-browser__browser_get_page_summary');
      expect(tools).toContain('mcp__athena-browser__browser_get_url');
      expect(tools).toContain('mcp__athena-browser__browser_screenshot');

      // Should NOT include interactive tools
      expect(tools).not.toContain('mcp__athena-browser__browser_execute_js');
      expect(tools).not.toContain('mcp__athena-browser__browser_navigate');
    });

    it('should return all tools for interactive prompts', () => {
      const mcpServer = {
        command: 'test',
        args: [],
        env: {},
        instance: {} as any
      };
      const clientWithMcp = new ClaudeClient(config, mcpServer, sessionManager);
      const getTools = (clientWithMcp as any).getToolsForPrompt.bind(clientWithMcp);

      const tools = getTools('navigate to google.com and click the search button');

      // Should include navigation tools
      expect(tools).toContain('mcp__athena-browser__browser_navigate');
      expect(tools).toContain('mcp__athena-browser__browser_execute_js');

      // Should also include read-only tools
      expect(tools).toContain('mcp__athena-browser__browser_get_page_summary');
    });

    it('should return empty array when no MCP server configured', () => {
      const getTools = (client as any).getToolsForPrompt.bind(client);

      const tools = getTools('test prompt');

      expect(tools).toEqual([]);
    });

    it('should only return browser tools (no file system or CLI tools)', () => {
      const mcpServer = {
        command: 'test',
        args: [],
        env: {},
        instance: {} as any
      };
      const clientWithMcp = new ClaudeClient(config, mcpServer, sessionManager);
      const getTools = (clientWithMcp as any).getToolsForPrompt.bind(clientWithMcp);

      const tools = getTools('navigate and analyze the page');

      // All tools should be browser MCP tools
      for (const tool of tools) {
        expect(tool).toMatch(/^mcp__athena-browser__/);
      }

      // Should NOT include file system tools
      expect(tools).not.toContain('Read');
      expect(tools).not.toContain('Write');
      expect(tools).not.toContain('Bash');
    });
  });

  describe('Tool Usage Tracking Removal', () => {
    it('should not have getToolUsageStats method', () => {
      expect((client as any).getToolUsageStats).toBeUndefined();
    });

    it('should not have resetToolUsageStats method', () => {
      expect((client as any).resetToolUsageStats).toBeUndefined();
    });

    it('should not have toolUsageCount instance variable', () => {
      expect((client as any).toolUsageCount).toBeUndefined();
    });

    it('should clear conversation without tool usage tracking', () => {
      // clearConversation should work without toolUsageCount
      expect(() => client.clearConversation()).not.toThrow();
    });
  });

  describe('Integration: allowedTools + canUseTool', () => {
    it('should use allowedTools for static permissions and canUseTool for dynamic logic', async () => {
      const mcpServer = {
        command: 'test',
        args: [],
        env: {},
        instance: {} as any
      };
      const clientWithMcp = new ClaudeClient(config, mcpServer, sessionManager);

      // Get allowed tools (static permissions)
      const getTools = (clientWithMcp as any).getToolsForPrompt.bind(clientWithMcp);
      const allowedTools = getTools('navigate to a website');

      // Verify allowedTools includes only browser tools
      expect(allowedTools.length).toBeGreaterThan(0);
      expect(allowedTools.every((tool: string) => tool.startsWith('mcp__athena-browser__'))).toBe(true);

      // canUseTool should only handle dynamic logic
      const canUseTool = (clientWithMcp as any).canUseTool.bind(clientWithMcp);

      // Non-JS tools are allowed (static permissions handled by allowedTools)
      await expect(canUseTool('mcp__athena-browser__browser_navigate', {})).resolves.toEqual({
        behavior: 'allow'
      });

      // JS execution requires confirmation (dynamic logic)
      await expect(canUseTool('mcp__athena-browser__browser_execute_js', { code: 'test' })).resolves.toMatchObject({
        behavior: 'ask'
      });
    });
  });

  describe('Backward Compatibility', () => {
    it('should maintain same security restrictions (browser-only tools)', () => {
      const mcpServer = {
        command: 'test',
        args: [],
        env: {},
        instance: {} as any
      };
      const clientWithMcp = new ClaudeClient(config, mcpServer, sessionManager);
      const getTools = (clientWithMcp as any).getToolsForPrompt.bind(clientWithMcp);

      const tools = getTools('test prompt with interaction');

      // Security: should NEVER include file system tools
      expect(tools).not.toContain('Read');
      expect(tools).not.toContain('Write');
      expect(tools).not.toContain('Edit');
      expect(tools).not.toContain('Bash');
      expect(tools).not.toContain('WebFetch');
      expect(tools).not.toContain('WebSearch');
    });

    it('should still require confirmation for JavaScript execution', async () => {
      const canUseTool = (client as any).canUseTool.bind(client);

      const result = await canUseTool('mcp__athena-browser__browser_execute_js', {
        code: 'document.querySelector("#btn").click()'
      });

      expect(result.behavior).toBe('ask');
      expect(result.message).toContain('Allow JavaScript execution');
    });
  });
});
