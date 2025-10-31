export const HTTP_PORT = parseInt(process.env.PORT || '8080', 10);

// WebSocket (ws) tuning for low-latency LAN control
export const WS_PATH = '/robot';
export const WS_HEARTBEAT_MS = 15000;      // server pings every 15s
export const WS_LIVENESS_GRACE_MS = 30000; // if no pong within 30s ⇒ drop

// keep payloads tight; we're sending tiny control frames
export const WS_MAX_PAYLOAD = 16 * 1024;   // 16KB upper bound
export const WS_PERMESSAGE_DEFLATE = false;// disable compression for low CPU/latency

// Queue & safety
export const MAX_QUEUE_PER_DEVICE = 32;

export const JWT_DISABLED = true;
export const LOCAL_SIMULATION = false;
