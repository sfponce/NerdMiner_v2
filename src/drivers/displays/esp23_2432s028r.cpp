#include "displayDriver.h"

#if defined ESP32_2432S028R || ESP32_2432S028_2USB

#include <TFT_eSPI.h>
#include <TFT_eTouch.h>
#include "media/myFonts.h"
#include "media/Free_Fonts.h"
#include "version.h"
#include "monitor.h"
#include "OpenFontRender.h"
#include <SPI.h>
#include <WiFi.h>
#include "rotation.h"
#include "drivers/storage/nvMemory.h"
#include "drivers/storage/storage.h"

#define SCREEN_W 320
#define SCREEN_H 240

extern nvMemory nvMem;

OpenFontRender render;
TFT_eSPI tft = TFT_eSPI();
SPIClass hSPI(HSPI);
TFT_eTouch<TFT_eSPI> touch(tft, ETOUCH_CS, 0xFF, hSPI); 

extern monitor_data mMonitor;
extern pool_data pData;
extern DisplayDriver *currentDisplayDriver;
extern bool invertColors; 
extern TSettings Settings;
bool hasChangedScreen = true;

// ================= Premium Dark Palette (RGB565) =================
#define CLR_BG          0x0821  // Deep obsidian dark (#090b10)
#define CLR_PANEL       0x10A3  // Dark graphite (#121620)
#define CLR_CARD        0x14E6  // Card background (#151b27)
#define CLR_BORDER      0x296A  // Card outline (#2a3445)
#define CLR_CYAN        0x05BF  // Electric Cyan (#00b4ff)
#define CLR_ORANGE      0xFD20  // Bitcoin Gold (#f7931a)
#define CLR_MINT        0x1E4C  // Mint Emerald (#10b981)
#define CLR_ROSE        0xF987  // Alert (#f43f5e)
#define CLR_TEXT_WHITE  0xFFFF  // Crisp White (#ffffff)
#define CLR_TEXT_MUTED  0x9CF3  // Slate (#94a3b8)
#define CLR_TEXT_DARK   0x52AA  // Subtle Gray (#525e73)

void esp32_2432S028R_Init(void)
{ 
  tft.init();
  if (nvMem.loadConfig(&Settings))
  {      
    invertColors = Settings.invertColors;           
  }  
  tft.invertDisplay(invertColors);
  tft.setRotation(1);    
  tft.setSwapBytes(true);
  if (invertColors) {
    tft.writecommand(ILI9341_GAMMASET);
    tft.writedata(2);
    delay(120);
    tft.writecommand(ILI9341_GAMMASET);
    tft.writedata(1); 
  }
  hSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, ETOUCH_CS);
  touch.init();

  TFT_eTouchBase::Calibation calibation = { 233, 3785, 3731, 120, 2 };
  touch.setCalibration(calibation);

  ledcSetup(0, 5000, 8);
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, Settings.Brightness);

  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_PIN_B, OUTPUT);
  pinMode(LED_PIN_G, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(LED_PIN_B, HIGH);
  digitalWrite(LED_PIN_G, HIGH);
  pData.bestDifficulty = "0";
  pData.workersHash = "0";
  pData.workersCount = 0;
}

void esp32_2432S028R_AlternateScreenState(void)
{
  int screen_state_duty = ledcRead(0);
  if (screen_state_duty > 0) {
    ledcWrite(0, 0);
  } else {
    ledcWrite(0, Settings.Brightness);
  }
}

void esp32_2432S028R_AlternateRotation(void)
{
  tft.setRotation( flipRotation(tft.getRotation()) );
  hasChangedScreen = true;
}

static void drawStatusHeader(const char* screenTitle, const String& tempStr, const String& timeStr)
{
  tft.fillRect(0, 0, 320, 24, CLR_PANEL);
  tft.drawFastHLine(0, 24, 320, CLR_BORDER);

  // Status LED
  uint16_t ledColor = (mMonitor.NerdStatus == NM_hashing) ? CLR_MINT : CLR_ORANGE;
  tft.fillCircle(8, 12, 3, ledColor);

  // Screen Title
  tft.setTextColor(CLR_TEXT_WHITE, CLR_PANEL);
  tft.setFreeFont(FSSB9);
  tft.drawString(screenTitle, 16, 16, GFXFF);

  // Local IP (Cyan)
  String ipStr = (WiFi.status() == WL_CONNECTED) ? ("IP: " + WiFi.localIP().toString()) : "Connecting WiFi...";
  tft.setTextColor(CLR_CYAN, CLR_PANEL);
  tft.setFreeFont(FSS9);
  tft.drawString(ipStr, 100, 16, GFXFF);

  // Time
  tft.setTextColor(CLR_TEXT_MUTED, CLR_PANEL);
  tft.drawRightString(timeStr, 314, 16, GFXFF);
}

