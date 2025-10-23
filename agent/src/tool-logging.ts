/**
 * Helper utilities for logging MCP tool invocations without dumping large payloads.
 */

type AnyRecord = Record<string, unknown>;

function pickStringPreview(value: unknown, length: number = 120): string | undefined {
  if (typeof value !== 'string') {
    return undefined;
  }
  if (value.length <= length) {
    return value;
  }
  return `${value.slice(0, length)}…`;
}

function base64ApproxBytes(data: string): number {
  // Rough estimate: 4 chars encode 3 bytes.
  return Math.floor(data.length * 0.75);
}

export function summarizeToolResult(toolName: string, result: unknown): AnyRecord {
  const summary: AnyRecord = { tool: toolName };

  if (result && typeof result === 'object') {
    const record = result as AnyRecord;
    if ('success' in record) {
      summary.success = Boolean(record.success);
    }
    if (typeof record.error === 'string') {
      summary.error = record.error;
    }
    if (typeof record.message === 'string' && !summary.error) {
      summary.message = record.message;
    }
    if (typeof record.tabIndex === 'number') {
      summary.tabIndex = record.tabIndex;
    }
    if (typeof record.loadTimeMs === 'number') {
      summary.loadTimeMs = record.loadTimeMs;
    }
    if (typeof record.html === 'string') {
      summary.htmlLength = (record.html as string).length;
    }
    if (typeof record.result === 'string') {
      summary.resultPreview = pickStringPreview(record.result, 80);
    }
    if (typeof record.stringResult === 'string') {
      summary.stringResultPreview = pickStringPreview(record.stringResult, 80);
    }
    if (typeof record.type === 'string') {
      summary.type = record.type;
    }
    if ('loadWaitTimedOut' in record) {
      summary.loadWaitTimedOut = Boolean(record.loadWaitTimedOut);
    }
    if (typeof record.screenshot === 'string') {
      summary.screenshotBytes = base64ApproxBytes(record.screenshot);
    }
    if (Array.isArray(record.elements)) {
      summary.elementCount = record.elements.length;
    }
    if (Array.isArray(record.tabs)) {
      summary.tabCount = record.tabs.length;
    }
    if (Array.isArray(record.summary)) {
      summary.summaryLength = record.summary.length;
    }
    const keys = Object.keys(record);
    summary.keys = keys.slice(0, 10);
  } else if (typeof result === 'string') {
    summary.success = true;
    summary.resultType = 'string';
    summary.length = result.length;
    summary.preview = pickStringPreview(result);
  } else {
    summary.resultType = typeof result;
    summary.success = true;
  }

  return summary;
}

export function summarizeToolError(toolName: string, error: unknown): AnyRecord {
  if (error instanceof Error) {
    return {
      tool: toolName,
      message: error.message,
      stack: error.stack?.split('\n').slice(0, 3).join(' | ')
    };
  }

  return {
    tool: toolName,
    message: typeof error === 'string' ? error : String(error)
  };
}
