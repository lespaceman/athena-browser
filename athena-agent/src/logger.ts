/**
 * Structured logging for Athena Agent
 */

import winston from 'winston';
import type { LogEntry } from './types.js';

const { createLogger, format, transports } = winston;

export class Logger {
  private logger: winston.Logger;
  private module: string;

  constructor(moduleName: string) {
    this.module = moduleName;

    // Create structured JSON logger
    this.logger = createLogger({
      level: process.env.LOG_LEVEL || 'info',
      format: format.combine(
        format.timestamp({ format: 'ISO' }),
        format.errors({ stack: true }),
        format.json()
      ),
      transports: [
        new transports.Console({
          format: format.combine(
            format.colorize({ all: false }), // No color codes for JSON
            format.json()
          ),
          // IMPORTANT: Write to stderr instead of stdout!
          // The C++ parent reads stdout for the READY signal, then closes the pipe.
          // If Winston writes to stdout after that, Node gets SIGPIPE and dies.
          // Stderr stays open and goes to the terminal, so logs work fine there.
          stderrLevels: ['error', 'warn', 'info', 'debug', 'verbose', 'silly']
        })
      ]
    });
  }

  private log(level: string, message: string, meta: Record<string, any> = {}): void {
    const entry: LogEntry = {
      timestamp: new Date().toISOString(),
      level,
      module: this.module,
      message,
      pid: process.pid,
      ...meta
    };

    this.logger.log(level, message, entry);
  }

  debug(message: string, meta?: Record<string, any>): void {
    this.log('debug', message, meta);
  }

  info(message: string, meta?: Record<string, any>): void {
    this.log('info', message, meta);
  }

  warn(message: string, meta?: Record<string, any>): void {
    this.log('warn', message, meta);
  }

  error(message: string, meta?: Record<string, any>): void {
    this.log('error', message, meta);
  }
}

// Create a global logger instance
export const logger = new Logger('AthenaAgent');
