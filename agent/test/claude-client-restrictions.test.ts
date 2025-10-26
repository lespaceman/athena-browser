/**
 * Unit tests for Claude Client tool restrictions
 *
 * These tests verify that the Claude agent is properly restricted to
 * only use Athena browser MCP tools and cannot access file system or CLI tools.
 */

import { describe, it, expect, beforeEach } from 'vitest';
import { ClaudeClient } from '../src/claude/client.js';
import type { AthenaAgentConfig } from '../src/server/types.js';

describe('ClaudeClient Tool Restrictions', () => {
  let client: ClaudeClient;

  beforeEach(() => {
    const config: AthenaAgentConfig = {
      socketPath: '/tmp/test.sock',
      cwd: process.cwd(),
      model: 'claude-sonnet-4-5',
      permissionMode: 'default',
      logLevel: 'error',
      apiKey: 'test-key',
      maxThinkingTokens: 1000,
      maxTurns: 5
    };

    // Create client without MCP server to test tool selection
    client = new ClaudeClient(config);
  });

  describe('getToolsForPrompt', () => {
    it('should return empty array when no MCP server is configured', () => {
      // Access private method using any cast for testing
      const tools = (client as any).getToolsForPrompt('show me the page');
      expect(tools).toEqual([]);
    });

    it('should only return browser tools for read-only tasks', () => {
      // Create client with mock MCP server
      const mockMcpServer = { name: 'athena-browser', version: '2.0.0' } as any;
      const configWithMcp: AthenaAgentConfig = {
        socketPath: '/tmp/test.sock',
        cwd: process.cwd(),
        model: 'claude-sonnet-4-5',
        permissionMode: 'default',
        logLevel: 'error',
        apiKey: 'test-key',
        maxThinkingTokens: 1000,
        maxTurns: 5
      };

      const clientWithMcp = new ClaudeClient(configWithMcp, mockMcpServer);
      const tools = (clientWithMcp as any).getToolsForPrompt('show me the page');

      // Should only contain browser MCP tools, no file system tools
      expect(tools).not.toContain('Read');
      expect(tools).not.toContain('Write');
      expect(tools).not.toContain('Edit');
      expect(tools).not.toContain('Glob');
      expect(tools).not.toContain('Grep');
      expect(tools).not.toContain('Bash');

      // Should contain read-only browser tools
      expect(tools).toContain('mcp__athena-browser__browser_get_url');
      expect(tools).toContain('mcp__athena-browser__browser_get_page_summary');
      expect(tools).toContain('mcp__athena-browser__browser_screenshot');
    });

    it('should only return browser tools for interactive tasks', () => {
      const mockMcpServer = { name: 'athena-browser', version: '2.0.0' } as any;
      const configWithMcp: AthenaAgentConfig = {
        socketPath: '/tmp/test.sock',
        cwd: process.cwd(),
        model: 'claude-sonnet-4-5',
        permissionMode: 'default',
        logLevel: 'error',
        apiKey: 'test-key',
        maxThinkingTokens: 1000,
        maxTurns: 5
      };

      const clientWithMcp = new ClaudeClient(configWithMcp, mockMcpServer);
      const tools = (clientWithMcp as any).getToolsForPrompt('navigate to google.com and click the search button');

      // Should NOT contain file system tools
      expect(tools).not.toContain('Read');
      expect(tools).not.toContain('Write');
      expect(tools).not.toContain('Edit');
      expect(tools).not.toContain('Bash');

      // Should contain navigation and interaction tools
      expect(tools).toContain('mcp__athena-browser__browser_navigate');
      expect(tools).toContain('mcp__athena-browser__browser_execute_js');
      expect(tools).toContain('mcp__athena-browser__browser_get_interactive_elements');
    });
  });

  describe('canUseTool', () => {
    it('should block Read tool', async () => {
      const result = await (client as any).canUseTool('Read', { file_path: '/etc/passwd' });
      expect(result.behavior).toBe('deny');
      expect(result.message).toContain('not allowed');
    });

    it('should block Write tool', async () => {
      const result = await (client as any).canUseTool('Write', { file_path: '/tmp/test.txt', content: 'test' });
      expect(result.behavior).toBe('deny');
      expect(result.message).toContain('not allowed');
    });

    it('should block Edit tool', async () => {
      const result = await (client as any).canUseTool('Edit', { file_path: '/tmp/test.txt' });
      expect(result.behavior).toBe('deny');
      expect(result.message).toContain('not allowed');
    });

    it('should block Glob tool', async () => {
      const result = await (client as any).canUseTool('Glob', { pattern: '*.js' });
      expect(result.behavior).toBe('deny');
      expect(result.message).toContain('not allowed');
    });

    it('should block Grep tool', async () => {
      const result = await (client as any).canUseTool('Grep', { pattern: 'test' });
      expect(result.behavior).toBe('deny');
      expect(result.message).toContain('not allowed');
    });

    it('should block Bash tool', async () => {
      const result = await (client as any).canUseTool('Bash', { command: 'ls -la' });
      expect(result.behavior).toBe('deny');
      expect(result.message).toContain('not allowed');
    });

    it('should block WebFetch tool', async () => {
      const result = await (client as any).canUseTool('WebFetch', { url: 'https://example.com' });
      expect(result.behavior).toBe('deny');
      expect(result.message).toContain('not allowed');
    });

    it('should block WebSearch tool', async () => {
      const result = await (client as any).canUseTool('WebSearch', { query: 'test' });
      expect(result.behavior).toBe('deny');
      expect(result.message).toContain('not allowed');
    });

    it('should allow browser_get_url', async () => {
      const result = await (client as any).canUseTool('mcp__athena-browser__browser_get_url', {});
      expect(result.behavior).toBe('allow');
    });

    it('should allow browser_navigate', async () => {
      const result = await (client as any).canUseTool('mcp__athena-browser__browser_navigate', { url: 'https://google.com' });
      expect(result.behavior).toBe('allow');
    });

    it('should ask permission for browser_execute_js', async () => {
      const result = await (client as any).canUseTool('mcp__athena-browser__browser_execute_js', { code: 'document.title' });
      expect(result.behavior).toBe('ask');
      expect(result.message).toContain('JavaScript execution');
    });

    it('should allow browser_screenshot', async () => {
      const result = await (client as any).canUseTool('mcp__athena-browser__browser_screenshot', {});
      expect(result.behavior).toBe('allow');
    });

    it('should block unknown tools', async () => {
      const result = await (client as any).canUseTool('UnknownTool', {});
      expect(result.behavior).toBe('deny');
      expect(result.message).toContain('not allowed');
    });
  });

  describe('getSubAgents', () => {
    it('should not include file system tools in web-analyzer', () => {
      const agents = (client as any).getSubAgents();
      const webAnalyzer = agents['web-analyzer'];

      expect(webAnalyzer.tools).not.toContain('Read');
      expect(webAnalyzer.tools).not.toContain('Write');
      expect(webAnalyzer.tools).not.toContain('Edit');
      expect(webAnalyzer.tools).not.toContain('Glob');
      expect(webAnalyzer.tools).not.toContain('Grep');
      expect(webAnalyzer.tools).not.toContain('Bash');
    });

    it('should not include file system tools in navigation-expert', () => {
      const agents = (client as any).getSubAgents();
      const navExpert = agents['navigation-expert'];

      expect(navExpert.tools).not.toContain('Read');
      expect(navExpert.tools).not.toContain('Bash');
    });

    it('should not include file system tools in form-automation', () => {
      const agents = (client as any).getSubAgents();
      const formAuto = agents['form-automation'];

      expect(formAuto.tools).not.toContain('Read');
      expect(formAuto.tools).not.toContain('Write');
      expect(formAuto.tools).not.toContain('Bash');
    });

    it('should not include file system tools in screenshot-analyst', () => {
      const agents = (client as any).getSubAgents();
      const screenshotAnalyst = agents['screenshot-analyst'];

      expect(screenshotAnalyst.tools).not.toContain('Read');
      expect(screenshotAnalyst.tools).not.toContain('Write');
      expect(screenshotAnalyst.tools).not.toContain('Bash');
    });

    it('should only include browser MCP tools in all sub-agents', () => {
      const agents = (client as any).getSubAgents();

      for (const [name, agent] of Object.entries(agents)) {
        const agentTools = (agent as any).tools;

        // All tools should start with mcp__athena-browser__
        for (const tool of agentTools) {
          expect(tool).toMatch(/^mcp__athena-browser__/);
        }
      }
    });
  });
});
