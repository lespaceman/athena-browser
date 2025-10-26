/**
 * Permission Handler
 *
 * Manages dynamic permission callbacks for tool usage decisions.
 * Only handles dynamic logic (JavaScript execution confirmation).
 * Static tool permissions are handled by the allowedTools array.
 */

import { Logger } from '../../server/logger';

const logger = new Logger('PermissionHandler');

export interface PermissionResult {
  behavior: 'allow' | 'ask' | 'deny';
  message?: string;
}

/**
 * Permission handler for browser tool usage
 */
export class PermissionHandler {
  /**
   * Custom permission callback for dynamic tool usage decisions.
   * Only handles dynamic logic (JavaScript execution confirmation).
   * Static tool permissions are handled by the allowedTools array.
   */
  static async canUseTool(toolName: string, input: any): Promise<PermissionResult> {
    // Require confirmation for JavaScript execution
    if (toolName === 'mcp__athena-browser__browser_execute_js') {
      logger.warn('JavaScript execution requested', { code: input.code });
      return {
        behavior: 'ask',
        message: `Allow JavaScript execution: "${input.code?.substring(0, 100)}..."?`
      };
    }

    // All other tools are controlled by allowedTools array
    return { behavior: 'allow' };
  }
}
