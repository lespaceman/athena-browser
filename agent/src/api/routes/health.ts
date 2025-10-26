/**
 * Health check endpoint
 */

import type { Request, Response } from 'express';
import type { HealthResponse } from '../../server/types';

const startTime = Date.now();

export function healthHandler(_req: Request, res: Response): void {
  const uptime = Date.now() - startTime;

  const response: HealthResponse = {
    status: 'healthy',
    ready: true,
    uptime,
    version: '1.0.0',
    features: [
      'chat',
      'claude-agent-sdk',
      'mcp-browser-tools',
      'streaming'
    ]
  };

  res.json(response);
}
