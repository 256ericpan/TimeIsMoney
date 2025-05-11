#include <TFT_eSPI.h>
#include <rpcWiFi.h>
#include <WiFiUdp.h>
#include <time.h>
#define ODO_ADDR 0

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

// NTP相关变量
WiFiUDP ntpUDP;
const char* ntpServer = "ntp.aliyun.com";
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];
unsigned int localPort = 2390;
String netTime = "";
unsigned long lastNtpSync = 0;

// 新增缓存变量减少闪烁
String lastDate = "";
String lastClock = "";

// 新增本地时间戳变量
unsigned long localEpoch = 0;
unsigned long localEpochSyncMillis = 0;

// 6个汉字的32x32点阵数组（请用工具生成并补全每个128字节）
static const unsigned char font32_shi[128]  = { /* 时字点阵 */ };
static const unsigned char font32_jian[128] = { /* 间字点阵 */ };
static const unsigned char font32_jiu[128]  = { /* 就字点阵 */ };
static const unsigned char font32_shi2[128] = { /* 是字点阵 */ };
static const unsigned char font32_jin[128]  = { /* 金字点阵 */ };
static const unsigned char font32_qian[128] = { /* 钱字点阵 */ };

void drawChinese32x32(int x, int y, const unsigned char* font, uint16_t color = TFT_YELLOW) {
  for (int row = 0; row < 32; row++) {
    for (int col = 0; col < 32; col++) {
      int byteIndex = row * 4 + col / 8;
      int bitIndex = 7 - (col % 8);
      if (font[byteIndex] & (1 << bitIndex)) {
        tft.drawPixel(x + col, y + row, color);
      }
    }
  }
}

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

unsigned long getNtpTime() {
  IPAddress ntpIP;
  WiFi.hostByName(ntpServer, ntpIP);
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;
  ntpUDP.beginPacket(ntpIP, 123);
  ntpUDP.write(packetBuffer, NTP_PACKET_SIZE);
  ntpUDP.endPacket();
  delay(100);
  int cb = ntpUDP.parsePacket();
  if (!cb) return 0;
  ntpUDP.read(packetBuffer, NTP_PACKET_SIZE);
  unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
  unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
  unsigned long secsSince1900 = highWord << 16 | lowWord;
  const unsigned long seventyYears = 2208988800UL;
  return secsSince1900 - seventyYears + 8 * 3600; // 东八区
}

void syncNtpTime() {
  unsigned long epoch = getNtpTime();
  if (epoch > 100000) {
    localEpoch = epoch;
    localEpochSyncMillis = millis();
    time_t raw = epoch;
    struct tm * ti = localtime(&raw);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ti);
    netTime = String(buf);
  } else {
    netTime = "NTP Err";
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

  WiFi.begin("breeze845", "ffffffff");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected!");
  ntpUDP.begin(localPort);
  syncNtpTime();
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
  int x = 8, y = 2;
  drawChinese32x32(x + 0,   y, font32_shi);
  drawChinese32x32(x + 36,  y, font32_jian);
  drawChinese32x32(x + 72,  y, font32_jiu);
  drawChinese32x32(x + 108, y, font32_shi2);
  drawChinese32x32(x + 144, y, font32_jin);
  drawChinese32x32(x + 180, y, font32_qian);

  // 显示IP地址（如有变化才刷新）
  static String lastIp = "";
  String curIp = WiFi.localIP().toString();
  if (curIp != lastIp) {
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.fillRect(0, 54, 320, 24, TFT_BLACK);
    tft.setCursor(8, 54);
    tft.printf("IP: %s", curIp.c_str());
    lastIp = curIp;
  }

  // 用本地计时推算当前时间
  String dateStr = "";
  String clockStr = "";
  if (localEpoch > 100000) {
    unsigned long nowEpoch = localEpoch + (millis() - localEpochSyncMillis) / 1000;
    time_t raw = nowEpoch;
    struct tm * ti = localtime(&raw);
    char dateBuf[16], timeBuf[16];
    strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", ti);
    strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", ti);
    dateStr = String(dateBuf);
    clockStr = String(timeBuf);
  } else {
    dateStr = netTime;
    clockStr = "";
  }

  // 日期小号字体，时间大号字体，只有变化时刷新
  static String lastDate = "";
  static String lastClock = "";
  if (dateStr != lastDate) {
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.fillRect(0, 78, 320, 24, TFT_BLACK);
    tft.setCursor(8, 78);
    tft.printf("Time: %s", dateStr.c_str());
    lastDate = dateStr;
  }
  if (clockStr != lastClock) {
    tft.setTextSize(4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.fillRect(0, 102, 320, 40, TFT_BLACK);
    tft.setCursor(8, 102);
    tft.printf("%s", clockStr.c_str());
    lastClock = clockStr;
  }

  String uptime = getUptimeString();
  static String lastUptimeStr = "";
  // 只刷新变动的字符
  if (uptime != lastUptimeStr) {
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    for (size_t i = 0; i < uptime.length(); ++i) {
      if (i >= lastUptimeStr.length() || uptime[i] != lastUptimeStr[i]) {
        // 只重绘变动的字符
        tft.fillRect(8 + i * 16, 26, 16, 24, TFT_BLACK);
        tft.setCursor(8 + i * 16, 26);
        tft.print(uptime[i]);
      }
    }
    lastUptimeStr = uptime;
  }

  // 横线分割
  tft.drawFastHLine(0, 54, 320, TFT_DARKGREY);
  tft.drawFastHLine(0, 78, 320, TFT_DARKGREY);
  tft.drawFastHLine(0, 102, 320, TFT_DARKGREY);

  // 金额大字居中（整数，无小数点，符号用ASCII的¥）
  if ((int)totalAmount != (int)lastTotalAmount) {
    tft.setTextSize(6);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.fillRect(0, 120, 320, 100, TFT_BLACK);
    char moneyStr[16];
    sprintf(moneyStr, "\xA5%d", (int)totalAmount); // ASCII的¥
    int len = strlen(moneyStr);
    int w = len * 18;
    int x = (320 - w) / 2;
    if (x < 0) x = 0;
    tft.setCursor(x, 140);
    tft.print(moneyStr);
    lastTotalAmount = (int)totalAmount;
  }

  // 横线分割
  tft.drawFastHLine(0, 220, 320, TFT_DARKGREY);

  // Cost/h（底部左）
  if (costPerHour != lastCostPerHour) {
    tft.setTextSize(2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.fillRect(0, 230, 160, 32, TFT_BLACK);
    tft.setCursor(8, 240);
    tft.printf("Cost/h: %d", costPerHour);
    lastCostPerHour = costPerHour;
  }

  // ODO（底部独占一行，靠左）
  if (odoAmount != lastOdoAmount) {
    tft.setTextSize(2);
    tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
    tft.fillRect(0, 270, 320, 32, TFT_BLACK);
    tft.setCursor(8, 280);
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