static void drawStatusFooter(uint8_t current, uint8_t total)
{
  tft.fillRect(0, 222, 320, 18, CLR_PANEL);
  tft.drawFastHLine(0, 221, 320, CLR_BORDER);

  tft.setFreeFont(FSS9);
  tft.setTextColor(CLR_TEXT_DARK, CLR_PANEL);
  tft.drawString("< PREV", 8, 235, GFXFF);

  String poolShort = Settings.PoolAddress;
  if (poolShort.length() > 22) poolShort = poolShort.substring(0, 20) + "..";
  char buf[48];
  snprintf(buf, sizeof(buf), "%s:%d", poolShort.c_str(), Settings.PoolPort);
  tft.setTextColor(CLR_TEXT_MUTED, CLR_PANEL);
  tft.drawCentreString(buf, 160, 235, GFXFF);

  tft.setTextColor(CLR_TEXT_DARK, CLR_PANEL);
  tft.drawRightString("NEXT >", 312, 235, GFXFF);
}

// ----------------- SCREEN 1: MINER DASHBOARD -----------------
void esp32_2432S028R_MinerScreen(unsigned long mElapsed)
{
  mining_data data = getMiningData(mElapsed);

  if (hasChangedScreen) {
    tft.fillScreen(CLR_BG);
    drawStatusHeader("MINER", data.temp, data.currentTime);
    drawStatusFooter(1, 4);

    // Hero Hashrate Card (y: 28 to 114, h: 86)
    tft.fillRoundRect(6, 28, 308, 86, 6, CLR_CARD);
    tft.drawRoundRect(6, 28, 308, 86, 6, CLR_BORDER);

    tft.fillRoundRect(12, 33, 3, 13, 1, CLR_CYAN);
    tft.setFreeFont(FSSB9);
    tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
    tft.drawString("HASHRATE", 20, 44, GFXFF);

    tft.setFreeFont(FSS9);
    tft.setTextColor(CLR_MINT, CLR_CARD);
    tft.drawRightString("XTENSA HARDWARE SHA", 304, 44, GFXFF);

    // Bottom Left Card: Shares & Diff (y: 118 to 216, h: 98)
    tft.fillRoundRect(6, 118, 151, 98, 6, CLR_CARD);
    tft.drawRoundRect(6, 118, 151, 98, 6, CLR_BORDER);
    tft.fillRoundRect(12, 123, 3, 12, 1, CLR_MINT);
    tft.setFreeFont(FSSB9);
    tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
    tft.drawString("SHARES & DIFF", 20, 133, GFXFF);

    // Bottom Right Card: Config Pool & Wallet (y: 118 to 216, h: 98)
    tft.fillRoundRect(163, 118, 151, 98, 6, CLR_CARD);
    tft.drawRoundRect(163, 118, 151, 98, 6, CLR_BORDER);
    tft.fillRoundRect(169, 123, 3, 12, 1, CLR_ORANGE);
    tft.setFreeFont(FSSB9);
    tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
    tft.drawString("CONFIG POOL", 177, 133, GFXFF);

    hasChangedScreen = false;
  }

  // Header update
  drawStatusHeader("MINER", data.temp, data.currentTime);

  // 1. Hashrate Display (FreeSansBold 18pt fits cleanly within width without clip)
  tft.fillRect(16, 52, 288, 38, CLR_CARD);
  tft.setFreeFont(FF23); // FreeSansBold 18pt
  tft.setTextColor(CLR_CYAN, CLR_CARD);
  tft.drawString(data.currentHashRate, 16, 80, GFXFF);

  tft.setFreeFont(FSSB12);
  tft.setTextColor(CLR_TEXT_WHITE, CLR_CARD);
  tft.drawString("KH/s", 160, 80, GFXFF);

  // Progress bar & Total MH
  tft.fillRoundRect(16, 95, 170, 4, 2, CLR_BORDER);
  tft.fillRoundRect(16, 95, 120, 4, 2, CLR_CYAN);
  tft.fillRect(195, 87, 110, 16, CLR_CARD);
  tft.setFreeFont(FSS9);
  tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
  tft.drawRightString(data.totalMHashes + " MH", 304, 100, GFXFF);

  // 2. Shares & Diff (Left Card)
  tft.fillRect(12, 140, 139, 72, CLR_CARD);
  tft.setFreeFont(FSSB12);
  tft.setTextColor(CLR_MINT, CLR_CARD);
  tft.drawString(data.completedShares, 14, 160, GFXFF);

  tft.setFreeFont(FSS9);
  tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
  tft.drawString("accepted", 80, 160, GFXFF);

  tft.setTextColor(CLR_ORANGE, CLR_CARD);
  tft.drawString("Best: " + data.bestDiff, 14, 182, GFXFF);

  tft.setTextColor(CLR_TEXT_WHITE, CLR_CARD);
  tft.drawString("Blocks: " + data.valids, 14, 204, GFXFF);

  // 3. Pool & Config (Right Card)
  tft.fillRect(169, 140, 139, 72, CLR_CARD);
  tft.setFreeFont(FSSB9);
  tft.setTextColor(CLR_ORANGE, CLR_CARD);
  String poolLabel = Settings.PoolAddress;
  if (poolLabel.length() > 14) poolLabel = poolLabel.substring(0, 12) + "..";
  tft.drawString(poolLabel, 169, 156, GFXFF);

  tft.setFreeFont(FSS9);
  tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
  tft.drawString("Port: " + String(Settings.PoolPort), 169, 174, GFXFF);

  // Truncated wallet preview: bc1pw28..9ml3uh
  String w = Settings.BtcWallet;
  String wShort = (w.length() > 14) ? (w.substring(0, 7) + ".." + w.substring(w.length() - 5)) : w;
  tft.setTextColor(CLR_CYAN, CLR_CARD);
  tft.drawString(wShort, 169, 192, GFXFF);

  tft.setTextColor(CLR_MINT, CLR_CARD);
  tft.drawString(data.timeMining, 169, 208, GFXFF);

  Serial.printf(">>> Completed %s share(s), %s Khashes, avg. hashrate %s KH/s\n",
                data.completedShares.c_str(), data.totalKHashes.c_str(), data.currentHashRate.c_str());
}

