/**
 * Browser Control Routes
 *
 * HTTP endpoints for controlling the Athena Browser via MCP tools.
 * These routes are called by the MCP server and forward commands to the C++ application.
 */

import type { Request, Response } from 'express';
import { Logger } from '../logger';

const logger = new Logger('BrowserRoutes');

// ============================================================================
// Browser Control Interface
// ============================================================================

/**
 * Result of opening a URL (POC endpoint)
 */
export interface OpenUrlResult {
  success: boolean;
  finalUrl?: string;
  tabIndex?: number;
  loadTimeMs?: number;
  error?: string;
}

/**
 * Interface for browser control operations.
 * This will be implemented by a bridge to the C++ QtMainWindow API.
 */
export interface BrowserController {
  // Navigation
  navigate(url: string, tabIndex?: number): Promise<void>;
  goBack(tabIndex?: number): Promise<void>;
  goForward(tabIndex?: number): Promise<void>;
  reload(tabIndex?: number, ignoreCache?: boolean): Promise<void>;

  // Information
  getCurrentUrl(tabIndex?: number): Promise<string>;
  getPageHtml(tabIndex?: number): Promise<string>;

  // Interaction
  executeJavaScript(code: string, tabIndex?: number): Promise<unknown>;
  screenshot(tabIndex?: number, fullPage?: boolean, quality?: number, maxWidth?: number, maxHeight?: number): Promise<string>; // Returns base64

  // Tab management
  createTab(url: string): Promise<number>; // Returns tab index
  closeTab(tabIndex: number): Promise<void>;
  switchToTab(tabIndex: number): Promise<void>;
  getTabCount(): Promise<number>;
  getActiveTabIndex(): Promise<number>;

  // POC: Combined operation
  /**
   * Open URL and wait for load complete (POC endpoint).
   * Creates/reuses tab, navigates, and waits for load confirmation.
   * @param url URL to open
   * @param timeoutMs Timeout in milliseconds (default: 10000)
   * @returns Result with success status and load metrics
   */
  openUrl(url: string, timeoutMs?: number): Promise<OpenUrlResult>;
}

/**
 * Global browser controller instance.
 * This is set by the C++ application when it creates the window.
 */
let browserController: BrowserController | null = null;

/**
 * Set the browser controller instance.
 * Called by C++ during window initialization.
 */
export function setBrowserController(controller: BrowserController) {
  browserController = controller;
  logger.info('Browser controller registered');
}

/**
 * Get the browser controller instance.
 * Throws if not initialized.
 */
export function getController(): BrowserController {
  if (!browserController) {
    throw new Error('Browser controller not initialized. Window may not be ready yet.');
  }
  return browserController;
}

// ============================================================================
// Route Handlers
// ============================================================================

/**
 * POST /v1/browser/navigate
 * Navigate to a URL
 */
export async function navigateHandler(req: Request, res: Response): Promise<void> {
  try {
    const { url, tabIndex } = req.body;

    if (!url || typeof url !== 'string') {
      res.status(400).json({
        error: 'Bad Request',
        message: 'url parameter is required and must be a string'
      });
      return;
    }

    logger.info('Navigate request', { url, tabIndex });

    const controller = getController();
    await controller.navigate(url, tabIndex);

    res.json({
      success: true,
      message: `Navigating to ${url}`
    });
  } catch (error) {
    logger.error('Navigate failed', { error: error instanceof Error ? error.message : String(error) });
    res.status(500).json({
      error: 'Internal Server Error',
      message: error instanceof Error ? error.message : 'Unknown error'
    });
  }
}

/**
 * POST /v1/browser/back
 * Navigate back in history
 */
export async function backHandler(req: Request, res: Response) {
  try {
    const { tabIndex } = req.body || {};

    logger.info('Back request', { tabIndex });

    const controller = getController();
    await controller.goBack(tabIndex);

    res.json({
      success: true,
      message: 'Navigated back'
    });
  } catch (error) {
    logger.error('Back failed', { error: error instanceof Error ? error.message : String(error) });
    res.status(500).json({
      error: 'Internal Server Error',
      message: error instanceof Error ? error.message : 'Unknown error'
    });
  }
}

/**
 * POST /v1/browser/forward
 * Navigate forward in history
 */
export async function forwardHandler(req: Request, res: Response) {
  try {
    const { tabIndex } = req.body || {};

    logger.info('Forward request', { tabIndex });

    const controller = getController();
    await controller.goForward(tabIndex);

    res.json({
      success: true,
      message: 'Navigated forward'
    });
  } catch (error) {
    logger.error('Forward failed', { error: error instanceof Error ? error.message : String(error) });
    res.status(500).json({
      error: 'Internal Server Error',
      message: error instanceof Error ? error.message : 'Unknown error'
    });
  }
}

/**
 * POST /v1/browser/reload
 * Reload current page
 */
export async function reloadHandler(req: Request, res: Response) {
  try {
    const { tabIndex, ignoreCache } = req.body || {};

    logger.info('Reload request', { tabIndex, ignoreCache });

    const controller = getController();
    await controller.reload(tabIndex, ignoreCache);

    res.json({
      success: true,
      message: ignoreCache ? 'Page reloaded (cache bypassed)' : 'Page reloaded'
    });
  } catch (error) {
    logger.error('Reload failed', { error: error instanceof Error ? error.message : String(error) });
    res.status(500).json({
      error: 'Internal Server Error',
      message: error instanceof Error ? error.message : 'Unknown error'
    });
  }
}

/**
 * GET /v1/browser/get_url
 * Get current URL
 */
