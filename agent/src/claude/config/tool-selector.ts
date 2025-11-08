/**
 * Tool Selection Logic
 *
 * Dynamically selects appropriate browser tools based on prompt analysis.
 * Only returns Athena browser MCP tools (no file system or CLI access).
 */

import { Logger } from '../../server/logger';

const logger = new Logger('ToolSelector');

export interface PromptAnalysis {
  isReadOnly: boolean;
  needsInteraction: boolean;
}

/**
 * Tool selector for browser automation
 */
export class ToolSelector {
  /**
   * Analyze prompt complexity and return appropriate tool set.
   * Only returns Athena browser MCP tools (no file system or CLI access).
   * This method provides the allowedTools array for static permission control.
   */
  static selectTools(prompt: string, enableMcp: boolean): string[] {
    if (!enableMcp) {
      logger.warn('MCP disabled - agent will have no browser automation tools available');
      return [];
    }

    const analysis = this.analyzePrompt(prompt);
    const tools: string[] = [];

    // For read-only browser tasks, only add read tools
    if (analysis.isReadOnly && !analysis.needsInteraction) {
      tools.push(...this.getReadOnlyTools());
      logger.debug('Using read-only browser tools', { toolCount: tools.length });
    } else {
      // For interactive tasks, add all browser tools (including navigation and JS execution)
      tools.push(...this.getInteractiveTools());
      logger.debug('Using full browser tools for interactive task', { toolCount: tools.length });
    }

    logger.debug('Tool selection complete (browser-only mode)', {
      promptLength: prompt.length,
      totalTools: tools.length,
      readOnly: analysis.isReadOnly,
      needsInteraction: analysis.needsInteraction
    });

    return tools;
  }

  /**
   * Analyze prompt to determine task type
   */
  private static analyzePrompt(prompt: string): PromptAnalysis {
    const lowerPrompt = prompt.toLowerCase();

    // Check for read-only vs. interactive tasks
    const isReadOnly =
      lowerPrompt.includes('show') ||
      lowerPrompt.includes('get') ||
      lowerPrompt.includes('find') ||
      lowerPrompt.includes('search') ||
      lowerPrompt.includes('analyze') ||
      lowerPrompt.includes('extract');

    const needsInteraction =
      lowerPrompt.includes('click') ||
      lowerPrompt.includes('fill') ||
      lowerPrompt.includes('submit') ||
      lowerPrompt.includes('execute') ||
      lowerPrompt.includes('navigate') ||
      lowerPrompt.includes('type');

    return {
      isReadOnly,
      needsInteraction
    };
  }

  /**
   * Get read-only browser tools (for information gathering)
   */
  private static getReadOnlyTools(): string[] {
    return [
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
  }

  /**
   * Get all browser tools (including navigation and interaction)
   */
  private static getInteractiveTools(): string[] {
    return [
      'mcp__athena-browser__browser_navigate',
      'mcp__athena-browser__browser_back',
      'mcp__athena-browser__browser_forward',
      'mcp__athena-browser__browser_reload',
      'mcp__athena-browser__browser_get_url',
      'mcp__athena-browser__browser_get_html',
      'mcp__athena-browser__browser_get_page_summary',
      'mcp__athena-browser__browser_get_interactive_elements',
      'mcp__athena-browser__browser_get_accessibility_tree',
      'mcp__athena-browser__browser_query_content',
      'mcp__athena-browser__browser_get_annotated_screenshot',
      'mcp__athena-browser__browser_execute_js',
      'mcp__athena-browser__browser_screenshot',
      'mcp__athena-browser__window_create_tab',
      'mcp__athena-browser__window_close_tab',
      'mcp__athena-browser__window_switch_tab',
      'mcp__athena-browser__window_get_tab_info'
    ];
  }

  /**
   * Get all available browser tools (complete set)
   */
  static getAllBrowserTools(): string[] {
    return this.getInteractiveTools();
  }
}
