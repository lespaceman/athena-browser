/**
 * MCP Agent Adapter
 *
 * Single Responsibility: Convert our MCP server to Claude Agent SDK format
 *
 * This adapter wraps our existing MCP server tools using the Claude Agent SDK's
 * createSdkMcpServer() function, allowing them to be used with query().
 *
 * Design: Thin adapter layer - delegates all business logic to BrowserApiClient
 */

import { createSdkMcpServer, tool } from '@anthropic-ai/claude-agent-sdk';
import { z } from 'zod';
import { BrowserApiClient } from './browser-api-client.js';
import { Logger } from './logger.js';

const logger = new Logger('MCPAgentAdapter');

/**
 * Create a Claude Agent SDK compatible MCP server for browser control
 *
 * @param socketPath - Path to the athena-agent Unix socket
 * @returns MCP server instance compatible with Claude Agent SDK
 */
export function createAgentMcpServer(socketPath: string) {
  logger.info('Creating Agent SDK MCP server', { socketPath });

  const client = new BrowserApiClient({ socketPath });

  return createSdkMcpServer({
    name: 'athena-browser',
    version: '2.0.0',
    tools: [
      // Navigation Tools
      tool(
        'browser_navigate',
        'Navigate the browser to a specific URL',
        {
          url: z.string().url().describe('The URL to navigate to'),
          tabIndex: z.number().optional().describe('Tab index (default: active tab)')
        },
        async (args) => {
          try {
            const result = await client.navigate(args.url, args.tabIndex);
            return {
              content: [{
                type: 'text',
                text: `Navigated to ${result.finalUrl || args.url} (tab ${result.tabIndex || 0})`
              }]
            };
          } catch (error) {
            return {
              content: [{ type: 'text', text: `Navigation failed: ${error instanceof Error ? error.message : String(error)}` }],
              isError: true
            };
          }
        }
      ),

      tool(
        'browser_back',
        'Navigate back in browser history',
        { tabIndex: z.number().optional().describe('Tab index (default: active tab)') },
        async (args) => {
          try {
            const result = await client.request('/internal/history', 'POST', { action: 'back', tabIndex: args.tabIndex });
            return {
              content: [{ type: 'text', text: `Went back to ${result.finalUrl || 'previous page'}` }]
            };
          } catch (error) {
            return { content: [{ type: 'text', text: `Back navigation failed: ${error instanceof Error ? error.message : String(error)}` }], isError: true };
          }
        }
      ),

      tool(
        'browser_forward',
        'Navigate forward in browser history',
        { tabIndex: z.number().optional().describe('Tab index (default: active tab)') },
        async (args) => {
          try {
            const result = await client.request('/internal/history', 'POST', { action: 'forward', tabIndex: args.tabIndex });
            return {
              content: [{ type: 'text', text: `Moved forward to ${result.finalUrl || 'next page'}` }]
            };
          } catch (error) {
            return { content: [{ type: 'text', text: `Forward navigation failed: ${error instanceof Error ? error.message : String(error)}` }], isError: true };
          }
        }
      ),

      tool(
        'browser_reload',
        'Reload the current page',
        {
          tabIndex: z.number().optional().describe('Tab index (default: active tab)'),
          ignoreCache: z.boolean().optional().describe('Bypass cache (hard reload)')
        },
        async (args) => {
          try {
            await client.request('/internal/reload', 'POST', { tabIndex: args.tabIndex, ignoreCache: args.ignoreCache });
            return {
              content: [{ type: 'text', text: `Page reloaded${args.ignoreCache ? ' (hard reload)' : ''}` }]
            };
          } catch (error) {
            return { content: [{ type: 'text', text: `Reload failed: ${error instanceof Error ? error.message : String(error)}` }], isError: true };
          }
        }
      ),

      // Information Tools
      tool(
        'browser_get_url',
        'Get the current URL of the active or specified tab',
        { tabIndex: z.number().optional().describe('Tab index (default: active tab)') },
        async (args) => {
          try {
            const result = await client.getUrl(args.tabIndex);
            return {
              content: [{ type: 'text', text: `Current URL: ${result.url || 'https://example.com'}` }]
            };
          } catch (error) {
            return { content: [{ type: 'text', text: `Failed to fetch URL: ${error instanceof Error ? error.message : String(error)}` }], isError: true };
          }
        }
      ),

      tool(
        'browser_get_html',
        'Get the HTML source of the current page',
        { tabIndex: z.number().optional().describe('Tab index (default: active tab)') },
        async (args) => {
          try {
            const result = await client.getHtml(args.tabIndex);
            return {
              content: [{ type: 'text', text: result.html || '' }]
            };
          } catch (error) {
            return { content: [{ type: 'text', text: `Failed to fetch HTML: ${error instanceof Error ? error.message : String(error)}` }], isError: true };
          }
        }
      ),

      tool(
        'browser_get_page_summary',
        'Get a compact summary of the current page (title, headings, element counts). Much smaller than full HTML (~1-2KB vs 100KB+).',
        { tabIndex: z.number().optional().describe('Tab index (default: active tab)') },
        async (args) => {
          try {
            const result = await client.request('/internal/get_page_summary', 'GET', undefined, { tabIndex: args.tabIndex });
            const summary = result.summary || {};
            const text = `Page Summary:
Title: ${summary.title || 'N/A'}
URL: ${summary.url || 'N/A'}
Headings: ${summary.headings?.length || 0}
Forms: ${summary.forms || 0}, Links: ${summary.links || 0}, Buttons: ${summary.buttons || 0}

Main Content: ${summary.mainText || 'N/A'}`;
            return { content: [{ type: 'text', text }] };
          } catch (error) {
            return { content: [{ type: 'text', text: `Failed to fetch page summary: ${error instanceof Error ? error.message : String(error)}` }], isError: true };
          }
        }
      ),

      tool(
        'browser_get_interactive_elements',
        'Get all clickable elements on the page with their positions and attributes',
        { tabIndex: z.number().optional().describe('Tab index (default: active tab)') },
        async (args) => {
          try {
            const result = await client.request('/internal/get_interactive_elements', 'GET', undefined, { tabIndex: args.tabIndex });
            const elements = Array.isArray(result.elements) ? result.elements : [];
            const text = `Found ${elements.length} interactive elements:\n${elements.slice(0, 10).map((el: any, i: number) =>
              `${i + 1}. ${el.tag}: ${el.text || el.ariaLabel || el.href || 'N/A'}`
            ).join('\n')}${elements.length > 10 ? `\n... and ${elements.length - 10} more` : ''}`;
            return { content: [{ type: 'text', text }] };
          } catch (error) {
            return { content: [{ type: 'text', text: `Failed to fetch interactive elements: ${error instanceof Error ? error.message : String(error)}` }], isError: true };
          }
        }
      ),

      tool(
        'browser_get_accessibility_tree',
        'Get semantic page structure using accessibility tree',
        { tabIndex: z.number().optional().describe('Tab index (default: active tab)') },
        async (args) => {
          try {
            const result = await client.request('/internal/get_accessibility_tree', 'GET', undefined, { tabIndex: args.tabIndex });
            return {
              content: [{ type: 'text', text: JSON.stringify(result.tree || {}, null, 2) }]
            };
          } catch (error) {
            return { content: [{ type: 'text', text: `Failed to fetch accessibility tree: ${error instanceof Error ? error.message : String(error)}` }], isError: true };
          }
        }
      ),

      tool(
        'browser_query_content',
        'Query specific content types from the page. Available: "forms", "navigation", "article", "tables", "media"',
        {
          queryType: z.enum(['forms', 'navigation', 'article', 'tables', 'media']).describe('Type of content to query'),
          tabIndex: z.number().optional().describe('Tab index (default: active tab)')
        },
        async (args) => {
          try {
            const result = await client.request('/internal/query_content', 'POST', { queryType: args.queryType, tabIndex: args.tabIndex });
            return {
              content: [{ type: 'text', text: JSON.stringify(result.data || {}, null, 2) }]
            };
          } catch (error) {
            return { content: [{ type: 'text', text: `Failed to query content: ${error instanceof Error ? error.message : String(error)}` }], isError: true };
          }
        }
      ),

      tool(
        'browser_get_annotated_screenshot',
        'Get screenshot with interactive element annotations',
        { tabIndex: z.number().optional().describe('Tab index (default: active tab)') },
        async (args) => {
          try {
            const result = await client.request('/internal/get_annotated_screenshot', 'GET', undefined, { tabIndex: args.tabIndex });
            return {
              content: [
                { type: 'image', data: result.screenshot || '', mimeType: 'image/png' },
                { type: 'text', text: `Annotated ${(result.elements || []).length} interactive elements` }
              ]
            };
          } catch (error) {
            return { content: [{ type: 'text', text: `Failed to get annotated screenshot: ${error instanceof Error ? error.message : String(error)}` }], isError: true };
          }
        }
      ),

      // Interaction Tools
      tool(
        'browser_execute_js',
        'Execute JavaScript code in the page context',
        {
          code: z.string().describe('JavaScript code to execute'),
          tabIndex: z.number().optional().describe('Tab index (default: active tab)')
        },
        async (args) => {
          try {
            const result = await client.executeJs(args.code, args.tabIndex);
            return {
              content: [{ type: 'text', text: `JavaScript executed. Result: ${result.result ?? 'undefined'}` }]
            };
          } catch (error) {
            return { content: [{ type: 'text', text: `Failed to execute JavaScript: ${error instanceof Error ? error.message : String(error)}` }], isError: true };
          }
        }
      ),

      tool(
        'browser_screenshot',
        'Capture a screenshot of the current page',
        {
          tabIndex: z.number().optional().describe('Tab index (default: active tab)'),
          fullPage: z.boolean().optional().describe('Capture full page scroll height')
        },
        async (args) => {
          try {
            const result = await client.screenshot(args.tabIndex, args.fullPage);
            return {
              content: [{ type: 'image', data: result.screenshot, mimeType: 'image/png' }]
            };
          } catch (error) {
            return { content: [{ type: 'text', text: `Failed to capture screenshot: ${error instanceof Error ? error.message : String(error)}` }], isError: true };
          }
        }
      ),

      // Tab Management Tools
      tool(
        'window_create_tab',
        'Create a new browser tab',
        { url: z.string().url().describe('URL to load in the new tab') },
        async (args) => {
          try {
            const result = await client.createTab(args.url);
            return {
              content: [{ type: 'text', text: `Created new tab at index ${result.tabIndex ?? 0}` }]
            };
          } catch (error) {
            return { content: [{ type: 'text', text: `Failed to create tab: ${error instanceof Error ? error.message : String(error)}` }], isError: true };
          }
        }
      ),

      tool(
        'window_close_tab',
        'Close a browser tab',
        { tabIndex: z.number().describe('Index of the tab to close') },
        async (args) => {
          try {
            await client.closeTab(args.tabIndex);
            return {
              content: [{ type: 'text', text: `Closed tab at index ${args.tabIndex}` }]
            };
          } catch (error) {
            return { content: [{ type: 'text', text: `Failed to close tab: ${error instanceof Error ? error.message : String(error)}` }], isError: true };
          }
        }
      ),

      tool(
        'window_switch_tab',
        'Switch to a different browser tab',
        { tabIndex: z.number().describe('Index of the tab to switch to') },
        async (args) => {
          try {
            await client.switchTab(args.tabIndex);
            return {
              content: [{ type: 'text', text: `Switched to tab ${args.tabIndex}` }]
            };
          } catch (error) {
            return { content: [{ type: 'text', text: `Failed to switch tab: ${error instanceof Error ? error.message : String(error)}` }], isError: true };
          }
        }
      ),

      tool(
        'window_get_tab_info',
        'Get information about all tabs (count and active tab index)',
        {},
        async () => {
          try {
            const result = await client.getTabInfo();
            return {
              content: [{ type: 'text', text: `Total tabs: ${result.count ?? 0}, Active tab: ${result.activeTabIndex ?? 0}` }]
            };
          } catch (error) {
            return { content: [{ type: 'text', text: `Failed to get tab info: ${error instanceof Error ? error.message : String(error)}` }], isError: true };
          }
        }
      )
    ]
  });
}
