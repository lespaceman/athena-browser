/**
 * Sub-Agent Configuration Tests
 */

import { describe, it, expect } from 'vitest';
import { SubAgentConfig } from '../../claude-config/sub-agents';

describe('SubAgentConfig', () => {
  describe('getAgents', () => {
    it('should return all sub-agents', () => {
      const agents = SubAgentConfig.getAgents();
      expect(agents).toBeDefined();
      expect(typeof agents).toBe('object');
    });

    it('should return 4 sub-agents', () => {
      const agents = SubAgentConfig.getAgents();
      const agentNames = Object.keys(agents);
      expect(agentNames.length).toBe(4);
    });

    it('should include web-analyzer agent', () => {
      const agents = SubAgentConfig.getAgents();
      expect(agents['web-analyzer']).toBeDefined();
    });

    it('should include navigation-expert agent', () => {
      const agents = SubAgentConfig.getAgents();
      expect(agents['navigation-expert']).toBeDefined();
    });

    it('should include form-automation agent', () => {
      const agents = SubAgentConfig.getAgents();
      expect(agents['form-automation']).toBeDefined();
    });

    it('should include screenshot-analyst agent', () => {
      const agents = SubAgentConfig.getAgents();
      expect(agents['screenshot-analyst']).toBeDefined();
    });
  });

  describe('web-analyzer agent', () => {
    it('should have description', () => {
      const agent = SubAgentConfig.getAgent('web-analyzer');
      expect(agent?.description).toBeDefined();
      expect(agent?.description.length).toBeGreaterThan(0);
    });

    it('should have prompt', () => {
      const agent = SubAgentConfig.getAgent('web-analyzer');
      expect(agent?.prompt).toBeDefined();
      expect(agent?.prompt).toContain('web page analysis');
    });

    it('should have tools array', () => {
      const agent = SubAgentConfig.getAgent('web-analyzer');
      expect(Array.isArray(agent?.tools)).toBe(true);
      expect(agent?.tools.length).toBeGreaterThan(0);
    });

    it('should use sonnet model', () => {
      const agent = SubAgentConfig.getAgent('web-analyzer');
      expect(agent?.model).toBe('sonnet');
    });

    it('should have appropriate tools for analysis', () => {
      const agent = SubAgentConfig.getAgent('web-analyzer');
      expect(agent?.tools).toContain('mcp__athena-browser__browser_get_page_summary');
      expect(agent?.tools).toContain('mcp__athena-browser__browser_get_html');
      expect(agent?.tools).toContain('mcp__athena-browser__browser_get_accessibility_tree');
      expect(agent?.tools).toContain('mcp__athena-browser__browser_query_content');
    });

    it('should not have navigation tools', () => {
      const agent = SubAgentConfig.getAgent('web-analyzer');
      expect(agent?.tools).not.toContain('mcp__athena-browser__browser_navigate');
      expect(agent?.tools).not.toContain('mcp__athena-browser__browser_back');
    });
  });

  describe('navigation-expert agent', () => {
    it('should have description', () => {
      const agent = SubAgentConfig.getAgent('navigation-expert');
      expect(agent?.description).toBeDefined();
      expect(agent?.description).toContain('navigation');
    });

    it('should use haiku model for efficiency', () => {
      const agent = SubAgentConfig.getAgent('navigation-expert');
      expect(agent?.model).toBe('haiku');
    });

    it('should have navigation tools', () => {
      const agent = SubAgentConfig.getAgent('navigation-expert');
      expect(agent?.tools).toContain('mcp__athena-browser__browser_navigate');
      expect(agent?.tools).toContain('mcp__athena-browser__browser_back');
      expect(agent?.tools).toContain('mcp__athena-browser__browser_forward');
      expect(agent?.tools).toContain('mcp__athena-browser__browser_reload');
    });

    it('should have tab management tools', () => {
      const agent = SubAgentConfig.getAgent('navigation-expert');
      expect(agent?.tools).toContain('mcp__athena-browser__window_create_tab');
      expect(agent?.tools).toContain('mcp__athena-browser__window_close_tab');
      expect(agent?.tools).toContain('mcp__athena-browser__window_switch_tab');
    });
  });

  describe('form-automation agent', () => {
    it('should have description about forms', () => {
      const agent = SubAgentConfig.getAgent('form-automation');
      expect(agent?.description).toContain('form');
    });

    it('should use sonnet model', () => {
      const agent = SubAgentConfig.getAgent('form-automation');
      expect(agent?.model).toBe('sonnet');
    });

    it('should have JavaScript execution tool', () => {
      const agent = SubAgentConfig.getAgent('form-automation');
      expect(agent?.tools).toContain('mcp__athena-browser__browser_execute_js');
    });

    it('should have query_content for form analysis', () => {
      const agent = SubAgentConfig.getAgent('form-automation');
      expect(agent?.tools).toContain('mcp__athena-browser__browser_query_content');
    });

    it('should have screenshot tool for verification', () => {
      const agent = SubAgentConfig.getAgent('form-automation');
      expect(agent?.tools).toContain('mcp__athena-browser__browser_screenshot');
    });
  });

  describe('screenshot-analyst agent', () => {
    it('should have description about visual analysis', () => {
      const agent = SubAgentConfig.getAgent('screenshot-analyst');
      expect(agent?.description).toContain('visual');
    });

    it('should use sonnet model', () => {
      const agent = SubAgentConfig.getAgent('screenshot-analyst');
      expect(agent?.model).toBe('sonnet');
    });

    it('should have screenshot tools', () => {
      const agent = SubAgentConfig.getAgent('screenshot-analyst');
      expect(agent?.tools).toContain('mcp__athena-browser__browser_screenshot');
      expect(agent?.tools).toContain('mcp__athena-browser__browser_get_annotated_screenshot');
    });

    it('should have accessibility tree for context', () => {
      const agent = SubAgentConfig.getAgent('screenshot-analyst');
      expect(agent?.tools).toContain('mcp__athena-browser__browser_get_accessibility_tree');
    });
  });

  describe('getAgentTools', () => {
    it('should return tools for a valid agent', () => {
      const tools = SubAgentConfig.getAgentTools('web-analyzer');
      expect(Array.isArray(tools)).toBe(true);
      expect(tools.length).toBeGreaterThan(0);
    });

    it('should return empty array for invalid agent', () => {
      const tools = SubAgentConfig.getAgentTools('non-existent-agent');
      expect(Array.isArray(tools)).toBe(true);
      expect(tools.length).toBe(0);
    });
  });

  describe('getAgentNames', () => {
    it('should return all agent names', () => {
      const names = SubAgentConfig.getAgentNames();
      expect(Array.isArray(names)).toBe(true);
      expect(names.length).toBe(4);
    });

    it('should include all expected agent names', () => {
      const names = SubAgentConfig.getAgentNames();
      expect(names).toContain('web-analyzer');
      expect(names).toContain('navigation-expert');
      expect(names).toContain('form-automation');
      expect(names).toContain('screenshot-analyst');
    });
  });

  describe('getAgent', () => {
    it('should return agent configuration for valid agent', () => {
      const agent = SubAgentConfig.getAgent('web-analyzer');
      expect(agent).not.toBeNull();
      expect(agent?.description).toBeDefined();
      expect(agent?.prompt).toBeDefined();
      expect(agent?.tools).toBeDefined();
      expect(agent?.model).toBeDefined();
    });

    it('should return null for invalid agent', () => {
      const agent = SubAgentConfig.getAgent('non-existent-agent');
      expect(agent).toBeNull();
    });
  });
});