// ----------------- SCREEN 2: CLOCK & BTC -----------------
void esp32_2432S028R_ClockScreen(unsigned long mElapsed)
{
  clock_data data = getClockData(mElapsed);

  if (hasChangedScreen) {
    tft.fillScreen(CLR_BG);
    drawStatusHeader("CLOCK & BTC", data.currentHashRate, data.currentTime);
    drawStatusFooter(2, 4);

    tft.fillRoundRect(6, 28, 308, 86, 6, CLR_CARD);
    tft.drawRoundRect(6, 28, 308, 86, 6, CLR_BORDER);
    tft.fillRoundRect(12, 33, 3, 13, 1, CLR_CYAN);
    tft.setFreeFont(FSSB9);
    tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
    tft.drawString("BITCOIN NETWORK TIME", 20, 44, GFXFF);

    tft.fillRoundRect(6, 118, 151, 98, 6, CLR_CARD);
    tft.drawRoundRect(6, 118, 151, 98, 6, CLR_BORDER);
    tft.fillRoundRect(12, 123, 3, 12, 1, CLR_ORANGE);
    tft.setFreeFont(FSSB9);
    tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
    tft.drawString("BTC PRICE", 20, 133, GFXFF);

    tft.fillRoundRect(163, 118, 151, 98, 6, CLR_CARD);
    tft.drawRoundRect(163, 118, 151, 98, 6, CLR_BORDER);
    tft.fillRoundRect(169, 123, 3, 12, 1, CLR_MINT);
    tft.setFreeFont(FSSB9);
    tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
    tft.drawString("MINER STATUS", 177, 133, GFXFF);

    hasChangedScreen = false;
  }

  drawStatusHeader("CLOCK & BTC", data.currentHashRate, data.currentTime);

  // Big Clock
  tft.fillRect(16, 52, 288, 38, CLR_CARD);
  tft.setTextColor(CLR_TEXT_WHITE, CLR_CARD);
  tft.setFreeFont(FF23);
  tft.drawString(data.currentTime, 16, 80, GFXFF);

  tft.setFreeFont(FSS9);
  tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
  tft.drawRightString(data.currentDate, 304, 80, GFXFF);

  // BTC Price
  tft.fillRect(12, 140, 139, 72, CLR_CARD);
  tft.setFreeFont(FSSB12);
  tft.setTextColor(CLR_ORANGE, CLR_CARD);
  tft.drawString(data.btcPrice, 14, 162, GFXFF);

  tft.setFreeFont(FSS9);
  tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
  tft.drawString("Block: #" + data.blockHeight, 14, 188, GFXFF);

  // Miner Stats
  tft.fillRect(169, 140, 139, 72, CLR_CARD);
  tft.setFreeFont(FSSB9);
  tft.setTextColor(CLR_CYAN, CLR_CARD);
  tft.drawString(data.currentHashRate + " KH/s", 169, 158, GFXFF);

  tft.setFreeFont(FSS9);
  tft.setTextColor(CLR_MINT, CLR_CARD);
  tft.drawString(data.completedShares + " shares", 169, 180, GFXFF);

  String ipShort = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "No WiFi";
  tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
  tft.drawString(ipShort, 169, 202, GFXFF);
}

