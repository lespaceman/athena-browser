/**
 * Unified v1 API router for browser control.
 *
 * These endpoints form the single public interface that MCP tools consume.
 * Each handler delegates to the active BrowserController implementation,
 * providing consistent JSON responses and error handling.
 */

import express from 'express';
import type { BrowserController } from '../routes/browser.js';

function toErrorResponse(error: unknown): { success: false; error: string } {
  if (error instanceof Error) {
    return { success: false, error: error.message };
  }
  return { success: false, error: String(error) };
}

function ensureController(controller: BrowserController | null): asserts controller is BrowserController {
  if (!controller) {
    throw new Error('Browser controller not initialised');
  }
}

/**
 * Create the v1 API router.
 */
export function createV1Router(controller: BrowserController | null) {
  const router = express.Router();

  router.post('/browser/navigate', async (req, res) => {
    try {
      ensureController(controller);
      const { url, tabIndex } = req.body ?? {};

      if (typeof url !== 'string' || url.length === 0) {
        res.status(400).json({ success: false, error: 'url must be a non-empty string' });
        return;
      }

      const start = Date.now();
      await controller.navigate(url, typeof tabIndex === 'number' ? tabIndex : undefined);
      const duration = Date.now() - start;

      const activeTab = await controller.getActiveTabIndex();
      const finalUrl = await controller.getCurrentUrl(activeTab);

      res.json({
        success: true,
        finalUrl,
        tabIndex: activeTab,
        loadTimeMs: duration
      });
    } catch (error) {
      const err = toErrorResponse(error);
      res.status(500).json(err);
    }
  });

  router.post('/browser/back', async (req, res) => {
    try {
      ensureController(controller);
      const { tabIndex } = req.body ?? {};
      const start = Date.now();
      await controller.goBack(typeof tabIndex === 'number' ? tabIndex : undefined);
      const duration = Date.now() - start;
      const activeTab = await controller.getActiveTabIndex();
      const finalUrl = await controller.getCurrentUrl(activeTab);

      res.json({ success: true, tabIndex: activeTab, finalUrl, loadTimeMs: duration });
    } catch (error) {
      const err = toErrorResponse(error);
      res.status(500).json(err);
    }
  });

  router.post('/browser/forward', async (req, res) => {
    try {
      ensureController(controller);
      const { tabIndex } = req.body ?? {};
      const start = Date.now();
      await controller.goForward(typeof tabIndex === 'number' ? tabIndex : undefined);
      const duration = Date.now() - start;
      const activeTab = await controller.getActiveTabIndex();
      const finalUrl = await controller.getCurrentUrl(activeTab);

      res.json({ success: true, tabIndex: activeTab, finalUrl, loadTimeMs: duration });
    } catch (error) {
      const err = toErrorResponse(error);
      res.status(500).json(err);
    }
  });

  router.post('/browser/reload', async (req, res) => {
    try {
      ensureController(controller);
      const { tabIndex, ignoreCache } = req.body ?? {};
      const start = Date.now();
      await controller.reload(
        typeof tabIndex === 'number' ? tabIndex : undefined,
        typeof ignoreCache === 'boolean' ? ignoreCache : undefined
      );
      const duration = Date.now() - start;
      const activeTab = await controller.getActiveTabIndex();
      const finalUrl = await controller.getCurrentUrl(activeTab);

      res.json({
        success: true,
        tabIndex: activeTab,
        finalUrl,
        loadTimeMs: duration,
        ignoreCache: Boolean(ignoreCache)
      });
    } catch (error) {
      const err = toErrorResponse(error);
      res.status(500).json(err);
    }
  });

  router.get('/browser/url', async (req, res) => {
    try {
      ensureController(controller);
      const tabIndex = typeof req.query.tabIndex === 'string' ? Number(req.query.tabIndex) : undefined;
      const url = await controller.getCurrentUrl(Number.isNaN(tabIndex) ? undefined : tabIndex);
      res.json({ success: true, url });
    } catch (error) {
      const err = toErrorResponse(error);
      res.status(500).json(err);
    }
  });

  router.get('/browser/html', async (req, res) => {
    try {
      ensureController(controller);
      const tabIndex = typeof req.query.tabIndex === 'string' ? Number(req.query.tabIndex) : undefined;
      const html = await controller.getPageHtml(Number.isNaN(tabIndex) ? undefined : tabIndex);
      res.json({ success: true, html });
    } catch (error) {
      const err = toErrorResponse(error);
      res.status(500).json(err);
    }
  });

  router.post('/browser/execute-js', async (req, res) => {
    try {
      ensureController(controller);
      const { code, tabIndex } = req.body ?? {};
      if (typeof code !== 'string' || code.length === 0) {
        res.status(400).json({ success: false, error: 'code must be a non-empty string' });
        return;
      }

      const result = await controller.executeJavaScript(
        code,
        typeof tabIndex === 'number' ? tabIndex : undefined
      );

      res.json({
        success: true,
        result
      });
    } catch (error) {
      const err = toErrorResponse(error);
      res.status(500).json(err);
    }
  });

  router.post('/browser/screenshot', async (req, res) => {
    try {
      ensureController(controller);
      const { tabIndex, fullPage } = req.body ?? {};
      const screenshot = await controller.screenshot(
        typeof tabIndex === 'number' ? tabIndex : undefined,
        typeof fullPage === 'boolean' ? fullPage : undefined
      );
      res.json({ success: true, screenshot });
    } catch (error) {
      const err = toErrorResponse(error);
      res.status(500).json(err);
    }
  });

  router.post('/window/create', async (req, res) => {
    try {
      ensureController(controller);
      const { url } = req.body ?? {};
      if (typeof url !== 'string' || url.length === 0) {
        res.status(400).json({ success: false, error: 'url must be a non-empty string' });
        return;
      }

      const tabIndex = await controller.createTab(url);
      res.json({ success: true, tabIndex });
    } catch (error) {
      const err = toErrorResponse(error);
      res.status(500).json(err);
    }
  });

  router.post('/window/close', async (req, res) => {
    try {
      ensureController(controller);
      const { tabIndex } = req.body ?? {};
      if (typeof tabIndex !== 'number') {
        res.status(400).json({ success: false, error: 'tabIndex must be provided' });
        return;
      }

      await controller.closeTab(tabIndex);
      res.json({ success: true });
    } catch (error) {
      const err = toErrorResponse(error);
      res.status(500).json(err);
    }
  });

  router.post('/window/switch', async (req, res) => {
    try {
      ensureController(controller);
      const { tabIndex } = req.body ?? {};
      if (typeof tabIndex !== 'number') {
        res.status(400).json({ success: false, error: 'tabIndex must be provided' });
        return;
      }

      await controller.switchToTab(tabIndex);
      res.json({ success: true, activeTab: tabIndex });
    } catch (error) {
      const err = toErrorResponse(error);
      res.status(500).json(err);
    }
  });

  router.get('/window/tabs', async (_req, res) => {
    try {
      ensureController(controller);
      const tabCount = await controller.getTabCount();
      const activeTabIndex = await controller.getActiveTabIndex();
      res.json({ success: true, tabCount, activeTabIndex });
    } catch (error) {
      const err = toErrorResponse(error);
      res.status(500).json(err);
    }
  });

  return router;
}
