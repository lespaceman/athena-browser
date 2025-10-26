/**
 * Tool Selector Tests
 */

import { describe, it, expect } from 'vitest';
import { ToolSelector } from '../../claude/config/tool-selector';

// Mock MCP server instance (minimal structure needed for tests)
const mockMcpServer: any = {
  name: 'athena-browser',
  instance: {}
};

describe('ToolSelector', () => {
  describe('selectTools', () => {
    it('should return empty array when no MCP server provided', () => {
      const tools = ToolSelector.selectTools('show me the page', null);
      expect(tools).toEqual([]);
    });

    it('should return read-only tools for "show" prompt', () => {
      const tools = ToolSelector.selectTools('show me the page', mockMcpServer);
      expect(tools.length).toBeGreaterThan(0);
      expect(tools).toContain('mcp__athena-browser__browser_get_page_summary');
      expect(tools).toContain('mcp__athena-browser__browser_get_html');
      expect(tools).not.toContain('mcp__athena-browser__browser_execute_js');
    });

    it('should return read-only tools for "get" prompt', () => {
      const tools = ToolSelector.selectTools('get the page title', mockMcpServer);
      expect(tools.length).toBeGreaterThan(0);
      expect(tools).toContain('mcp__athena-browser__browser_get_page_summary');
      expect(tools).not.toContain('mcp__athena-browser__browser_navigate');
    });

    it('should return read-only tools for "find" prompt', () => {
      const tools = ToolSelector.selectTools('find all links on the page', mockMcpServer);
      expect(tools).toContain('mcp__athena-browser__browser_get_interactive_elements');
      expect(tools).not.toContain('mcp__athena-browser__browser_execute_js');
    });

    it('should return read-only tools for "search" prompt', () => {
      const tools = ToolSelector.selectTools('search for form elements', mockMcpServer);
      expect(tools).toContain('mcp__athena-browser__browser_query_content');
      expect(tools).not.toContain('mcp__athena-browser__browser_execute_js');
    });

    it('should return read-only tools for "analyze" prompt', () => {
      const tools = ToolSelector.selectTools('analyze the page structure', mockMcpServer);
      expect(tools).toContain('mcp__athena-browser__browser_get_accessibility_tree');
      expect(tools).not.toContain('mcp__athena-browser__browser_navigate');
    });

    it('should return read-only tools for "extract" prompt', () => {
      const tools = ToolSelector.selectTools('extract data from the page', mockMcpServer);
      expect(tools).toContain('mcp__athena-browser__browser_query_content');
      expect(tools).not.toContain('mcp__athena-browser__browser_execute_js');
    });

    it('should return interactive tools for "click" prompt', () => {
      const tools = ToolSelector.selectTools('click the submit button', mockMcpServer);
      expect(tools).toContain('mcp__athena-browser__browser_execute_js');
      expect(tools).toContain('mcp__athena-browser__browser_navigate');
    });

    it('should return interactive tools for "fill" prompt', () => {
      const tools = ToolSelector.selectTools('fill out the form', mockMcpServer);
      expect(tools).toContain('mcp__athena-browser__browser_execute_js');
    });

    it('should return interactive tools for "submit" prompt', () => {
      const tools = ToolSelector.selectTools('submit the form', mockMcpServer);
      expect(tools).toContain('mcp__athena-browser__browser_execute_js');
    });

    it('should return interactive tools for "execute" prompt', () => {
      const tools = ToolSelector.selectTools('execute some JavaScript', mockMcpServer);
      expect(tools).toContain('mcp__athena-browser__browser_execute_js');
    });

    it('should return interactive tools for "navigate" prompt', () => {
      const tools = ToolSelector.selectTools('navigate to google.com', mockMcpServer);
      expect(tools).toContain('mcp__athena-browser__browser_navigate');
      expect(tools).toContain('mcp__athena-browser__browser_execute_js');
    });

    it('should return interactive tools for "type" prompt', () => {
      const tools = ToolSelector.selectTools('type in the search box', mockMcpServer);
      expect(tools).toContain('mcp__athena-browser__browser_execute_js');
    });

    it('should prioritize interaction over read-only when both keywords present', () => {
      const tools = ToolSelector.selectTools('show me the page and click the button', mockMcpServer);
      expect(tools).toContain('mcp__athena-browser__browser_execute_js');
      expect(tools).toContain('mcp__athena-browser__browser_navigate');
    });

    it('should be case insensitive', () => {
      const tools1 = ToolSelector.selectTools('SHOW me the page', mockMcpServer);
      const tools2 = ToolSelector.selectTools('show me the page', mockMcpServer);
      expect(tools1).toEqual(tools2);
    });

    it('should include tab info tool in read-only mode', () => {
      const tools = ToolSelector.selectTools('get tab information', mockMcpServer);
      expect(tools).toContain('mcp__athena-browser__window_get_tab_info');
    });

    it('should include all tab management tools in interactive mode', () => {
      const tools = ToolSelector.selectTools('navigate to a new tab', mockMcpServer);
      expect(tools).toContain('mcp__athena-browser__window_create_tab');
      expect(tools).toContain('mcp__athena-browser__window_close_tab');
      expect(tools).toContain('mcp__athena-browser__window_switch_tab');
    });
  });

  describe('getAllBrowserTools', () => {
    it('should return all available browser tools', () => {
      const tools = ToolSelector.getAllBrowserTools();
      expect(Array.isArray(tools)).toBe(true);
      expect(tools.length).toBe(17);
    });

    it('should include navigation tools', () => {
      const tools = ToolSelector.getAllBrowserTools();
      expect(tools).toContain('mcp__athena-browser__browser_navigate');
      expect(tools).toContain('mcp__athena-browser__browser_back');
      expect(tools).toContain('mcp__athena-browser__browser_forward');
      expect(tools).toContain('mcp__athena-browser__browser_reload');
    });

    it('should include information gathering tools', () => {
      const tools = ToolSelector.getAllBrowserTools();
      expect(tools).toContain('mcp__athena-browser__browser_get_url');
      expect(tools).toContain('mcp__athena-browser__browser_get_html');
      expect(tools).toContain('mcp__athena-browser__browser_get_page_summary');
      expect(tools).toContain('mcp__athena-browser__browser_get_interactive_elements');
      expect(tools).toContain('mcp__athena-browser__browser_get_accessibility_tree');
      expect(tools).toContain('mcp__athena-browser__browser_query_content');
    });

    it('should include interaction tools', () => {
      const tools = ToolSelector.getAllBrowserTools();
      expect(tools).toContain('mcp__athena-browser__browser_execute_js');
    });

    it('should include screenshot tools', () => {
      const tools = ToolSelector.getAllBrowserTools();
      expect(tools).toContain('mcp__athena-browser__browser_screenshot');
      expect(tools).toContain('mcp__athena-browser__browser_get_annotated_screenshot');
    });

    it('should include tab management tools', () => {
      const tools = ToolSelector.getAllBrowserTools();
      expect(tools).toContain('mcp__athena-browser__window_create_tab');
      expect(tools).toContain('mcp__athena-browser__window_close_tab');
      expect(tools).toContain('mcp__athena-browser__window_switch_tab');
      expect(tools).toContain('mcp__athena-browser__window_get_tab_info');
    });
  });
});
