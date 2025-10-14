/**
 * POC Routes
 *
 * Minimal proof-of-concept endpoint for opening URLs with load confirmation.
 * This is the single happy-path test to prove Claude → HTTP → Browser flow.
 */

import type { Request, Response } from 'express';
import { Logger } from '../logger.js';
import { getController } from './browser.js';

const logger = new Logger('POCRoutes');

// ============================================================================
// URL Allowlist (Safety Guardrail for POC)
// ============================================================================

const ALLOWED_DOMAINS = [
  'example.com',
  'github.com',
  'google.com',
  'anthropic.com',
  'wikipedia.org',
  'stackoverflow.com',
  'developer.mozilla.org',
  'npmjs.com',
  'nodejs.org',
  'python.org',
  'rust-lang.org',
  'docs.rs',
  'crates.io',
  'pypi.org',
  'w3.org',
  'youtube.com',
  'vimeo.com',
  'reddit.com',
  'medium.com',
  'dev.to',
  'hackernews.com',
  'news.ycombinator.com',
  'cloudflare.com',
  'aws.amazon.com',
  'azure.microsoft.com',
  'cloud.google.com',
  'docker.com',
  'kubernetes.io',
  'gitlab.com',
  'bitbucket.org',
  'atlassian.com',
  'slack.com',
  'discord.com',
  'notion.so',
  'trello.com',
  'asana.com',
  'linear.app',
  'figma.com',
  'canva.com',
  'unsplash.com',
  'pexels.com',
  'freepik.com'
];

/**
 * Check if URL is on the allowlist.
 * For POC safety, only allow specific domains.
 */
function isUrlAllowed(url: string): boolean {
  try {
    const parsed = new URL(url);

    // Only allow https
    if (parsed.protocol !== 'https:') {
      return false;
    }

    // Check if hostname matches allowlist
    return ALLOWED_DOMAINS.some(domain =>
      parsed.hostname === domain ||
      parsed.hostname === `www.${domain}` ||
      parsed.hostname.endsWith(`.${domain}`)
    );
  } catch {
    return false;
  }
}

// ============================================================================
// Route Handler
// ============================================================================

/**
 * POST /v1/poc/open_url
 * Open a URL and wait for load confirmation (POC endpoint)
 */
export async function openUrlHandler(req: Request, res: Response): Promise<void> {
  try {
    const { url } = req.body;

    // Validate input
    if (!url || typeof url !== 'string') {
      res.status(400).json({
        success: false,
        error: 'Bad Request: url parameter is required and must be a string'
      });
      return;
    }

    // Validate URL format
    try {
      new URL(url);
    } catch {
      res.status(400).json({
        success: false,
        error: `Bad Request: Invalid URL format: ${url}`
      });
      return;
    }

    // Check allowlist
    if (!isUrlAllowed(url)) {
      res.status(403).json({
        success: false,
        error: `Forbidden: URL not on allowlist. Allowed domains: ${ALLOWED_DOMAINS.join(', ')}`
      });
      return;
    }

    logger.info('POC: Opening URL request', { url });

    // Call controller
    const controller = getController();
    const result = await controller.openUrl(url, 10000); // 10s timeout

    // Return result
    if (result.success) {
      res.json(result);
    } else {
      res.status(500).json(result);
    }
  } catch (error) {
    logger.error('POC: Open URL failed', {
      error: error instanceof Error ? error.message : String(error)
    });

    res.status(500).json({
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error'
    });
  }
}
