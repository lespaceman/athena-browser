/**
 * Permission Handler Tests
 */

import { describe, it, expect } from 'vitest';
import { PermissionHandler } from '../../claude/config/permissions';

describe('PermissionHandler', () => {
  describe('canUseTool', () => {
    it('should allow non-JS execution tools', async () => {
      const result = await PermissionHandler.canUseTool(
        'mcp__athena-browser__browser_navigate',
        { url: 'https://example.com' }
      );
      expect(result.behavior).toBe('allow');
      expect(result.message).toBeUndefined();
    });

    it('should allow page summary tool', async () => {
      const result = await PermissionHandler.canUseTool(
        'mcp__athena-browser__browser_get_page_summary',
        {}
      );
      expect(result.behavior).toBe('allow');
    });

    it('should allow screenshot tool', async () => {
      const result = await PermissionHandler.canUseTool(
        'mcp__athena-browser__browser_screenshot',
        {}
      );
      expect(result.behavior).toBe('allow');
    });

    it('should ask for confirmation on JavaScript execution', async () => {
      const result = await PermissionHandler.canUseTool(
        'mcp__athena-browser__browser_execute_js',
        { code: 'console.log("test")' }
      );
      expect(result.behavior).toBe('ask');
      expect(result.message).toBeDefined();
    });

    it('should include code snippet in JS execution message', async () => {
      const code = 'document.querySelector("#submit").click()';
      const result = await PermissionHandler.canUseTool(
        'mcp__athena-browser__browser_execute_js',
        { code }
      );
      expect(result.message).toContain('JavaScript execution');
      expect(result.message).toContain(code);
    });

    it('should truncate long code snippets to 100 chars in message', async () => {
      const longCode = 'a'.repeat(200);
      const result = await PermissionHandler.canUseTool(
        'mcp__athena-browser__browser_execute_js',
        { code: longCode }
      );
      expect(result.message?.length).toBeLessThan(150);
    });

    it('should handle missing code in input gracefully', async () => {
      const result = await PermissionHandler.canUseTool(
        'mcp__athena-browser__browser_execute_js',
        {}
      );
      expect(result.behavior).toBe('ask');
      expect(result.message).toBeDefined();
    });

    it('should allow tab management tools', async () => {
      const result = await PermissionHandler.canUseTool(
        'mcp__athena-browser__window_create_tab',
        { url: 'https://example.com' }
      );
      expect(result.behavior).toBe('allow');
    });

    it('should allow all read-only tools', async () => {
      const readOnlyTools = [
        'mcp__athena-browser__browser_get_url',
        'mcp__athena-browser__browser_get_html',
        'mcp__athena-browser__browser_get_page_summary',
        'mcp__athena-browser__browser_get_interactive_elements',
        'mcp__athena-browser__browser_get_accessibility_tree',
        'mcp__athena-browser__browser_query_content',
        'mcp__athena-browser__browser_screenshot',
        'mcp__athena-browser__browser_get_annotated_screenshot',
        'mcp__athena-browser__window_get_tab_info'
      ];

      for (const tool of readOnlyTools) {
        const result = await PermissionHandler.canUseTool(tool, {});
        expect(result.behavior).toBe('allow');
      }
    });
  });
});
