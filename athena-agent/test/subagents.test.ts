/**
 * Subagent Configuration Tests
 *
 * Tests for specialized browser automation subagents:
 * - Configuration and availability
 * - Tool assignments and restrictions
 * - Model selection and optimization
 * - Integration with session management
 * - Browser-only tool enforcement
 */

import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import { ClaudeClient } from '../src/claude-client.js';
import { SessionManager } from '../src/session-manager.js';
import type { AthenaAgentConfig } from '../src/types.js';
import { existsSync, rmSync } from 'fs';

describe('Subagent Configuration', () => {
  const testSessionDir = '.test-sessions-subagents';
  let config: AthenaAgentConfig;
  let sessionManager: SessionManager;
  let mockMcpServer: any;

  beforeEach(() => {
    if (existsSync(testSessionDir)) {
      rmSync(testSessionDir, { recursive: true, force: true });
    }
    sessionManager = new SessionManager(testSessionDir);

    config = {
      model: 'sonnet',
      cwd: process.cwd(),
      permissionMode: 'default',
      maxThinkingTokens: 1000,
      maxTurns: 10
    };

    mockMcpServer = {
      command: 'test-mcp-server',
      args: ['--test'],
      instance: {}
    };
  });

  afterEach(() => {
    if (existsSync(testSessionDir)) {
      rmSync(testSessionDir, { recursive: true, force: true });
    }
  });

  describe('Availability', () => {
    it('should provide four specialized subagents when MCP server is configured', () => {
      const client = new ClaudeClient(config, mockMcpServer, sessionManager);
      const getSubAgents = (client as any).getSubAgents.bind(client);
      const subAgents = getSubAgents();

      expect(subAgents).toBeDefined();
      expect(Object.keys(subAgents)).toHaveLength(4);
      expect(subAgents).toHaveProperty('web-analyzer');
      expect(subAgents).toHaveProperty('navigation-expert');
      expect(subAgents).toHaveProperty('form-automation');
      expect(subAgents).toHaveProperty('screenshot-analyst');
    });

    it('should return subagent definitions even without MCP server', () => {
      const client = new ClaudeClient(config, undefined, sessionManager);
      const getSubAgents = (client as any).getSubAgents.bind(client);
      const subAgents = getSubAgents();

      // Subagents are defined but won't be passed to query() without MCP server
      expect(subAgents).toBeDefined();
    });
  });

  describe('web-analyzer Configuration', () => {
    it('should be configured for page analysis and data extraction', () => {
      const client = new ClaudeClient(config, mockMcpServer, sessionManager);
      const getSubAgents = (client as any).getSubAgents.bind(client);
      const subAgents = getSubAgents();

      const webAnalyzer = subAgents['web-analyzer'];

      expect(webAnalyzer).toBeDefined();
      expect(webAnalyzer.description).toContain('analyzing web page structure');
      expect(webAnalyzer.prompt).toContain('web page analysis and data extraction');
      expect(webAnalyzer.model).toBe('sonnet');
    });

    it('should have read-only browser tools', () => {
      const client = new ClaudeClient(config, mockMcpServer, sessionManager);
      const getSubAgents = (client as any).getSubAgents.bind(client);
      const subAgents = getSubAgents();

      const tools = subAgents['web-analyzer'].tools;

      expect(tools).toContain('mcp__athena-browser__browser_get_url');
      expect(tools).toContain('mcp__athena-browser__browser_get_html');
      expect(tools).toContain('mcp__athena-browser__browser_get_page_summary');
      expect(tools).toContain('mcp__athena-browser__browser_get_interactive_elements');
      expect(tools).toContain('mcp__athena-browser__browser_get_accessibility_tree');
      expect(tools).toContain('mcp__athena-browser__browser_query_content');
      expect(tools).toContain('mcp__athena-browser__browser_screenshot');
    });

    it('should not have navigation or interaction tools', () => {
      const client = new ClaudeClient(config, mockMcpServer, sessionManager);
      const getSubAgents = (client as any).getSubAgents.bind(client);
      const subAgents = getSubAgents();

      const tools = subAgents['web-analyzer'].tools;

      expect(tools).not.toContain('mcp__athena-browser__browser_navigate');
      expect(tools).not.toContain('mcp__athena-browser__browser_execute_js');
    });
  });

  describe('navigation-expert Configuration', () => {
    it('should be configured for browser navigation and tab management', () => {
      const client = new ClaudeClient(config, mockMcpServer, sessionManager);
      const getSubAgents = (client as any).getSubAgents.bind(client);
      const subAgents = getSubAgents();

      const navigationExpert = subAgents['navigation-expert'];

      expect(navigationExpert).toBeDefined();
      expect(navigationExpert.description).toContain('browser navigation');
      expect(navigationExpert.prompt).toContain('navigation and workflow automation');
      expect(navigationExpert.model).toBe('haiku');
    });

    it('should have navigation and tab management tools', () => {
      const client = new ClaudeClient(config, mockMcpServer, sessionManager);
      const getSubAgents = (client as any).getSubAgents.bind(client);
      const subAgents = getSubAgents();

      const tools = subAgents['navigation-expert'].tools;

      expect(tools).toContain('mcp__athena-browser__browser_navigate');
      expect(tools).toContain('mcp__athena-browser__browser_back');
      expect(tools).toContain('mcp__athena-browser__browser_forward');
      expect(tools).toContain('mcp__athena-browser__browser_reload');
      expect(tools).toContain('mcp__athena-browser__window_create_tab');
      expect(tools).toContain('mcp__athena-browser__window_close_tab');
      expect(tools).toContain('mcp__athena-browser__window_switch_tab');
      expect(tools).toContain('mcp__athena-browser__window_get_tab_info');
    });

    it('should not have JavaScript execution capability', () => {
      const client = new ClaudeClient(config, mockMcpServer, sessionManager);
      const getSubAgents = (client as any).getSubAgents.bind(client);
      const subAgents = getSubAgents();

      const tools = subAgents['navigation-expert'].tools;

      expect(tools).not.toContain('mcp__athena-browser__browser_execute_js');
    });

    it('should use haiku model for efficient navigation', () => {
      const client = new ClaudeClient(config, mockMcpServer, sessionManager);
      const getSubAgents = (client as any).getSubAgents.bind(client);
      const subAgents = getSubAgents();

      expect(subAgents['navigation-expert'].model).toBe('haiku');
    });
  });

  describe('form-automation Configuration', () => {
    it('should be configured for form interactions and input handling', () => {
      const client = new ClaudeClient(config, mockMcpServer, sessionManager);
      const getSubAgents = (client as any).getSubAgents.bind(client);
      const subAgents = getSubAgents();

      const formAutomation = subAgents['form-automation'];

      expect(formAutomation).toBeDefined();
      expect(formAutomation.description).toContain('web form interactions');
      expect(formAutomation.prompt).toContain('form automation and interaction');
      expect(formAutomation.model).toBe('sonnet');
    });

    it('should have form analysis and JavaScript execution tools', () => {
      const client = new ClaudeClient(config, mockMcpServer, sessionManager);
      const getSubAgents = (client as any).getSubAgents.bind(client);
      const subAgents = getSubAgents();

      const tools = subAgents['form-automation'].tools;

      expect(tools).toContain('mcp__athena-browser__browser_get_page_summary');
      expect(tools).toContain('mcp__athena-browser__browser_query_content');
      expect(tools).toContain('mcp__athena-browser__browser_get_interactive_elements');
      expect(tools).toContain('mcp__athena-browser__browser_execute_js');
      expect(tools).toContain('mcp__athena-browser__browser_screenshot');
    });

    it('should not have navigation tools', () => {
      const client = new ClaudeClient(config, mockMcpServer, sessionManager);
      const getSubAgents = (client as any).getSubAgents.bind(client);
      const subAgents = getSubAgents();

      const tools = subAgents['form-automation'].tools;

      expect(tools).not.toContain('mcp__athena-browser__browser_navigate');
    });
  });

  describe('screenshot-analyst Configuration', () => {
    it('should be configured for visual analysis and UI understanding', () => {
      const client = new ClaudeClient(config, mockMcpServer, sessionManager);
      const getSubAgents = (client as any).getSubAgents.bind(client);
      const subAgents = getSubAgents();

      const screenshotAnalyst = subAgents['screenshot-analyst'];

      expect(screenshotAnalyst).toBeDefined();
      expect(screenshotAnalyst.description).toContain('analyzing visual content');
      expect(screenshotAnalyst.prompt).toContain('visual analysis');
      expect(screenshotAnalyst.model).toBe('sonnet');
    });

    it('should have screenshot and visual analysis tools', () => {
      const client = new ClaudeClient(config, mockMcpServer, sessionManager);
      const getSubAgents = (client as any).getSubAgents.bind(client);
      const subAgents = getSubAgents();

      const tools = subAgents['screenshot-analyst'].tools;

      expect(tools).toContain('mcp__athena-browser__browser_screenshot');
      expect(tools).toContain('mcp__athena-browser__browser_get_annotated_screenshot');
      expect(tools).toContain('mcp__athena-browser__browser_get_interactive_elements');
      expect(tools).toContain('mcp__athena-browser__browser_get_accessibility_tree');
    });

    it('should not have navigation or execution capabilities', () => {
      const client = new ClaudeClient(config, mockMcpServer, sessionManager);
      const getSubAgents = (client as any).getSubAgents.bind(client);
      const subAgents = getSubAgents();

      const tools = subAgents['screenshot-analyst'].tools;

      expect(tools).not.toContain('mcp__athena-browser__browser_navigate');
      expect(tools).not.toContain('mcp__athena-browser__browser_execute_js');
    });
  });

  describe('Security: Tool Restrictions', () => {
    it('should ensure no subagent has file system access', () => {
      const client = new ClaudeClient(config, mockMcpServer, sessionManager);
      const getSubAgents = (client as any).getSubAgents.bind(client);
      const subAgents = getSubAgents();

      const fileSystemTools = ['Read', 'Write', 'Edit', 'Glob', 'Grep', 'Bash', 'WebFetch', 'WebSearch'];

      for (const [agentName, agentConfig] of Object.entries(subAgents)) {
        const tools = (agentConfig as any).tools;
        for (const fsTool of fileSystemTools) {
          expect(tools).not.toContain(fsTool);
        }
      }
    });

    it('should ensure all subagent tools are browser MCP tools', () => {
      const client = new ClaudeClient(config, mockMcpServer, sessionManager);
      const getSubAgents = (client as any).getSubAgents.bind(client);
      const subAgents = getSubAgents();

      for (const [agentName, agentConfig] of Object.entries(subAgents)) {
        const tools = (agentConfig as any).tools;
        for (const tool of tools) {
          expect(tool).toMatch(/^mcp__athena-browser__/);
        }
      }
    });
  });

  describe('Model Selection', () => {
    it('should use appropriate models for performance optimization', () => {
      const client = new ClaudeClient(config, mockMcpServer, sessionManager);
      const getSubAgents = (client as any).getSubAgents.bind(client);
      const subAgents = getSubAgents();

      // Navigation uses fast haiku model
      expect(subAgents['navigation-expert'].model).toBe('haiku');

      // Complex tasks use sonnet model
      expect(subAgents['web-analyzer'].model).toBe('sonnet');
      expect(subAgents['form-automation'].model).toBe('sonnet');
      expect(subAgents['screenshot-analyst'].model).toBe('sonnet');
    });
  });

  describe('SDK Integration', () => {
    it('should have correct structure for Claude Agent SDK', () => {
      const client = new ClaudeClient(config, mockMcpServer, sessionManager);
      const getSubAgents = (client as any).getSubAgents.bind(client);
      const subAgents = getSubAgents();

      for (const [agentName, agentConfig] of Object.entries(subAgents)) {
        const agent = agentConfig as any;

        // Verify required SDK fields
        expect(agent).toHaveProperty('description');
        expect(agent).toHaveProperty('prompt');
        expect(agent).toHaveProperty('tools');
        expect(agent).toHaveProperty('model');

        // Verify field types
        expect(typeof agent.description).toBe('string');
        expect(typeof agent.prompt).toBe('string');
        expect(Array.isArray(agent.tools)).toBe(true);
        expect(typeof agent.model).toBe('string');

        // Verify non-empty values
        expect(agent.description.length).toBeGreaterThan(0);
        expect(agent.prompt.length).toBeGreaterThan(0);
        expect(agent.tools.length).toBeGreaterThan(0);
      }
    });
  });
});

