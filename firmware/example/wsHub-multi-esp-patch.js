// Patch để hỗ trợ nhiều ESP8266 kết nối
// Thêm vào wsHub.ts để thay thế logic chỉ cho phép 1 ESP

// Thay thế dòng 144-149 trong wsHub.ts:
/*
  private handleConnection(socket: WebSocket, req: http.IncomingMessage): void {
    // Cho phép nhiều ESP kết nối (nếu cần)
    const espId = req.socket.remoteAddress || 'unknown';
    
    // Nếu đã có ESP kết nối và muốn giới hạn, có thể:
    // 1. Cho phép nhiều ESP (bỏ check này)
    // 2. Hoặc thay thế ESP cũ bằng ESP mới
    if (this.espSocket && this.espSocket.readyState === WebSocket.OPEN) {
      wsLog(`Replacing ESP connection from ${espId}`);
      this.espSocket.close(1000, 'Replaced by new connection');
    }

    this.espSocket = socket;
    wsLog('ESP connected from', espId);
    // ... rest of the method
  }
*/

// Hoặc để hỗ trợ nhiều ESP8266:
/*
  private espSockets: Map<string, WebSocket> = new Map();

  private handleConnection(socket: WebSocket, req: http.IncomingMessage): void {
    const espId = req.socket.remoteAddress || 'unknown';
    
    // Lưu ESP mới
    this.espSockets.set(espId, socket);
    wsLog(`ESP ${espId} connected`);
    
    socket.on('close', () => {
      this.espSockets.delete(espId);
      wsLog(`ESP ${espId} disconnected`);
    });
    
    // ... rest of the method
  }
*/
