/**
 * Claude Code Agent SDK Integration
 *
 * Provides a high-level API for interacting with Claude Code from the Node runtime.
 * This allows the Node.js process to leverage Claude's capabilities for code analysis,
 * file operations, and other AI-assisted tasks.
 */

const { query } = require('@anthropic-ai/claude-agent-sdk');
const path = require('path');

/**
 * Claude client configuration
 */
class ClaudeClient {
  constructor(config = {}) {
    this.config = {
      cwd: config.cwd || process.cwd(),
      model: config.model || 'claude-sonnet-4-5',
      permissionMode: config.permissionMode || 'bypassPermissions',
      settingSources: config.settingSources || ['project'],
      allowedTools: config.allowedTools || [
        'Read',
        'Write',
        'Edit',
        'Glob',
        'Grep',
        'Bash',
        'WebFetch',
        'WebSearch'
      ],
      systemPrompt: config.systemPrompt || {
        type: 'preset',
        preset: 'claude_code'
      },
      ...config
    };
  }

  /**
   * Query Claude with a prompt and collect the full response
   *
   * @param {string} prompt - The prompt to send to Claude
   * @param {Object} options - Additional options to override defaults
   * @returns {Promise<Object>} Result with messages and final result
   */
  async query(prompt, options = {}) {
    const queryOptions = {
      ...this.config,
      ...options
    };

    const messages = [];
    let resultMessage = null;
    let systemMessage = null;

    try {
      const stream = query({
        prompt,
        options: queryOptions
      });

      for await (const message of stream) {
        messages.push(message);

        // Capture system initialization
        if (message.type === 'system' && message.subtype === 'init') {
          systemMessage = message;
        }

        // Capture final result
        if (message.type === 'result') {
          resultMessage = message;
        }
      }

      return {
        success: true,
        result: resultMessage,
        messages,
        system: systemMessage,
        sessionId: resultMessage?.session_id
      };
    } catch (error) {
      return {
        success: false,
        error: error.message,
        messages,
        system: systemMessage
      };
    }
  }

  /**
   * Query Claude with streaming response
   *
   * @param {string} prompt - The prompt to send to Claude
   * @param {Function} onMessage - Callback for each message
   * @param {Object} options - Additional options
   * @returns {Promise<Object>} Final result
   */
  async queryStream(prompt, onMessage, options = {}) {
    const queryOptions = {
      ...this.config,
      includePartialMessages: true,
      ...options
    };

    let resultMessage = null;
    let systemMessage = null;

    try {
      const stream = query({
        prompt,
        options: queryOptions
      });

      for await (const message of stream) {
        if (onMessage) {
          await onMessage(message);
        }

        if (message.type === 'system' && message.subtype === 'init') {
          systemMessage = message;
        }

        if (message.type === 'result') {
          resultMessage = message;
        }
      }

      return {
        success: true,
        result: resultMessage,
        system: systemMessage
      };
    } catch (error) {
      return {
        success: false,
        error: error.message
      };
    }
  }

  /**
   * Analyze code in a file
   *
   * @param {string} filePath - Path to the file to analyze
   * @param {string} question - Question about the code
   * @returns {Promise<Object>} Analysis result
   */
  async analyzeCode(filePath, question) {
    const prompt = `Analyze the code in ${filePath}. ${question}`;
    return this.query(prompt, {
      allowedTools: ['Read', 'Glob', 'Grep']
    });
  }

  /**
   * Generate code based on a specification
   *
   * @param {string} spec - Code specification
   * @param {string} outputPath - Where to write the generated code
   * @returns {Promise<Object>} Generation result
   */
  async generateCode(spec, outputPath = null) {
    let prompt = `Generate code based on this specification:\n\n${spec}`;
    if (outputPath) {
      prompt += `\n\nWrite the code to ${outputPath}`;
    }

    return this.query(prompt, {
      allowedTools: ['Write', 'Read', 'Glob', 'Grep']
    });
  }

  /**
   * Refactor code in a file
   *
   * @param {string} filePath - Path to the file to refactor
   * @param {string} instructions - Refactoring instructions
   * @returns {Promise<Object>} Refactoring result
   */
  async refactorCode(filePath, instructions) {
    const prompt = `Refactor the code in ${filePath}. ${instructions}`;
    return this.query(prompt, {
      allowedTools: ['Read', 'Edit', 'Write']
    });
  }

  /**
   * Search codebase for patterns
   *
   * @param {string} pattern - Search pattern (regex)
   * @param {string} globPattern - File pattern to search
   * @returns {Promise<Object>} Search results
   */
  async searchCode(pattern, globPattern = '**/*') {
    const prompt = `Search for the pattern "${pattern}" in files matching "${globPattern}"`;
    return this.query(prompt, {
      allowedTools: ['Grep', 'Glob']
    });
  }

  /**
   * Run a bash command and get the result
   *
   * @param {string} command - Command to execute
   * @returns {Promise<Object>} Command result
   */
  async runCommand(command) {
    const prompt = `Run this command: ${command}`;
    return this.query(prompt, {
      allowedTools: ['Bash'],
      permissionMode: 'bypassPermissions'
    });
  }

  /**
   * Fetch and analyze web content
   *
   * @param {string} url - URL to fetch
   * @param {string} question - Question about the content
   * @returns {Promise<Object>} Analysis result
   */
  async analyzeWeb(url, question) {
    const prompt = `Fetch ${url} and answer: ${question}`;
    return this.query(prompt, {
      allowedTools: ['WebFetch']
    });
  }

  /**
   * Search the web
   *
   * @param {string} searchQuery - Search query
   * @returns {Promise<Object>} Search results
   */
  async searchWeb(searchQuery) {
    const prompt = `Search the web for: ${searchQuery}`;
    return this.query(prompt, {
      allowedTools: ['WebSearch']
    });
  }

  /**
   * Resume a previous session
   *
   * @param {string} sessionId - Session ID to resume
   * @param {string} prompt - New prompt for the session
   * @returns {Promise<Object>} Result
   */
  async resumeSession(sessionId, prompt) {
    return this.query(prompt, {
      resume: sessionId
    });
  }

  /**
   * Continue the most recent conversation
   *
   * @param {string} prompt - New prompt to continue with
   * @returns {Promise<Object>} Result
   */
  async continueConversation(prompt) {
    return this.query(prompt, {
      continue: true
    });
  }
}

module.exports = { ClaudeClient };
