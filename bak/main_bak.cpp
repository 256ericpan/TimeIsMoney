#include <TFT_eSPI.h>

TFT_eSPI tft;

unsigned long lastReadTime = 0;
unsigned long lastPersonTime = 0;
int peopleCount = 0;
int costPerHour = 100;
float totalAmount = 0;
float odoAmount = 0;
bool resetFlag = false;
int displayMode = 0; // 0:金额 1:时间 2:人数

String lastUptime = "";
float lastTotalAmount = -1;
int lastCostPerHour = -1;
float lastOdoAmount = -1;

// 8x16点阵字模（示例，建议用工具生成更美观的点阵）
const uint8_t font_shi[16]  = {0x00,0x10,0x10,0x10,0xFE,0x10,0x10,0x10,0x10,0xFF,0x10,0x10,0x10,0x10,0x10,0x00};
const uint8_t font_jian[16] = {0x00,0x20,0x20,0x20,0xFE,0x20,0x20,0x20,0x20,0xFF,0x20,0x20,0x20,0x20,0x20,0x00};
const uint8_t font_jiu[16]  = {0x00,0x08,0x08,0x08,0xFE,0x08,0x08,0x08,0x08,0xFF,0x08,0x08,0x08,0x08,0x08,0x00};
const uint8_t font_shi2[16] = {0x00,0x10,0x10,0x10,0xFE,0x10,0x10,0x10,0x10,0xFF,0x10,0x10,0x10,0x10,0x10,0x00};
const uint8_t font_jin[16]  = {0x00,0x04,0x04,0x04,0xFE,0x04,0x04,0x04,0x04,0xFF,0x04,0x04,0x04,0x04,0x04,0x00};
const uint8_t font_qian[16] = {0x00,0x40,0x40,0x40,0xFE,0x40,0x40,0x40,0x40,0xFF,0x40,0x40,0x40,0x40,0x40,0x00};

void drawChinese8x16(int x, int y, const uint8_t* font, uint16_t color = TFT_YELLOW) {
  for (int row = 0; row < 16; row++) {
    uint8_t data = font[row];
    for (int col = 0; col < 8; col++) {
      if (data & (0x80 >> col)) {
        tft.drawPixel(x + col, y + row, color);
      }
    }
  }
}

void setup() {
  tft.begin();
  tft.setRotation(0); // 顺时针90度，USB在左
  Serial1.begin(115200);
  pinMode(WIO_KEY_A, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);
  tft.fillScreen(TFT_BLACK);
}

String getUptimeString() {
  unsigned long seconds = millis() / 1000;
  unsigned long hours = seconds / 3600;
  unsigned long minutes = (seconds % 3600) / 60;
  unsigned long secs = seconds % 60;
  char buf[16];
  sprintf(buf, "%02lu:%02lu:%02lu", hours, minutes, secs);
  return String(buf);
}

void drawUI() {
  // 先画点阵字，避免被后续UI覆盖
  int x = 8, y = 2;
  drawChinese8x16(x + 0,  y, font_shi);
  drawChinese8x16(x + 12, y, font_jian);
  drawChinese8x16(x + 24, y, font_jiu);
  drawChinese8x16(x + 36, y, font_shi2);
  drawChinese8x16(x + 48, y, font_jin);
  drawChinese8x16(x + 60, y, font_qian);

  String uptime = getUptimeString();

  // Uptime（总时间）- 顶部小字
  if (uptime != lastUptime) {
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.fillRect(0, 20, 320, 28, TFT_BLACK); // 下移，避免覆盖点阵
    tft.setCursor(8, 26);
    tft.printf("Uptime: %s", uptime.c_str());
    lastUptime = uptime;
  }

  // 横线分割
  tft.drawFastHLine(0, 54, 320, TFT_DARKGREY);

  // 金额大字居中（整数，无小数点，符号用ASCII的¥）
  if ((int)totalAmount != (int)lastTotalAmount) {
    tft.setTextSize(6);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.fillRect(0, 60, 320, 100, TFT_BLACK);
    char moneyStr[16];
    sprintf(moneyStr, "\xA5%d", (int)totalAmount); // ASCII的¥
    int len = strlen(moneyStr);
    int w = len * 18;
    int x = (320 - w) / 2;
    if (x < 0) x = 0;
    tft.setCursor(x, 80);
    tft.print(moneyStr);
    lastTotalAmount = (int)totalAmount;
  }

  // 横线分割
  tft.drawFastHLine(0, 170, 320, TFT_DARKGREY);

  // Cost/h（底部左）
  if (costPerHour != lastCostPerHour) {
    tft.setTextSize(2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.fillRect(0, 180, 160, 32, TFT_BLACK);
    tft.setCursor(8, 190);
    tft.printf("Cost/h: %d", costPerHour);
    lastCostPerHour = costPerHour;
  }

  // ODO（底部独占一行，靠左）
  if (odoAmount != lastOdoAmount) {
    tft.setTextSize(2);
    tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
    tft.fillRect(0, 220, 320, 32, TFT_BLACK);
    tft.setCursor(8, 230);
    tft.printf("ODO: %.2f", odoAmount);
    lastOdoAmount = odoAmount;
  }
}

void readPeopleCount() {
  if (Serial1.available()) {
    String data = Serial1.readStringUntil('\n');
    peopleCount = data.toInt();
    if (peopleCount == 0) peopleCount = 1; // 防止收到0时也默认1
  } else {
    peopleCount = 1; // 没有串口数据时默认为1
  }
}

void handleButtons() {
  if (digitalRead(WIO_KEY_A) == LOW) { // 上
    costPerHour += 10;
    delay(200);
  }
  if (digitalRead(WIO_KEY_B) == LOW) { // 下
    costPerHour = max(10, costPerHour - 10);
    delay(200);
  }
  if (digitalRead(WIO_KEY_C) == LOW) { // 切换显示
    displayMode = (displayMode + 1) % 3;
    delay(200);
  }
  // 复位
  if (peopleCount == 0 && resetFlag && digitalRead(WIO_KEY_A) == LOW) {
    totalAmount = 0;
    odoAmount = 0;
    resetFlag = false;
    delay(200);
  }
}

void loop() {
  unsigned long now = millis();

  // 每10秒读取一次人数并累加金额
  if (now - lastReadTime > 10000) {
    lastReadTime = now;
    readPeopleCount();
    if (peopleCount > 0) {
      lastPersonTime = now;
      resetFlag = false;
    }
    // 只在此处累加金额
    float hours = 10.0 / 3600.0; // 10秒换算小时
    float amount = peopleCount * costPerHour * hours;
    totalAmount += amount;
    odoAmount += amount;
  }

  // 无人10分钟归零
  if (peopleCount == 0 && (now - lastPersonTime > 600000) && !resetFlag) {
    totalAmount = 0;
    resetFlag = true;
  }

  // 按钮处理
  handleButtons();

  // 刷新界面
  drawUI();

  delay(100);
} 