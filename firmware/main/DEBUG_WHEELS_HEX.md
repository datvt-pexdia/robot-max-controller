# Debug Wheels - Hex Codes Explained

## Mã Hex được gửi đến Motor

### Format gói tin:
```
[FF] [Byte1] [Byte2] [FE] [FE] [Checksum]
      RIGHT   LEFT
```

## Speed Bytes (0x40 - 0x4F)

| Hex  | Dec | Ý nghĩa | RPM |
|------|-----|---------|-----|
| 0x40 | 64  | STOP    | 0   |
| 0x41 | 65  | (không dùng) | - |
| 0x42 | 66  | Min speed | ~6 |
| 0x43 | 67  | | ~7 |
| ... | ... | ... | ... |
| 0x4F | 79  | Max speed | ~21 |

### Mapping từ Percent (0-100) sang Hex:
```cpp
// Percent 0   → 0x40 (STOP)
// Percent 1   → 0x42 (min)
// Percent 50  → 0x49
// Percent 100 → 0x4F (max)
```

## Direction Bytes

### Left Motor:
| Hex  | Ý nghĩa | Hướng quay |
|------|---------|------------|
| 0x00 | STOP | Không quay |
| 0x24 | REVERSE | Clockwise (lùi) |
| 0x34 | FORWARD | Anti-clockwise (tiến) |

### Right Motor:
| Hex  | Ý nghĩa | Hướng quay |
|------|---------|------------|
| 0x00 | STOP | Không quay |
| 0x24 | FORWARD | Clockwise (tiến) |
| 0x34 | REVERSE | Anti-clockwise (lùi) |

## Ví dụ Log Output

### 1. Chạy tiến (L=50, R=50):
```
[WHEELS] startTask L=50 R=50 -> speed L=0x49 R=0x49 dir L=0x34 R=0x24
[WHEELS] setMotorSpeed: L=0x49 R=0x49
[WHEELS] sendRL: [FF 49 49 FE FE] → Right=0x49 Left=0x49
[WHEELS] sendRL: [FF 24 34 FE FE] → Right=0x24 Left=0x34
```

**Giải thích:**
- Speed: 50% → 0x49 (cả hai bánh)
- Direction: Left=0x34 (forward), Right=0x24 (forward)
- Gửi speed trước, sau đó gửi direction

### 2. Rẽ trái (L=30, R=70):
```
[WHEELS] tryUpdate: L=30 R=70 -> speed L=0x46 R=0x4B dir L=0x34 R=0x24
[WHEELS] tryUpdate: SPEED CHANGED L=0x49->0x46 R=0x49->0x4B
[WHEELS] setMotorSpeed: L=0x46 R=0x4B
[WHEELS] sendRL: [FF 4B 46 FE FE] → Right=0x4B Left=0x46
[WHEELS] sendRL: [FF 24 34 FE FE] → Right=0x24 Left=0x34
```

**Giải thích:**
- Speed: Left=30%→0x46, Right=70%→0x4B
- Direction: vẫn forward (0x34, 0x24)
- Right nhanh hơn Left → rẽ trái

### 3. Lùi (L=-50, R=-50):
```
[WHEELS] tryUpdate: L=-50 R=-50 -> speed L=0x49 R=0x49 dir L=0x24 R=0x34
[WHEELS] tryUpdate: SPEED CHANGED L=0x46->0x49 R=0x4B->0x49
[WHEELS] setMotorSpeed: L=0x49 R=0x49
[WHEELS] sendRL: [FF 49 49 FE FE] → Right=0x49 Left=0x49
[WHEELS] sendRL: [FF 34 24 FE FE] → Right=0x34 Left=0x24
```

**Giải thích:**
- Speed: abs(-50)=50% → 0x49 (cả hai bánh)
- Direction: Left=0x24 (reverse), Right=0x34 (reverse)
- **CHÚ Ý**: Direction đảo ngược so với tiến!

### 4. Xoay trái tại chỗ (L=-50, R=50):
```
[WHEELS] tryUpdate: L=-50 R=50 -> speed L=0x49 R=0x49 dir L=0x24 R=0x24
[WHEELS] setMotorSpeed: L=0x49 R=0x49
[WHEELS] sendRL: [FF 49 49 FE FE] → Right=0x49 Left=0x49
[WHEELS] sendRL: [FF 24 24 FE FE] → Right=0x24 Left=0x24
```

**Giải thích:**
- Speed: cả hai 50% → 0x49
- Direction: Left=0x24 (reverse), Right=0x24 (forward)
- Left lùi, Right tiến → xoay trái

## Vấn đề có thể xảy ra

### ❌ Vấn đề 1: Bánh không chạy với số âm

**Triệu chứng:**
```
[WHEELS] tryUpdate: L=-40 R=40 -> speed L=0x47 R=0x47 dir L=0x24 R=0x24
[WHEELS] sendRL: [FF 24 24 FE FE] → Right=0x24 Left=0x24
```

**Nguyên nhân:**
- Cả hai direction đều 0x24
- Right=0x24 là FORWARD (đúng)
- Left=0x24 là REVERSE (đúng)
- **Nhưng**: Nếu Left không chạy, có thể:
  1. Speed byte chưa được gửi
  2. Motor chưa initialized
  3. Wiring sai

**Kiểm tra:**
1. Xem có log `setMotorSpeed` không?
2. Xem có log `sendRL: [FF 47 47 FE FE]` (speed) không?
3. Kiểm tra `motorInitialized = true`

### ❌ Vấn đề 2: Speed không được gửi

