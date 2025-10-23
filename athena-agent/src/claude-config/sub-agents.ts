/**
 * Sub-Agent Configuration
 *
 * Defines specialized sub-agents for browser automation tasks.
 * Each agent has specific tools and expertise for different aspects of web interaction.
 */

export interface SubAgentDefinition {
  description: string;
  prompt: string;
  tools: string[];
  model: 'sonnet' | 'haiku';
}

export type SubAgents = Record<string, SubAgentDefinition>;

/**
 * Sub-agent configuration manager
 */
export class SubAgentConfig {
  /**
   * Get all sub-agents configuration for specialized browser tasks.
   * RESTRICTED: Only browser MCP tools, no file system access.
   */
  static getAgents(): SubAgents {
    return {
      'web-analyzer': {
        description: 'Expert in analyzing web page structure, extracting information, and understanding semantic content. Use for page analysis, data extraction, and content understanding tasks.',
        prompt: `You specialize in web page analysis and data extraction.

Your expertise:
- Analyzing HTML structure and DOM trees
- Extracting structured data from pages
- Understanding semantic content and accessibility trees
- Identifying key page elements and their relationships
- Optimizing data extraction strategies

Always use the most efficient tools:
1. Start with page_summary for overview
2. Use query_content for targeted extraction
3. Use accessibility_tree for semantic structure
4. Only use full HTML when absolutely necessary

Provide clear, structured output.`,
        tools: [
          'mcp__athena-browser__browser_get_url',
          'mcp__athena-browser__browser_get_html',
          'mcp__athena-browser__browser_get_page_summary',
          'mcp__athena-browser__browser_get_interactive_elements',
          'mcp__athena-browser__browser_get_accessibility_tree',
          'mcp__athena-browser__browser_query_content',
          'mcp__athena-browser__browser_screenshot'
        ],
        model: 'sonnet'
      },

      'navigation-expert': {
        description: 'Expert in browser navigation, multi-step workflows, and tab management. Use for complex navigation tasks, site exploration, and workflow automation.',
        prompt: `You specialize in browser navigation and workflow automation.

Your expertise:
- Planning and executing multi-step navigation workflows
- Managing browser tabs efficiently
- Handling history navigation (back/forward)
- Recovering from navigation errors
- Optimizing navigation paths

Best practices:
1. Verify current URL before navigating
2. Check page load success after navigation
3. Use page_summary to confirm correct page
4. Manage tabs strategically (close unused tabs)
5. Handle redirects and errors gracefully

Always confirm successful navigation.`,
        tools: [
          'mcp__athena-browser__browser_navigate',
          'mcp__athena-browser__browser_back',
          'mcp__athena-browser__browser_forward',
          'mcp__athena-browser__browser_reload',
          'mcp__athena-browser__browser_get_url',
          'mcp__athena-browser__browser_get_page_summary',
          'mcp__athena-browser__window_create_tab',
          'mcp__athena-browser__window_close_tab',
          'mcp__athena-browser__window_switch_tab',
          'mcp__athena-browser__window_get_tab_info'
        ],
        model: 'haiku'
      },

      'form-automation': {
        description: 'Expert in web form interactions, input validation, and form submission. Use for filling forms, handling input fields, and automating form-based workflows.',
        prompt: `You specialize in web form automation and interaction.

Your expertise:
- Analyzing form structure and fields
- Identifying form inputs and their types
- Generating appropriate JavaScript for form filling
- Validating input before submission
- Handling form errors and validation

Process:
1. Use query_content with "forms" to get form structure
2. Analyze required fields and validation rules
3. Generate safe JavaScript for form filling
4. Verify form state before submission
5. Handle errors and edge cases

Always validate inputs and explain your approach.`,
        tools: [
          'mcp__athena-browser__browser_get_page_summary',
          'mcp__athena-browser__browser_query_content',
          'mcp__athena-browser__browser_get_interactive_elements',
          'mcp__athena-browser__browser_execute_js',
          'mcp__athena-browser__browser_screenshot'
        ],
        model: 'sonnet'
      },

      'screenshot-analyst': {
        description: 'Expert in analyzing visual content from screenshots, understanding UI layout, and identifying visual elements. Use for visual debugging and UI understanding.',
        prompt: `You specialize in visual analysis of web pages.

Your expertise:
- Analyzing screenshots for UI elements
- Understanding visual layout and design
- Identifying clickable elements from visual cues
- Debugging visual rendering issues
- Comparing before/after states

Tools:
1. Use annotated_screenshot for element identification
2. Use regular screenshot for visual debugging
3. Combine with accessibility_tree for semantic context
4. Use interactive_elements for clickability verification

Provide detailed visual analysis.`,
        tools: [
          'mcp__athena-browser__browser_screenshot',
          'mcp__athena-browser__browser_get_annotated_screenshot',
          'mcp__athena-browser__browser_get_interactive_elements',
          'mcp__athena-browser__browser_get_accessibility_tree'
        ],
        model: 'sonnet'
      }
    };
  }

  /**
   * Get tools for a specific sub-agent
   */
  static getAgentTools(agentName: string): string[] {
    const agents = this.getAgents();
    return agents[agentName]?.tools || [];
  }

  /**
   * Get list of all available sub-agent names
   */
  static getAgentNames(): string[] {
    return Object.keys(this.getAgents());
  }

  /**
   * Get a specific sub-agent configuration
   */
  static getAgent(agentName: string): SubAgentDefinition | null {
    const agents = this.getAgents();
    return agents[agentName] || null;
  }
}
