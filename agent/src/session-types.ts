/**
 * Session Management Types
 *
 * Defines types for persistent session storage and conversation memory.
 */

export interface Message {
  role: 'user' | 'assistant' | 'system';
  content: string;
  timestamp: number;
  tokens?: {
    input?: number;
    output?: number;
  };
  cost?: number;
}

export interface SessionMetadata {
  sessionId: string;
  title: string;
  createdAt: number;
  lastUsedAt: number;
  messageCount: number;
  totalTokens: {
    input: number;
    output: number;
  };
  totalCost: number;
  model: string;
  tags?: string[];
  claudeSessionId?: string; // Claude SDK session ID for conversation continuity
  firstMessage?: string; // First user message for preview/search only
}

export interface Session {
  metadata: SessionMetadata;
  // messages array removed - Claude SDK manages conversation history internally
  // Only metadata is stored locally for session management and UI display
}

export interface SessionSummary {
  sessionId: string;
  title: string;
  createdAt: number;
  lastUsedAt: number;
  messageCount: number;
  preview?: string; // First user message or summary
}

export interface SessionListOptions {
  limit?: number;
  offset?: number;
  sortBy?: 'createdAt' | 'lastUsedAt' | 'messageCount';
  sortOrder?: 'asc' | 'desc';
  tags?: string[];
}

export interface SessionStore {
  // Session CRUD operations
  createSession(metadata: Omit<SessionMetadata, 'sessionId'>): Promise<Session>;
  getSession(sessionId: string): Promise<Session | null>;
  updateSession(session: Session): Promise<void>;
  deleteSession(sessionId: string): Promise<void>;

  // Session listing and search
  listSessions(options?: SessionListOptions): Promise<SessionSummary[]>;
  searchSessions(query: string): Promise<SessionSummary[]>;

  // Message operations
  addMessage(sessionId: string, message: Message): Promise<void>;
  getMessages(sessionId: string, limit?: number, offset?: number): Promise<Message[]>;

  // Metadata updates
  updateMetadata(sessionId: string, updates: Partial<SessionMetadata>): Promise<void>;

  // Cleanup
  pruneOldSessions(olderThanDays: number): Promise<number>;
  getStorageSize(): Promise<number>;
}