// ----------------- SCREEN 3: GLOBAL STATS -----------------
void esp32_2432S028R_GlobalHashScreen(unsigned long mElapsed)
{
  coin_data data = getCoinData(mElapsed);

  if (hasChangedScreen) {
    tft.fillScreen(CLR_BG);
    drawStatusHeader("GLOBAL STATS", data.currentHashRate, data.currentTime);
    drawStatusFooter(3, 4);

    tft.fillRoundRect(6, 28, 308, 86, 6, CLR_CARD);
    tft.drawRoundRect(6, 28, 308, 86, 6, CLR_BORDER);
    tft.fillRoundRect(12, 33, 3, 13, 1, CLR_CYAN);
    tft.setFreeFont(FSSB9);
    tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
    tft.drawString("GLOBAL BITCOIN HASHRATE", 20, 44, GFXFF);

    tft.fillRoundRect(6, 118, 151, 98, 6, CLR_CARD);
    tft.drawRoundRect(6, 118, 151, 98, 6, CLR_BORDER);
    tft.fillRoundRect(12, 123, 3, 12, 1, CLR_ORANGE);
    tft.setFreeFont(FSSB9);
    tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
    tft.drawString("DIFFICULTY", 20, 133, GFXFF);

    tft.fillRoundRect(163, 118, 151, 98, 6, CLR_CARD);
    tft.drawRoundRect(163, 118, 151, 98, 6, CLR_BORDER);
    tft.fillRoundRect(169, 123, 3, 12, 1, CLR_MINT);
    tft.setFreeFont(FSSB9);
    tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
    tft.drawString("HALVING PROGRESS", 177, 133, GFXFF);

    hasChangedScreen = false;
  }

  drawStatusHeader("GLOBAL STATS", data.currentHashRate, data.currentTime);

  tft.fillRect(16, 52, 288, 38, CLR_CARD);
  tft.setTextColor(CLR_CYAN, CLR_CARD);
  tft.setFreeFont(FF23);
  tft.drawString(data.globalHashRate + " EH/s", 16, 80, GFXFF);

  tft.setFreeFont(FSS9);
  tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
  tft.drawRightString("Fee: " + data.halfHourFee, 304, 80, GFXFF);

  // Difficulty
  tft.fillRect(12, 140, 139, 72, CLR_CARD);
  tft.setFreeFont(FSSB12);
  tft.setTextColor(CLR_ORANGE, CLR_CARD);
  tft.drawString(data.netwrokDifficulty, 14, 165, GFXFF);

  tft.setFreeFont(FSS9);
  tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
  tft.drawString("Height: #" + data.blockHeight, 14, 192, GFXFF);

  // Halving progress
  tft.fillRect(169, 140, 139, 72, CLR_CARD);
  tft.setFreeFont(FSSB9);
  tft.setTextColor(CLR_MINT, CLR_CARD);
  tft.drawString(data.remainingBlocks, 169, 158, GFXFF);

  tft.fillRoundRect(169, 172, 135, 8, 3, CLR_BORDER);
  int pw = (135 * (int)data.progressPercent) / 100;
  if (pw > 135) pw = 135;
  tft.fillRoundRect(169, 172, pw, 8, 3, CLR_MINT);

  tft.setFreeFont(FSS9);
  tft.setTextColor(CLR_TEXT_WHITE, CLR_CARD);
  tft.drawString(String(data.progressPercent, 1) + "% Completed", 169, 198, GFXFF);
}

