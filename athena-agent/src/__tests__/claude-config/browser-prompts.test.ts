/**
 * Browser Prompts Tests
 */

import { describe, it, expect } from 'vitest';
import { BrowserPrompts } from '../../claude-config/browser-prompts';

describe('BrowserPrompts', () => {
  describe('getSystemPrompt', () => {
    it('should return a non-empty system prompt', () => {
      const prompt = BrowserPrompts.getSystemPrompt();
      expect(prompt).toBeDefined();
      expect(prompt.length).toBeGreaterThan(0);
    });

    it('should include IMPORTANT RESTRICTION section', () => {
      const prompt = BrowserPrompts.getSystemPrompt();
      expect(prompt).toContain('**IMPORTANT RESTRICTION:**');
    });

    it('should mention browser MCP tools only', () => {
      const prompt = BrowserPrompts.getSystemPrompt();
      expect(prompt).toContain('Athena browser MCP tools');
      expect(prompt).toContain('You do NOT have access to');
    });

    it('should list available browser tools', () => {
      const prompt = BrowserPrompts.getSystemPrompt();
      expect(prompt).toContain('browser_navigate');
      expect(prompt).toContain('browser_get_url');
      expect(prompt).toContain('browser_get_html');
      expect(prompt).toContain('browser_execute_js');
      expect(prompt).toContain('browser_screenshot');
      expect(prompt).toContain('window_create_tab');
    });

    it('should include best practices section', () => {
      const prompt = BrowserPrompts.getSystemPrompt();
      expect(prompt).toContain('**Best Practices:**');
      expect(prompt).toContain('browser_get_page_summary before browser_get_html');
    });

    it('should include tool usage efficiency guidelines', () => {
      const prompt = BrowserPrompts.getSystemPrompt();
      expect(prompt).toContain('**Tool Usage Efficiency:**');
    });
  });

  describe('getBrowserTools', () => {
    it('should return an array of browser tools', () => {
      const tools = BrowserPrompts.getBrowserTools();
      expect(Array.isArray(tools)).toBe(true);
      expect(tools.length).toBeGreaterThan(0);
    });

    it('should include all core navigation tools', () => {
      const tools = BrowserPrompts.getBrowserTools();
      expect(tools).toContain('browser_navigate');
      expect(tools).toContain('browser_back');
      expect(tools).toContain('browser_forward');
      expect(tools).toContain('browser_reload');
    });

    it('should include all information gathering tools', () => {
      const tools = BrowserPrompts.getBrowserTools();
      expect(tools).toContain('browser_get_url');
      expect(tools).toContain('browser_get_html');
      expect(tools).toContain('browser_get_page_summary');
      expect(tools).toContain('browser_get_interactive_elements');
      expect(tools).toContain('browser_get_accessibility_tree');
      expect(tools).toContain('browser_query_content');
    });

    it('should include interaction tools', () => {
      const tools = BrowserPrompts.getBrowserTools();
      expect(tools).toContain('browser_execute_js');
    });

    it('should include screenshot tools', () => {
      const tools = BrowserPrompts.getBrowserTools();
      expect(tools).toContain('browser_screenshot');
      expect(tools).toContain('browser_get_annotated_screenshot');
    });

    it('should include tab management tools', () => {
      const tools = BrowserPrompts.getBrowserTools();
      expect(tools).toContain('window_create_tab');
      expect(tools).toContain('window_close_tab');
      expect(tools).toContain('window_switch_tab');
      expect(tools).toContain('window_get_tab_info');
    });

    it('should return exactly 17 tools', () => {
      const tools = BrowserPrompts.getBrowserTools();
      expect(tools.length).toBe(17);
    });
  });

  describe('getBestPractices', () => {
    it('should return an array of best practices', () => {
      const practices = BrowserPrompts.getBestPractices();
      expect(Array.isArray(practices)).toBe(true);
      expect(practices.length).toBeGreaterThan(0);
    });

    it('should include page_summary usage guideline', () => {
      const practices = BrowserPrompts.getBestPractices();
      const found = practices.some(p => p.includes('browser_get_page_summary'));
      expect(found).toBe(true);
    });

    it('should include interactive_elements guideline', () => {
      const practices = BrowserPrompts.getBestPractices();
      const found = practices.some(p => p.includes('browser_get_interactive_elements'));
      expect(found).toBe(true);
    });

    it('should include query_content guideline', () => {
      const practices = BrowserPrompts.getBestPractices();
      const found = practices.some(p => p.includes('browser_query_content'));
      expect(found).toBe(true);
    });

    it('should include navigation verification guideline', () => {
      const practices = BrowserPrompts.getBestPractices();
      const found = practices.some(p => p.includes('browser_get_url'));
      expect(found).toBe(true);
    });

    it('should return exactly 6 best practices', () => {
      const practices = BrowserPrompts.getBestPractices();
      expect(practices.length).toBe(6);
    });
  });
});