export async function getUrlHandler(req: Request, res: Response) {
  try {
    const tabIndex = req.query.tabIndex ? parseInt(req.query.tabIndex as string) : undefined;

    logger.info('Get URL request', { tabIndex });

    const controller = getController();
    const url = await controller.getCurrentUrl(tabIndex);

    res.json({
      success: true,
      url
    });
  } catch (error) {
    logger.error('Get URL failed', { error: error instanceof Error ? error.message : String(error) });
    res.status(500).json({
      error: 'Internal Server Error',
      message: error instanceof Error ? error.message : 'Unknown error'
    });
  }
}

/**
 * GET /v1/browser/get_html
 * Get page HTML
 */
export async function getHtmlHandler(req: Request, res: Response) {
  try {
    const tabIndex = req.query.tabIndex ? parseInt(req.query.tabIndex as string) : undefined;

    logger.info('Get HTML request', { tabIndex });

    const controller = getController();
    const html = await controller.getPageHtml(tabIndex);

    res.json({
      success: true,
      html
    });
  } catch (error) {
    logger.error('Get HTML failed', { error: error instanceof Error ? error.message : String(error) });
    res.status(500).json({
      error: 'Internal Server Error',
      message: error instanceof Error ? error.message : 'Unknown error'
    });
  }
}

/**
 * POST /v1/browser/execute_js
 * Execute JavaScript in page context
 */
export async function executeJsHandler(req: Request, res: Response): Promise<void> {
  try {
    const { code, tabIndex } = req.body;

    if (!code || typeof code !== 'string') {
      res.status(400).json({
        error: 'Bad Request',
        message: 'code parameter is required and must be a string'
      });
      return;
    }

    logger.info('Execute JS request', { codeLength: code.length, tabIndex });

    const controller = getController();
    const result = await controller.executeJavaScript(code, tabIndex);

    res.json({
      success: true,
      result
    });
  } catch (error) {
    logger.error('Execute JS failed', { error: error instanceof Error ? error.message : String(error) });
    res.status(500).json({
      error: 'Internal Server Error',
      message: error instanceof Error ? error.message : 'Unknown error'
    });
  }
}

/**
 * POST /v1/browser/screenshot
 * Capture screenshot
 */
export async function screenshotHandler(req: Request, res: Response) {
  try {
    const { tabIndex, fullPage } = req.body || {};

    logger.info('Screenshot request', { tabIndex, fullPage });

    const controller = getController();
    const imageData = await controller.screenshot(tabIndex, fullPage);

    res.json({
      success: true,
      imageData // Base64 encoded
    });
  } catch (error) {
    logger.error('Screenshot failed', { error: error instanceof Error ? error.message : String(error) });
    res.status(500).json({
      error: 'Internal Server Error',
      message: error instanceof Error ? error.message : 'Unknown error'
    });
  }
}

/**
 * POST /v1/window/create_tab
 * Create a new tab
 */
export async function createTabHandler(req: Request, res: Response): Promise<void> {
  try {
    const { url } = req.body;

    if (!url || typeof url !== 'string') {
      res.status(400).json({
        error: 'Bad Request',
        message: 'url parameter is required and must be a string'
      });
      return;
    }

    logger.info('Create tab request', { url });

    const controller = getController();
    const tabIndex = await controller.createTab(url);

    res.json({
      success: true,
      tabIndex,
      message: `Created tab at index ${tabIndex}`
    });
  } catch (error) {
    logger.error('Create tab failed', { error: error instanceof Error ? error.message : String(error) });
    res.status(500).json({
      error: 'Internal Server Error',
      message: error instanceof Error ? error.message : 'Unknown error'
    });
  }
}

/**
 * POST /v1/window/close_tab
 * Close a tab
 */
export async function closeTabHandler(req: Request, res: Response): Promise<void> {
  try {
    const { tabIndex } = req.body;

    if (typeof tabIndex !== 'number') {
      res.status(400).json({
        error: 'Bad Request',
        message: 'tabIndex parameter is required and must be a number'
      });
      return;
    }

    logger.info('Close tab request', { tabIndex });

    const controller = getController();
    await controller.closeTab(tabIndex);

    res.json({
      success: true,
      message: `Closed tab at index ${tabIndex}`
    });
  } catch (error) {
    logger.error('Close tab failed', { error: error instanceof Error ? error.message : String(error) });
    res.status(500).json({
      error: 'Internal Server Error',
      message: error instanceof Error ? error.message : 'Unknown error'
    });
  }
}

/**
 * POST /v1/window/switch_tab
 * Switch to a tab
 */
export async function switchTabHandler(req: Request, res: Response): Promise<void> {
  try {
    const { tabIndex } = req.body;

    if (typeof tabIndex !== 'number') {
      res.status(400).json({
        error: 'Bad Request',
        message: 'tabIndex parameter is required and must be a number'
      });
      return;
    }

    logger.info('Switch tab request', { tabIndex });

    const controller = getController();
    await controller.switchToTab(tabIndex);

    res.json({
      success: true,
      message: `Switched to tab ${tabIndex}`
    });
  } catch (error) {
    logger.error('Switch tab failed', { error: error instanceof Error ? error.message : String(error) });
    res.status(500).json({
      error: 'Internal Server Error',
      message: error instanceof Error ? error.message : 'Unknown error'
    });
  }
}

/**
 * GET /v1/window/tab_info
 * Get tab information
 */
export async function tabInfoHandler(_req: Request, res: Response): Promise<void> {
  try {
    logger.info('Tab info request');

    const controller = getController();
    const tabCount = await controller.getTabCount();
    const activeTabIndex = await controller.getActiveTabIndex();

    res.json({
      success: true,
      tabCount,
      activeTabIndex
    });
  } catch (error) {
    logger.error('Tab info failed', { error: error instanceof Error ? error.message : String(error) });
    res.status(500).json({
      error: 'Internal Server Error',
      message: error instanceof Error ? error.message : 'Unknown error'
    });
  }
}