describe('Session Integration with Subagents', () => {
  const testSessionDir = '.test-sessions-subagents-integration';
  let config: AthenaAgentConfig;
  let sessionManager: SessionManager;
  let mockMcpServer: any;

  beforeEach(() => {
    if (existsSync(testSessionDir)) {
      rmSync(testSessionDir, { recursive: true, force: true });
    }
    sessionManager = new SessionManager(testSessionDir);

    config = {
      model: 'sonnet',
      cwd: process.cwd(),
      permissionMode: 'default',
      maxThinkingTokens: 1000,
      maxTurns: 10
    };

    mockMcpServer = {
      command: 'test-mcp-server',
      args: ['--test'],
      instance: {}
    };
  });

  afterEach(() => {
    if (existsSync(testSessionDir)) {
      rmSync(testSessionDir, { recursive: true, force: true });
    }
  });

  it('should maintain subagent configuration when resuming sessions', async () => {
    const client = new ClaudeClient(config, mockMcpServer, sessionManager);

    const session = await sessionManager.createSession({
      title: 'Test Session with Subagents',
      createdAt: Date.now(),
      lastUsedAt: Date.now(),
      messageCount: 0,
      totalTokens: { input: 0, output: 0 },
      totalCost: 0,
      model: 'sonnet',
      tags: []
    });

    await sessionManager.updateMetadata(session.metadata.sessionId, {
      claudeSessionId: 'claude_session_test_123'
    });

    await client.resumeSession(session.metadata.sessionId);

    expect(client.getSessionId()).toBe(session.metadata.sessionId);

    const getSubAgents = (client as any).getSubAgents.bind(client);
    const subAgents = getSubAgents();

    expect(subAgents).toBeDefined();
    expect(Object.keys(subAgents)).toHaveLength(4);
  });

  it('should include subagents in all query types', () => {
    const client = new ClaudeClient(config, mockMcpServer, sessionManager);
    const getSubAgents = (client as any).getSubAgents.bind(client);
    const subAgents = getSubAgents();

    // Verify subagents are available for all operation types
    expect(subAgents).toBeDefined();
    expect(Object.keys(subAgents)).toEqual([
      'web-analyzer',
      'navigation-expert',
      'form-automation',
      'screenshot-analyst'
    ]);
  });

  it('should work without MCP server configured', () => {
    const client = new ClaudeClient(config, undefined, sessionManager);

    expect(() => {
      const getSubAgents = (client as any).getSubAgents.bind(client);
      getSubAgents();
    }).not.toThrow();
  });
});

