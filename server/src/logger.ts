import util from 'util';

const LEVEL_COLORS: Record<string, string> = {
  info: '\x1b[32m',
  warn: '\x1b[33m',
  error: '\x1b[31m',
  debug: '\x1b[36m'
};

const RESET = '\x1b[0m';

function timestamp(): string {
  const now = new Date();
  return now.toISOString();
}

function format(level: string, prefix: string, message: unknown[]): string {
  const color = LEVEL_COLORS[level] ?? '';
  const rendered = message
    .map((part) => (typeof part === 'string' ? part : util.inspect(part, { depth: 6, colors: false })))
    .join(' ');
  return `${timestamp()} ${color}${prefix}${RESET} ${rendered}`;
}

export const logger = {
  info(prefix: string, ...message: unknown[]): void {
    console.log(format('info', prefix, message));
  },
  warn(prefix: string, ...message: unknown[]): void {
    console.warn(format('warn', prefix, message));
  },
  error(prefix: string, ...message: unknown[]): void {
    console.error(format('error', prefix, message));
  },
  debug(prefix: string, ...message: unknown[]): void {
    if (process.env.DEBUG_LOGS) {
      console.debug(format('debug', prefix, message));
    }
  }
};

export function httpLog(...message: unknown[]): void {
  logger.info('[HTTP]', ...message);
}

export function wsLog(...message: unknown[]): void {
  logger.info('[WS]', ...message);
}

export function taskLog(...message: unknown[]): void {
  logger.info('[TASK]', ...message);
}

export function espLog(...message: unknown[]): void {
  logger.info('[ESP]', ...message);
}
