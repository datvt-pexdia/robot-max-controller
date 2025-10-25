import cors from 'cors';
import express from 'express';
import http from 'http';
import path from 'path';
import { buildRouter } from './api';
import { HTTP_PORT, WS_PATH } from './config';
import { DeviceAdapter } from './deviceAdapter';
import { DriveRelay } from './driveRelay';
import { httpLog, wsLog } from './logger';
import { WsHub } from './wsHub';

const app = express();
app.use(cors());
app.use(express.json());

// Serve static files from public directory
app.use(express.static(path.join(__dirname, '../public')));

const server = http.createServer(app);

// WsHub for ESP firmware (handles WebSocket connections for devices)
const wsHub = new WsHub(server);

// DriveRelay for UI control (piggybacks on same WS connection)
const driveRelay = new DriveRelay();

// DeviceAdapter bridges DriveRelay intents to WsHub legacy format
const deviceAdapter = new DeviceAdapter(wsHub);

// Connect DriveRelay output to DeviceAdapter
driveRelay.onDriveIntent((intent) => {
  deviceAdapter.handleDriveIntent(intent);
});

// Hook into WsHub to add DriveRelay handling
const originalWsHubHandleConnection = (wsHub as any).handleConnection.bind(wsHub);
(wsHub as any).handleConnection = (socket: any, req: any) => {
  // Check if this is a UI connection (browser) or ESP connection
  const userAgent = req?.headers?.['user-agent'] || '';
  const isBrowser = userAgent.includes('Mozilla') || userAgent.includes('Chrome') || userAgent.includes('Safari');
  
  wsLog(`Connection from ${req?.socket?.remoteAddress}, User-Agent: ${userAgent.substring(0, 50)}...`);
  wsLog(`Detected as: ${isBrowser ? 'BROWSER (UI)' : 'ESP (Device)'}`);
  
  if (isBrowser) {
    // Let DriveRelay handle UI connections
    driveRelay.handleConnection(socket);
  } else {
    // Let WsHub handle ESP connections (backward compat)
    originalWsHubHandleConnection(socket, req);
  }
};

// API routes
app.use(buildRouter(wsHub));

// Fallback route for SPA
app.get('/', (_req, res) => {
  res.sendFile(path.join(__dirname, '../public/index.html'));
});

server.listen(HTTP_PORT, '0.0.0.0', () => {
  httpLog(`HTTP server listening on http://0.0.0.0:${HTTP_PORT}`);
  wsLog(`WebSocket endpoint ws://localhost:${HTTP_PORT}${WS_PATH}`);
  httpLog('Tank Drive Controller ready');
});