// ----------------- SCREEN 4: BTC PRICE & WALLET -----------------
void esp32_2432S028R_BTCprice(unsigned long mElapsed)
{
  clock_data data = getClockData(mElapsed);

  if (hasChangedScreen) {
    tft.fillScreen(CLR_BG);
    drawStatusHeader("MARKET & WALLET", data.currentHashRate, data.currentTime);
    drawStatusFooter(4, 4);

    tft.fillRoundRect(6, 28, 308, 86, 6, CLR_CARD);
    tft.drawRoundRect(6, 28, 308, 86, 6, CLR_BORDER);
    tft.fillRoundRect(12, 33, 3, 13, 1, CLR_ORANGE);
    tft.setFreeFont(FSSB9);
    tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
    tft.drawString("BITCOIN SPOT PRICE (USD)", 20, 44, GFXFF);

    tft.fillRoundRect(6, 118, 151, 98, 6, CLR_CARD);
    tft.drawRoundRect(6, 118, 151, 98, 6, CLR_BORDER);
    tft.fillRoundRect(12, 123, 3, 12, 1, CLR_CYAN);
    tft.setFreeFont(FSSB9);
    tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
    tft.drawString("ACTIVE WALLET", 20, 133, GFXFF);

    tft.fillRoundRect(163, 118, 151, 98, 6, CLR_CARD);
    tft.drawRoundRect(163, 118, 151, 98, 6, CLR_BORDER);
    tft.fillRoundRect(169, 123, 3, 12, 1, CLR_MINT);
    tft.setFreeFont(FSSB9);
    tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
    tft.drawString("POOL SERVER", 177, 133, GFXFF);

    hasChangedScreen = false;
  }

  drawStatusHeader("MARKET & WALLET", data.currentHashRate, data.currentTime);

  tft.fillRect(16, 52, 288, 38, CLR_CARD);
  tft.setTextColor(CLR_ORANGE, CLR_CARD);
  tft.setFreeFont(FF23);
  tft.drawString(data.btcPrice, 16, 80, GFXFF);

  tft.setFreeFont(FSS9);
  tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
  tft.drawRightString("Height: #" + data.blockHeight, 304, 80, GFXFF);

  // Active Wallet Card (displayed cleanly in 3 lines without overflowing)
  tft.fillRect(12, 140, 139, 72, CLR_CARD);
  tft.setFreeFont(FSS9);
  tft.setTextColor(CLR_CYAN, CLR_CARD);
  String w = Settings.BtcWallet;
  if (w.length() > 14) {
    tft.drawString(w.substring(0, 14), 14, 156, GFXFF);
    if (w.length() > 28) {
      tft.drawString(w.substring(14, 28), 14, 176, GFXFF);
      tft.drawString(w.substring(28), 14, 196, GFXFF);
    } else {
      tft.drawString(w.substring(14), 14, 176, GFXFF);
    }
  } else {
    tft.drawString(w, 14, 156, GFXFF);
  }

  // Active Pool Card
  tft.fillRect(169, 140, 139, 72, CLR_CARD);
  tft.setFreeFont(FSSB9);
  tft.setTextColor(CLR_MINT, CLR_CARD);
  String poolStr = Settings.PoolAddress;
  if (poolStr.length() > 14) poolStr = poolStr.substring(0, 12) + "..";
  tft.drawString(poolStr, 169, 156, GFXFF);

  tft.setFreeFont(FSS9);
  tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
  tft.drawString("Port: " + String(Settings.PoolPort), 169, 178, GFXFF);
  tft.setTextColor(CLR_TEXT_WHITE, CLR_CARD);
  tft.drawString(data.currentHashRate + " KH/s", 169, 200, GFXFF);
}

// ----------------- LOADING & SETUP SCREENS -----------------
void esp32_2432S028R_LoadingScreen(void)
{
  tft.fillScreen(CLR_BG);
  tft.fillRoundRect(20, 20, 280, 200, 8, CLR_CARD);
  tft.drawRoundRect(20, 20, 280, 200, 8, CLR_BORDER);

  tft.fillCircle(160, 68, 26, CLR_ORANGE);
  tft.setFreeFont(FF24);
  tft.setTextColor(CLR_TEXT_WHITE, CLR_ORANGE);
  tft.drawCentreString("B", 160, 77, GFXFF);

  tft.setFreeFont(FF22);
  tft.setTextColor(CLR_TEXT_WHITE, CLR_CARD);
  tft.drawCentreString("NERDMINER PRO", 160, 115, GFXFF);

  tft.setFreeFont(FSS9);
  tft.setTextColor(CLR_CYAN, CLR_CARD);
  tft.drawCentreString("Xtensa LX6 Hardware SHA256", 160, 142, GFXFF);

  tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
  tft.drawCentreString("Starting System...", 160, 168, GFXFF);
  tft.drawString(CURRENT_VERSION, 28, 202, FONT2);
}