**Triệu chứng:**
```
[WHEELS] tryUpdate: L=-40 R=40 -> speed L=0x47 R=0x47 dir L=0x24 R=0x24
[WHEELS] sendRL: [FF 24 24 FE FE] → Right=0x24 Left=0x24
// Không có log setMotorSpeed!
```

**Nguyên nhân:**
- Trong continuous mode, speed chỉ gửi khi:
  1. `!wasMoving && isMoving` (bắt đầu di chuyển)
  2. `speedChanged && isMoving` (thay đổi tốc độ)
- Nếu đã đang chạy và không thay đổi tốc độ → không gửi speed

**Giải pháp:**
- Luôn gửi speed khi thay đổi direction (số âm/dương)

### ❌ Vấn đề 3: Direction byte sai

**Triệu chứng:**
```
// Muốn: Left lùi, Right tiến
[WHEELS] tryUpdate: L=-50 R=50 -> speed L=0x49 R=0x49 dir L=0x34 R=0x34
// Sai! Cả hai đều 0x34
```

**Nguyên nhân:**
- Logic `leftDirFromSigned()` hoặc `rightDirFromSigned()` sai

**Kiểm tra:**
```cpp
static inline uint8_t leftDirFromSigned(int v) {
  if (v == 0) return DIR_STOP;     // 0x00
  return (v > 0) ? DIR_L_FWD : DIR_L_REV;  // 0x34 : 0x24
}

static inline uint8_t rightDirFromSigned(int v) {
  if (v == 0) return DIR_STOP;     // 0x00
  return (v > 0) ? DIR_R_FWD : DIR_R_REV;  // 0x24 : 0x34
}
```

## Cách Debug

### 1. Kiểm tra Speed Mapping
```
Input: L=-40, R=40
Expected speed: abs(-40)=40% → 0x47, abs(40)=40% → 0x47
Check log: speed L=0x47 R=0x47 ✓
```

### 2. Kiểm tra Direction Mapping
```
Input: L=-40 (âm), R=40 (dương)
Expected dir: Left=0x24 (reverse), Right=0x24 (forward)
Check log: dir L=0x24 R=0x24 ✓
```

### 3. Kiểm tra Speed có được gửi không
```
Check log: setMotorSpeed: L=0x47 R=0x47 ✓
Check log: sendRL: [FF 47 47 FE FE] ✓
```

### 4. Kiểm tra Direction có được gửi không
```
Check log: sendRL: [FF 24 24 FE FE] ✓
```

### 5. Kiểm tra thứ tự gửi
```
Đúng:
1. sendRL: [FF 47 47 FE FE] (speed)
2. sendRL: [FF 24 24 FE FE] (direction)

Sai:
1. sendRL: [FF 24 24 FE FE] (direction)
// Thiếu speed!
```

## Bảng tra cứu nhanh

### Robot di chuyển tiến (L=50, R=50):
```
Speed: [FF 49 49 FE FE]
Dir:   [FF 24 34 FE FE]
       Right↑ Left↑
       0x24  0x34
```

### Robot di chuyển lùi (L=-50, R=-50):
```
Speed: [FF 49 49 FE FE]
Dir:   [FF 34 24 FE FE]
       Right↑ Left↑
       0x34  0x24
```

### Robot rẽ trái (L=30, R=70):
```
Speed: [FF 4B 46 FE FE]
       Right↑ Left↑
       70%   30%
Dir:   [FF 24 34 FE FE]
```

### Robot xoay trái (L=-50, R=50):
```
Speed: [FF 49 49 FE FE]
Dir:   [FF 24 24 FE FE]
       Right↑ Left↑
       FWD   REV
```

## Lưu ý quan trọng

1. **Wiring**: Byte[1]=RIGHT, Byte[2]=LEFT
2. **Speed trước, Direction sau**: Luôn gửi speed trước direction
3. **Continuous mode**: Direction được gửi liên tục mỗi 10ms
4. **Speed chỉ gửi khi cần**: Khi bắt đầu hoặc thay đổi tốc độ
5. **Số âm**: abs() cho speed, direction dựa trên dấu

## Test Commands

### Test với curl:
```bash
# Tiến
curl -X POST http://localhost:8080/robot/tasks/enqueue \
  -H "Content-Type: application/json" \
  -d '{"tasks":[{"taskId":"t1","device":"wheels","type":"drive","left":50,"right":50,"durationMs":100}]}'

# Lùi
curl -X POST http://localhost:8080/robot/tasks/enqueue \
  -H "Content-Type: application/json" \
  -d '{"tasks":[{"taskId":"t2","device":"wheels","type":"drive","left":-50,"right":-50,"durationMs":100}]}'

# Xoay trái
curl -X POST http://localhost:8080/robot/tasks/enqueue \
  -H "Content-Type: application/json" \
  -d '{"tasks":[{"taskId":"t3","device":"wheels","type":"drive","left":-50,"right":50,"durationMs":100}]}'
```

### Xem logs trên Serial Monitor:
```
[WHEELS] tryUpdate: L=-50 R=50 -> speed L=0x49 R=0x49 dir L=0x24 R=0x24 wasMoving=1 isMoving=1 speedChanged=0
[WHEELS] sendRL: [FF 24 24 FE FE] → Right=0x24 Left=0x24
```

Nếu thấy log này, nghĩa là:
- ✅ Speed: 0x49 (50%)
- ✅ Direction: Left=0x24 (reverse), Right=0x24 (forward)
- ✅ Đang gửi direction liên tục
- ❓ Speed có được gửi không? → Kiểm tra có log `setMotorSpeed` không

