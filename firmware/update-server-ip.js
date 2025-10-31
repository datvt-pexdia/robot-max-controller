#!/usr/bin/env node
/**
 * Script tự động cập nhật IP server trong Config.h
 * Sử dụng: node update-server-ip.js [ip-address]
 * 
 * Nếu không truyền IP, script sẽ tự động detect IP của máy
 */

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');
const os = require('os');

const CONFIG_FILE = path.join(__dirname, 'main', 'Config.h');

function getLocalIP() {
  const interfaces = os.networkInterfaces();
  
  // Tìm IPv4 address không phải loopback
  for (const name of Object.keys(interfaces)) {
    for (const iface of interfaces[name]) {
      // Skip loopback và non-IPv4
      if (iface.family !== 'IPv4' || iface.internal) continue;
      
      // Ưu tiên địa chỉ 192.168.x.x (mạng LAN thông thường)
      if (iface.address.startsWith('192.168.')) {
        return iface.address;
      }
    }
  }
  
  // Fallback: lấy địa chỉ IPv4 đầu tiên không phải loopback
  for (const name of Object.keys(interfaces)) {
    for (const iface of interfaces[name]) {
      if (iface.family !== 'IPv4' || iface.internal) continue;
      return iface.address;
    }
  }
  
  return null;
}

function updateConfigFile(newIP) {
  if (!fs.existsSync(CONFIG_FILE)) {
    console.error(`❌ File không tồn tại: ${CONFIG_FILE}`);
    process.exit(1);
  }
  
  let content = fs.readFileSync(CONFIG_FILE, 'utf8');
  
  // Tìm dòng WS_HOST
  const regex = /static const char\* WS_HOST = "([^"]+)";/;
  const match = content.match(regex);
  
  if (!match) {
    console.error('❌ Không tìm thấy WS_HOST trong Config.h');
    process.exit(1);
  }
  
  const oldIP = match[1];
  
  if (oldIP === newIP) {
    console.log(`✓ IP đã đúng: ${newIP}`);
    return;
  }
  
  // Thay thế IP
  content = content.replace(regex, `static const char* WS_HOST = "${newIP}";`);
  
  fs.writeFileSync(CONFIG_FILE, content, 'utf8');
  
  console.log(`✓ Đã cập nhật Config.h:`);
  console.log(`  Cũ: ${oldIP}`);
  console.log(`  Mới: ${newIP}`);
}

function main() {
  const args = process.argv.slice(2);
  let targetIP;
  
  if (args.length > 0) {
    // IP được truyền qua argument
    targetIP = args[0];
    
    // Validate IP format
    const ipRegex = /^(\d{1,3}\.){3}\d{1,3}$/;
    if (!ipRegex.test(targetIP)) {
      console.error(`❌ IP không hợp lệ: ${targetIP}`);
      process.exit(1);
    }
  } else {
    // Auto-detect IP
    targetIP = getLocalIP();
    
    if (!targetIP) {
      console.error('❌ Không thể tự động detect IP. Vui lòng truyền IP thủ công:');
      console.error('   node update-server-ip.js 192.168.1.5');
      process.exit(1);
    }
    
    console.log(`📡 Đã detect IP: ${targetIP}`);
  }
  
  updateConfigFile(targetIP);
  
  console.log('\n📝 Nhớ upload firmware mới lên ESP8266!');
}

main();