void esp32_2432S028R_SetupScreen(void)
{
  tft.fillScreen(CLR_BG);
  tft.fillRoundRect(15, 15, 290, 210, 8, CLR_CARD);
  tft.drawRoundRect(15, 15, 290, 210, 8, CLR_BORDER);

  tft.setFreeFont(FF22);
  tft.setTextColor(CLR_CYAN, CLR_CARD);
  tft.drawString("WIFI SETUP MODE", 25, 42, GFXFF);

  tft.setFreeFont(FSS9);
  tft.setTextColor(CLR_TEXT_WHITE, CLR_CARD);
  tft.drawString("1. Connect to SSID:", 25, 75, GFXFF);
  tft.setTextColor(CLR_ORANGE, CLR_CARD);
  tft.drawString("NerdMinerAP", 45, 95, GFXFF);

  tft.setTextColor(CLR_TEXT_WHITE, CLR_CARD);
  tft.drawString("2. Open browser:", 25, 125, GFXFF);
  tft.setTextColor(CLR_CYAN, CLR_CARD);
  tft.drawString("http://192.168.4.1", 45, 145, GFXFF);

  tft.setTextColor(CLR_TEXT_MUTED, CLR_CARD);
  tft.drawString("Configure Wi-Fi, Pool & Wallet", 25, 178, GFXFF);
  tft.setTextColor(CLR_TEXT_DARK, CLR_CARD);
  tft.drawCentreString("Tap screen to reboot", 160, 205, GFXFF);
}

void esp32_2432S028R_AnimateCurrentScreen(unsigned long frame)
{
}

unsigned long previousMillis = 0;
unsigned long previousTouchMillis = 0;
char currentScreen = 0;

void esp32_2432S028R_DoLedStuff(unsigned long frame)
{
  unsigned long currentMillis = millis();    
  if (currentMillis - previousTouchMillis >= 500)
  { 
    int16_t t_x , t_y;
    bool pressed = touch.getXY(t_x, t_y);
    if (pressed) {                        
      if ((t_x > 235) && ((t_y > 0) && (t_y < 26))) {
        esp32_2432S028R_AlternateScreenState();
      }
      else if (t_x > 160) {
        currentDisplayDriver->current_cyclic_screen = (currentDisplayDriver->current_cyclic_screen + 1) % currentDisplayDriver->num_cyclic_screens;
      } else if (t_x <= 160) {
        currentDisplayDriver->current_cyclic_screen = currentDisplayDriver->current_cyclic_screen - 1;      
        if (currentDisplayDriver->current_cyclic_screen < 0) currentDisplayDriver->current_cyclic_screen = currentDisplayDriver->num_cyclic_screens - 1;              
      }
    }
    previousTouchMillis = currentMillis;
  }

  if (currentScreen != currentDisplayDriver->current_cyclic_screen) hasChangedScreen = true;
  currentScreen = currentDisplayDriver->current_cyclic_screen;

  switch (mMonitor.NerdStatus)
  {
  case NM_waitingConfig:
    digitalWrite(LED_PIN, LOW);
    break;

  case NM_Connecting:
    if (currentMillis - previousMillis >= 500)
    {
      previousMillis = currentMillis;
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(LED_PIN_B, !digitalRead(LED_PIN));
    }
    break;

  case NM_hashing:
    if (currentMillis - previousMillis >= 500)
    {
      previousMillis = currentMillis;
      digitalWrite(LED_PIN_B, HIGH);
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));      
    }
    break;
  }
}

CyclicScreenFunction esp32_2432S028RCyclicScreens[] = {
  esp32_2432S028R_MinerScreen,
  esp32_2432S028R_ClockScreen,
  esp32_2432S028R_GlobalHashScreen,
  esp32_2432S028R_BTCprice
};

DisplayDriver esp32_2432S028RDriver = {
    esp32_2432S028R_Init,
    esp32_2432S028R_AlternateScreenState,
    esp32_2432S028R_AlternateRotation,
    esp32_2432S028R_LoadingScreen,
    esp32_2432S028R_SetupScreen,
    esp32_2432S028RCyclicScreens,
    esp32_2432S028R_AnimateCurrentScreen,
    esp32_2432S028R_DoLedStuff,
    SCREENS_ARRAY_SIZE(esp32_2432S028RCyclicScreens),
    0,
    SCREEN_W,
    SCREEN_H
};

#endif
