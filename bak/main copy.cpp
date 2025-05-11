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
  // 1. 第一行：开机时间（Uptime，按字符刷新）
  String uptime = getUptimeString();
  static String lastUptimeStr = "";
  int uptimeX = 8;
  int uptimeY = 0;
  int uptimeW = 16;
  String uptimeLabel = "Uptime: ";
  static String lastUptimeLabel = "Uptime: ";
  // 先刷新前缀
  for (size_t i = 0; i < uptimeLabel.length(); ++i) {
    if (i >= lastUptimeLabel.length() || uptimeLabel[i] != lastUptimeLabel[i]) {
      tft.setTextSize(2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.fillRect(uptimeX + i * uptimeW, uptimeY, uptimeW, 24, TFT_BLACK);
      tft.setCursor(uptimeX + i * uptimeW, uptimeY);
      tft.print(uptimeLabel[i]);
    }
  }
  lastUptimeLabel = uptimeLabel;
  // 再刷新uptime内容
  for (size_t i = 0; i < uptime.length(); ++i) {
    if (i >= lastUptimeStr.length() || uptime[i] != lastUptimeStr[i]) {
      tft.setTextSize(2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.fillRect(uptimeX + (uptimeLabel.length() + i) * uptimeW, uptimeY, uptimeW, 24, TFT_BLACK);
      tft.setCursor(uptimeX + (uptimeLabel.length() + i) * uptimeW, uptimeY);
      tft.print(uptime[i]);
    }
  }
  for (size_t i = uptime.length(); i < lastUptimeStr.length(); ++i) {
    tft.fillRect(uptimeX + (uptimeLabel.length() + i) * uptimeW, uptimeY, uptimeW, 24, TFT_BLACK);
  }
  lastUptimeStr = uptime;

  // 2. 第二行：金额（"Amount:"小号，金额数字大号，均只刷新变化字符）
  static String lastAmountPrefix = "Amount:";
  static String lastAmountNum = "";
  String amountPrefix = "Amount:";
  String amountNum = String((int)totalAmount);
  int prefixX = 8;
  int prefixY = 24;
  int prefixW = 16; // 小号字体宽度
  int numX = prefixX + amountPrefix.length() * prefixW + 8;
  int numY = 24;
  int numW = 24; // 大号字体宽度
  // 刷新"Amount:"前缀
  for (size_t i = 0; i < amountPrefix.length(); ++i) {
    if (i >= lastAmountPrefix.length() || amountPrefix[i] != lastAmountPrefix[i]) {
      tft.setTextSize(2);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.fillRect(prefixX + i * prefixW, prefixY, prefixW, 32, TFT_BLACK);
      tft.setCursor(prefixX + i * prefixW, prefixY);
      tft.print(amountPrefix[i]);
    }
  }
  for (size_t i = amountPrefix.length(); i < lastAmountPrefix.length(); ++i) {
    tft.fillRect(prefixX + i * prefixW, prefixY, prefixW, 32, TFT_BLACK);
  }
  lastAmountPrefix = amountPrefix;
  // 刷新金额数字
  for (size_t i = 0; i < amountNum.length(); ++i) {
    if (i >= lastAmountNum.length() || amountNum[i] != lastAmountNum[i]) {
      tft.setTextSize(4);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.fillRect(numX + i * numW, numY, numW, 40, TFT_BLACK);
      tft.setCursor(numX + i * numW, numY);
      tft.print(amountNum[i]);
    }
  }
  for (size_t i = amountNum.length(); i < lastAmountNum.length(); ++i) {
    tft.fillRect(numX + i * numW, numY, numW, 40, TFT_BLACK);
  }
  lastAmountNum = amountNum;

  // 3. 第三行：日期（小号字体）
  static String lastDate = "";
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
  if (dateStr != lastDate) {
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.fillRect(0, 64, 320, 24, TFT_BLACK);
    tft.setCursor(8, 64);
    tft.print(dateStr);
    lastDate = dateStr;
  }

  // 4. 第四行：现在时间（大号字体3，按字符刷新，与uptime同步）
  static String lastClock = "";
  int clockX = 8;
  int clockY = 88;
  int clockW = 24; // 字符宽度
  for (size_t i = 0; i < clockStr.length(); ++i) {
    if (i >= lastClock.length() || clockStr[i] != lastClock[i]) {
      tft.setTextSize(3);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.fillRect(clockX + i * clockW, clockY, clockW, 32, TFT_BLACK);
      tft.setCursor(clockX + i * clockW, clockY);
      tft.print(clockStr[i]);
    }
  }
  for (size_t i = clockStr.length(); i < lastClock.length(); ++i) {
    tft.fillRect(clockX + i * clockW, clockY, clockW, 32, TFT_BLACK);
  }
  lastClock = clockStr;

  // 5. 第五行：Cost/h
  if (costPerHour != lastCostPerHour) {
    tft.setTextSize(2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.fillRect(0, 120, 320, 24, TFT_BLACK);
    tft.setCursor(8, 120);
    tft.printf("Cost/h: %d", costPerHour);
    lastCostPerHour = costPerHour;
  }

  // 6. 第六行：ODO
  if (odoAmount != lastOdoAmount) {
    tft.setTextSize(2);
    tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
    tft.fillRect(0, 144, 320, 24, TFT_BLACK);
    tft.setCursor(8, 144);
    tft.printf("ODO: %.2f", odoAmount);
    lastOdoAmount = odoAmount;
  }

  // 7. 最后一行：IP地址
  static String lastIp = "";
  String curIp = WiFi.localIP().toString();
  if (curIp != lastIp) {
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.fillRect(0, 168, 320, 24, TFT_BLACK);
    tft.setCursor(8, 168);
    tft.printf("IP: %s", curIp.c_str());
    lastIp = curIp;
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