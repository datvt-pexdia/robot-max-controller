import cors from 'cors';
import express from 'express';
import http from 'http';
import { buildRouter } from './api';
import { HTTP_PORT, WS_PATH } from './config';
import { exampleTaskBatch } from './demoData';
import { httpLog, wsLog } from './logger';
import { WsHub } from './wsHub';

const app = express();
app.use(cors());
app.use(express.json());
app.get('/', (_req, res) => {
  res.json({ status: 'ok', message: 'robot-max-controller server' });
});

const server = http.createServer(app);
const wsHub = new WsHub(server);

app.use(buildRouter(wsHub));

server.listen(HTTP_PORT, '0.0.0.0', () => {
  httpLog(`HTTP server listening on http://0.0.0.0:${HTTP_PORT}`);
  wsLog(`WebSocket endpoint ws://localhost:${HTTP_PORT}${WS_PATH}`);
  httpLog('Example curl commands:');
  console.log(`curl -X POST http://localhost:${HTTP_PORT}/robot/tasks/replace \\\n  -H "Content-Type: application/json" \\\n  -d '${JSON.stringify({ tasks: exampleTaskBatch() }, null, 2)}'`);
  console.log(`curl http://localhost:${HTTP_PORT}/robot/status`);
});
