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

// Đăng ký DriveRelay để xử lý browser connections
wsHub.setBrowserConnectionHandler((socket) => {
  driveRelay.handleConnection(socket);
});

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