describe('Backward Compatibility', () => {
  const testSessionDir = '.test-sessions-subagents-compat';
  let config: AthenaAgentConfig;
  let sessionManager: SessionManager;

  beforeEach(() => {
    if (existsSync(testSessionDir)) {
      rmSync(testSessionDir, { recursive: true, force: true });
    }
    sessionManager = new SessionManager(testSessionDir);

    config = {
      model: 'sonnet',
      cwd: process.cwd(),
      permissionMode: 'default',
      maxThinkingTokens: 1000,
      maxTurns: 10
    };
  });

  afterEach(() => {
    if (existsSync(testSessionDir)) {
      rmSync(testSessionDir, { recursive: true, force: true });
    }
  });

  it('should maintain session storage behavior with subagents', async () => {
    const mockMcpServer = {
      command: 'test-mcp-server',
      args: ['--test'],
      instance: {}
    };

    const client = new ClaudeClient(config, mockMcpServer, sessionManager);

    const session = await sessionManager.createSession({
      title: 'Compatibility Test',
      createdAt: Date.now(),
      lastUsedAt: Date.now(),
      messageCount: 0,
      totalTokens: { input: 0, output: 0 },
      totalCost: 0,
      model: 'sonnet',
      tags: []
    });

    // Session should not have messages array
    expect((session as any).messages).toBeUndefined();
  });

  it('should maintain tool permission behavior with subagents', () => {
    const mockMcpServer = {
      command: 'test-mcp-server',
      args: ['--test'],
      instance: {}
    };

    const client = new ClaudeClient(config, mockMcpServer, sessionManager);
    const getToolsForPrompt = (client as any).getToolsForPrompt.bind(client);
    const tools = getToolsForPrompt('show me the page');

    expect(Array.isArray(tools)).toBe(true);
    expect(tools.length).toBeGreaterThan(0);
    expect(tools.every((t: string) => t.startsWith('mcp__athena-browser__'))).toBe(true);
  });
});
