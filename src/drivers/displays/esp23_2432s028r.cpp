#include "displayDriver.h"

#if defined ESP32_2432S028R || ESP32_2432S028_2USB

#include <TFT_eSPI.h>
#include <TFT_eTouch.h>
#include "media/images_320_170.h"
#include "media/images_bottom_320_70.h"
#include "media/myFonts.h"
#include "media/Free_Fonts.h"
#include "version.h"
#include "monitor.h"
#include "OpenFontRender.h"
#include <SPI.h>
#include "rotation.h"
#include "drivers/storage/nvMemory.h"
#include "drivers/storage/storage.h"

#define WIDTH 320
#define HEIGHT 240 

extern nvMemory nvMem;

OpenFontRender render;
TFT_eSPI tft = TFT_eSPI();                  // Invoke library, pins defined in platformio.ini
TFT_eSprite background = TFT_eSprite(&tft); // Invoke library sprite
SPIClass hSPI(HSPI);
TFT_eTouch<TFT_eSPI> touch(tft, ETOUCH_CS, 0xFF, hSPI); 

extern monitor_data mMonitor;
extern pool_data pData;
extern DisplayDriver *currentDisplayDriver;
extern bool invertColors; 
extern TSettings Settings;
bool hasChangedScreen = true;

void getChipInfo(void){
  Serial.print("Chip: ");
  Serial.println(ESP.getChipModel());
  Serial.print("ChipRevision: ");
  Serial.println(ESP.getChipRevision());
  Serial.print("Psram size: ");
  Serial.print(ESP.getPsramSize() / 1024);
  Serial.println("KB");
  Serial.print("Flash size: ");
  Serial.print(ESP.getFlashChipSize() / 1024);
  Serial.println("KB");
  Serial.print("CPU frequency: ");
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println("MHz");  
}

void esp32_2432S028R_Init(void)
{ 
  // getChipInfo();  
  tft.init();
  if (nvMem.loadConfig(&Settings))
    {      
     // Serial.print("Invert Colors: ");
     // Serial.println(Settings.invertColors);  
      invertColors = Settings.invertColors;           
    }  
  tft.invertDisplay(invertColors);
  tft.setRotation(1);    
  tft.setSwapBytes(true); // Swap the colour byte order when rendering
  if (invertColors) {
    tft.writecommand(ILI9341_GAMMASET);
    tft.writedata(2);
    delay(120);
    tft.writecommand(ILI9341_GAMMASET); //Gamma curve selected
    tft.writedata(1); 
  }
  hSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, ETOUCH_CS);
  touch.init();

  TFT_eTouchBase::Calibation calibation = { 233, 3785, 3731, 120, 2 };
  touch.setCalibration(calibation);

  // Configuring screen backlight brightness using ledcontrol channel 0.
  // Using 5000Hz in 8bit resolution, which gives 0-255 possible duty cycle setting.
  ledcSetup(0, 5000, 8);
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, Settings.Brightness);
 
  //background.createSprite(WIDTH, HEIGHT); // Background Sprite
  //background.setSwapBytes(true);
  //render.setDrawer(background);  // Link drawing object to background instance (so font will be rendered on background)
  //render.setLineSpaceRatio(0.9); // Espaciado entre texto

  // Load the font and check it can be read OK
  // if (render.loadFont(NotoSans_Bold, sizeof(NotoSans_Bold)))
  if (render.loadFont(DigitalNumbers, sizeof(DigitalNumbers)))
  {
    Serial.println("Initialise error");
    return;
  }
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_PIN_B, OUTPUT);
  pinMode(LED_PIN_G, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(LED_PIN_B, HIGH);
  digitalWrite(LED_PIN_G, HIGH);
  pData.bestDifficulty = "0";
  pData.workersHash = "0";
  pData.workersCount = 0;
  //Serial.println("=========== Fim Display ==============") ;
}

void esp32_2432S028R_AlternateScreenState(void)
{
  Serial.println("Switching display state");
  int screen_state_duty = ledcRead(0);
  // Switching the duty cycle for the ledc channel, where the TFT_BL pin is attached.
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

bool bottomScreenBlue = true;

void printheap(){
  Serial.print("$$ Free Heap:");
  Serial.println(ESP.getFreeHeap()); 
  // Serial.printf("### stack WMark usage: %d\n", uxTaskGetStackHighWaterMark(NULL));
}

bool createBackgroundSprite(int16_t wdt, int16_t hgt){  // Set the background and link the render, used multiple times to fit in heap
  background.createSprite(wdt, hgt) ; //Background Sprite
  // printheap();
  if (background.created()) {
      background.setColorDepth(16);
      background.setSwapBytes(true);
      render.setDrawer(background); // Link drawing object to background instance (so font will be rendered on background)
      render.setLineSpaceRatio(0.9);      
  } else {
    Serial.println("#### Sprite Error ####");
    Serial.printf("Size w:%d h:%d \n", wdt, hgt);
    printheap();
  }
  return background.created();
}

extern unsigned long mPoolUpdate;

void printPoolData(){
  if ((hasChangedScreen) || (mPoolUpdate == 0) || (millis() - mPoolUpdate > UPDATE_POOL_min * 60 * 1000)){     
      if (Settings.PoolAddress != "tn.vkbit.com") { 
          pData = getPoolData();             
          background.createSprite(320,50); //Background Sprite
          if (!background.created()) {    
            Serial.println("###### POOL SPRITE ERROR ######");
          // Serial.printf("Pool data W:%d H:%s D:%s\n", pData.workersCount, pData.workersHash, pData.bestDifficulty);
            printheap();        
          }       
          background.setSwapBytes(true);
          if (bottomScreenBlue) {
            background.pushImage(0, -20, 320, 70, bottonPoolScreen);
            tft.pushImage(0,170,320,20,bottonPoolScreen);      
          } else {
            background.pushImage(0, -20, 320, 70, bottonPoolScreen_g);
            tft.pushImage(0,170,320,20,bottonPoolScreen_g);
          }
                
          render.setDrawer(background); // Link drawing object to background instance (so font will be rendered on background)
          render.setLineSpaceRatio(1);
          
          render.setFontSize(24);
          render.cdrawString(String(pData.workersCount).c_str(), 157, 16, TFT_BLACK);
          render.setFontSize(18);
          render.setAlignment(Align::BottomRight);
          render.cdrawString(pData.workersHash.c_str(), 265, 14, TFT_BLACK);
          render.setAlignment(Align::BottomLeft);
          render.cdrawString(pData.bestDifficulty.c_str(), 54, 14, TFT_BLACK);
          background.pushSprite(0,190);      
          background.deleteSprite();
      } else {
        pData.bestDifficulty = "TESTNET";
        pData.workersHash = "TESTNET";
        pData.workersCount = 1;
        tft.fillRect(0,170,320,70, TFT_DARKGREEN);        
        background.createSprite(320,40); //Background Sprite
        background.fillSprite(TFT_DARKGREEN);
          if (!background.created()) {    
            Serial.println("###### POOL SPRITE ERROR ######");
          // Serial.printf("Pool data W:%d H:%s D:%s\n", pData.workersCount, pData.workersHash, pData.bestDifficulty);
            printheap();        
          }
        background.setFreeFont(FF24);
        background.setTextDatum(TL_DATUM);
        background.setTextSize(1);
        background.setTextColor(TFT_WHITE, TFT_DARKGREEN);        
        background.drawString("TESTNET", 50, 0, GFXFF);
        background.pushSprite(0,185);  
        mPoolUpdate = millis();
        Serial.println("Testnet");
        background.deleteSprite();
      }
  }
}



// ================= Clean Tech Color Palette (RGB565) =================
#define CT_BG           0x0862  // Deep dark slate/navy (#0a0e17)
#define CT_CARD_BG      0x10E4  // Dark slate card background (#151c28)
#define CT_CARD_BORDER  0x21E8  // Subtle card border (#283548)
#define CT_CYAN         0x06BF  // Electric Cyan (#00d4ff)
#define CT_TEAL         0x2EB8  // Mint/Teal (#2dd4bf)
#define CT_ORANGE       0xFD20  // Bitcoin Orange (#f7931a)
#define CT_GREEN        0x1706  // Emerald Green (#10b981)
#define CT_RED          0xF9A7  // Crimson Red (#f43f5e)
#define CT_TEXT_LIGHT   0xFFFF  // Crisp White (#ffffff)
#define CT_TEXT_MUTED   0x9CF3  // Slate Gray (#94a3b8)
#define CT_TEXT_DARK    0x632C  // Dim Slate (#64748b)

static void drawCleanTechHeader(const char* screenName, const String& temp, const String& time) {
  tft.fillRect(0, 0, 320, 26, CT_BG);
  tft.fillCircle(10, 13, 3, CT_GREEN);
  tft.setTextColor(CT_CYAN, CT_BG);
  tft.drawString("NERDMINER", 18, 5, 2);
  tft.setTextColor(CT_TEXT_MUTED, CT_BG);
  tft.drawString(screenName, 115, 5, 2);
  tft.setTextColor(CT_ORANGE, CT_BG);
  tft.drawString((temp + "C").c_str(), 220, 5, 2);
  tft.setTextColor(CT_TEXT_LIGHT, CT_BG);
  tft.drawString(time.c_str(), 262, 5, 2);
  tft.drawFastHLine(0, 25, 320, CT_CARD_BORDER);
}

static void drawCleanTechFooter(uint8_t screenIdx, uint8_t totalScreens) {
  tft.fillRect(0, 220, 320, 20, CT_BG);
  tft.drawFastHLine(0, 220, 320, CT_CARD_BORDER);
  tft.setTextColor(CT_TEXT_DARK, CT_BG);
  tft.drawString("< PREV", 8, 223, 2);
  char buf[28];
  snprintf(buf, sizeof(buf), "CLEANTECH %d/%d", screenIdx, totalScreens);
  tft.setTextColor(CT_TEXT_MUTED, CT_BG);
  tft.drawCentreString(buf, 160, 223, 2);
  tft.setTextColor(CT_TEXT_DARK, CT_BG);
  tft.drawString("NEXT >", 265, 223, 2);
}

void esp32_2432S028R_MinerScreen(unsigned long mElapsed)
{
  mining_data data = getMiningData(mElapsed);

  if (hasChangedScreen) {
    tft.fillScreen(CT_BG);
    drawCleanTechHeader("DASHBOARD", data.temp, data.currentTime);
    drawCleanTechFooter(1, 4);

    // Hero Card Frame (Hashrate)
    tft.fillRoundRect(6, 28, 308, 92, 6, CT_CARD_BG);
    tft.drawRoundRect(6, 28, 308, 92, 6, CT_CARD_BORDER);
    tft.fillRect(14, 34, 3, 10, CT_CYAN);
    tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
    tft.drawString("HASHRATE", 22, 32, 2);
    tft.setTextColor(CT_TEAL, CT_CARD_BG);
    tft.drawString("XTENSA LX6 - 240MHz", 150, 32, 2);

    // Left Bottom Card Frame (Shares & Blocks)
    tft.fillRoundRect(6, 124, 151, 92, 6, CT_CARD_BG);
    tft.drawRoundRect(6, 124, 151, 92, 6, CT_CARD_BORDER);
    tft.fillRect(14, 130, 3, 10, CT_GREEN);
    tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
    tft.drawString("SHARES & DIFF", 22, 128, 2);

    // Right Bottom Card Frame (Pool & System)
    tft.fillRoundRect(163, 124, 151, 92, 6, CT_CARD_BG);
    tft.drawRoundRect(163, 124, 151, 92, 6, CT_CARD_BORDER);
    tft.fillRect(171, 130, 3, 10, CT_ORANGE);
    tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
    tft.drawString("POOL & TIME", 179, 128, 2);

    hasChangedScreen = false;
  }

  // Dynamic Header Update
  drawCleanTechHeader("DASHBOARD", data.temp, data.currentTime);

  // Dynamic Hero Card
  tft.fillRect(14, 48, 290, 48, CT_CARD_BG);
  tft.setTextColor(CT_CYAN, CT_CARD_BG);
  tft.drawString(data.currentHashRate.c_str(), 18, 50, 6);
  tft.setTextColor(CT_TEXT_LIGHT, CT_CARD_BG);
  tft.drawString("KH/s", 225, 68, 4);

  // Progress/Activity line in hero card
  tft.fillRoundRect(18, 104, 170, 4, 2, CT_CARD_BORDER);
  tft.fillRoundRect(18, 104, 120, 4, 2, CT_CYAN);
  tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
  tft.fillRect(195, 100, 110, 16, CT_CARD_BG);
  tft.drawString((data.totalMHashes + " MH").c_str(), 195, 100, 2);

  // Dynamic Left Card (Shares & Diff)
  tft.fillRect(14, 146, 135, 66, CT_CARD_BG);
  tft.setTextColor(CT_GREEN, CT_CARD_BG);
  tft.drawString(data.completedShares.c_str(), 18, 146, 4);
  tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
  tft.drawString("Accepted", 85, 152, 2);
  tft.setTextColor(CT_ORANGE, CT_CARD_BG);
  tft.drawString(("Best: " + data.bestDiff).c_str(), 18, 176, 2);
  tft.setTextColor(CT_TEXT_LIGHT, CT_CARD_BG);
  tft.drawString(("Blocks: " + data.valids).c_str(), 18, 194, 2);

  // Dynamic Right Card (Pool & System)
  tft.fillRect(171, 146, 135, 66, CT_CARD_BG);
  tft.setTextColor(CT_CYAN, CT_CARD_BG);
  String poolDisplay = Settings.PoolAddress;
  if (poolDisplay.length() > 14) poolDisplay = poolDisplay.substring(0, 14);
  tft.drawString(poolDisplay.c_str(), 171, 146, 2);
  tft.setTextColor(CT_TEXT_LIGHT, CT_CARD_BG);
  tft.drawString(data.timeMining.c_str(), 171, 168, 2);
  tft.setTextColor(CT_TEAL, CT_CARD_BG);
  tft.drawString("Pipelined ASM", 171, 192, 2);

  Serial.printf(">>> Completed %s share(s), %s Khashes, avg. hashrate %s KH/s\n",
                data.completedShares.c_str(), data.totalKHashes.c_str(), data.currentHashRate.c_str());
}

void esp32_2432S028R_ClockScreen(unsigned long mElapsed)
{
  clock_data data = getClockData(mElapsed);

  if (hasChangedScreen) {
    tft.fillScreen(CT_BG);
    drawCleanTechHeader("CLOCK & BTC", data.currentHashRate, data.currentTime);
    drawCleanTechFooter(2, 4);

    // Hero Clock Card Frame
    tft.fillRoundRect(6, 28, 308, 92, 6, CT_CARD_BG);
    tft.drawRoundRect(6, 28, 308, 92, 6, CT_CARD_BORDER);
    tft.fillRect(14, 34, 3, 10, CT_CYAN);
    tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
    tft.drawString("BITCOIN NETWORK TIME", 22, 32, 2);

    // Left Card Frame (BTC Price)
    tft.fillRoundRect(6, 124, 151, 92, 6, CT_CARD_BG);
    tft.drawRoundRect(6, 124, 151, 92, 6, CT_CARD_BORDER);
    tft.fillRect(14, 130, 3, 10, CT_ORANGE);
    tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
    tft.drawString("BITCOIN PRICE", 22, 128, 2);

    // Right Card Frame (Hashrate & Diff)
    tft.fillRoundRect(163, 124, 151, 92, 6, CT_CARD_BG);
    tft.drawRoundRect(163, 124, 151, 92, 6, CT_CARD_BORDER);
    tft.fillRect(171, 130, 3, 10, CT_TEAL);
    tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
    tft.drawString("MINER STATS", 179, 128, 2);

    hasChangedScreen = false;
  }

  drawCleanTechHeader("CLOCK & BTC", data.currentHashRate, data.currentTime);

  // Big Clock in Hero Card
  tft.fillRect(14, 48, 290, 48, CT_CARD_BG);
  tft.setTextColor(CT_CYAN, CT_CARD_BG);
  tft.drawString(data.currentTime.c_str(), 18, 50, 6);
  tft.setTextColor(CT_TEXT_LIGHT, CT_CARD_BG);
  tft.drawString(data.currentDate.c_str(), 18, 98, 2);

  // BTC Price Card
  tft.fillRect(14, 146, 135, 66, CT_CARD_BG);
  tft.setTextColor(CT_ORANGE, CT_CARD_BG);
  tft.drawString(data.btcPrice.c_str(), 18, 146, 4);
  tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
  tft.drawString(("Block: #" + data.blockHeight).c_str(), 18, 180, 2);

  // Right Card (Hashrate / Shares)
  tft.fillRect(171, 146, 135, 66, CT_CARD_BG);
  tft.setTextColor(CT_TEAL, CT_CARD_BG);
  tft.drawString((data.currentHashRate + " KH/s").c_str(), 171, 146, 2);
  tft.setTextColor(CT_GREEN, CT_CARD_BG);
  tft.drawString((data.completedShares + " shares").c_str(), 171, 168, 2);
  tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
  tft.drawString("Dual-Core 240MHz", 171, 192, 2);

  Serial.printf(">>> Completed %s share(s), %s Khashes, avg. hashrate %s KH/s\n",
                data.completedShares.c_str(), data.totalKHashes.c_str(), data.currentHashRate.c_str());
}

void esp32_2432S028R_GlobalHashScreen(unsigned long mElapsed)
{
  coin_data data = getCoinData(mElapsed);

  if (hasChangedScreen) {
    tft.fillScreen(CT_BG);
    drawCleanTechHeader("GLOBAL STATS", data.currentHashRate, data.currentTime);
    drawCleanTechFooter(3, 4);

    // Hero Global Hash Card Frame
    tft.fillRoundRect(6, 28, 308, 92, 6, CT_CARD_BG);
    tft.drawRoundRect(6, 28, 308, 92, 6, CT_CARD_BORDER);
    tft.fillRect(14, 34, 3, 10, CT_CYAN);
    tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
    tft.drawString("GLOBAL HASHRATE", 22, 32, 2);

    // Left Card Frame (Difficulty)
    tft.fillRoundRect(6, 124, 151, 92, 6, CT_CARD_BG);
    tft.drawRoundRect(6, 124, 151, 92, 6, CT_CARD_BORDER);
    tft.fillRect(14, 130, 3, 10, CT_ORANGE);
    tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
    tft.drawString("NETWORK DIFF", 22, 128, 2);

    // Right Card Frame (Halving / Block)
    tft.fillRoundRect(163, 124, 151, 92, 6, CT_CARD_BG);
    tft.drawRoundRect(163, 124, 151, 92, 6, CT_CARD_BORDER);
    tft.fillRect(171, 130, 3, 10, CT_TEAL);
    tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
    tft.drawString("BLOCK & FEES", 179, 128, 2);

    hasChangedScreen = false;
  }

  drawCleanTechHeader("GLOBAL STATS", data.currentHashRate, data.currentTime);

  // Big Global Hash
  tft.fillRect(14, 48, 290, 48, CT_CARD_BG);
  tft.setTextColor(CT_CYAN, CT_CARD_BG);
  tft.drawString((data.globalHashRate + " EH/s").c_str(), 18, 50, 6);
  tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
  tft.drawString(("Mempool: " + data.halfHourFee).c_str(), 18, 98, 2);

  // Network Diff Card
  tft.fillRect(14, 146, 135, 66, CT_CARD_BG);
  tft.setTextColor(CT_ORANGE, CT_CARD_BG);
  tft.drawString(data.netwrokDifficulty.c_str(), 18, 146, 4);
  tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
  tft.drawString(("Block: #" + data.blockHeight).c_str(), 18, 180, 2);

  // Right Card (Halving progress)
  tft.fillRect(171, 146, 135, 66, CT_CARD_BG);
  tft.setTextColor(CT_TEAL, CT_CARD_BG);
  tft.drawString(data.remainingBlocks.c_str(), 171, 146, 2);
  tft.fillRoundRect(171, 172, 130, 8, 3, CT_CARD_BORDER);
  int pw = (130 * data.progressPercent) / 100;
  if (pw > 130) pw = 130;
  tft.fillRoundRect(171, 172, pw, 8, 3, CT_TEAL);
  tft.setTextColor(CT_TEXT_LIGHT, CT_CARD_BG);
  tft.drawString((String(data.progressPercent) + "% completed").c_str(), 171, 192, 2);
}

void esp32_2432S028R_BTCprice(unsigned long mElapsed)
{
  clock_data data = getClockData(mElapsed);

  if (hasChangedScreen) {
    tft.fillScreen(CT_BG);
    drawCleanTechHeader("MARKET & PRICE", data.currentHashRate, data.currentTime);
    drawCleanTechFooter(4, 4);

    // Hero Card Frame (BTC Price)
    tft.fillRoundRect(6, 28, 308, 92, 6, CT_CARD_BG);
    tft.drawRoundRect(6, 28, 308, 92, 6, CT_CARD_BORDER);
    tft.fillRect(14, 34, 3, 10, CT_ORANGE);
    tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
    tft.drawString("BITCOIN SPOT PRICE (USD)", 22, 32, 2);

    // Left Card Frame
    tft.fillRoundRect(6, 124, 151, 92, 6, CT_CARD_BG);
    tft.drawRoundRect(6, 124, 151, 92, 6, CT_CARD_BORDER);
    tft.fillRect(14, 130, 3, 10, CT_CYAN);
    tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
    tft.drawString("BLOCK HEIGHT", 22, 128, 2);

    // Right Card Frame
    tft.fillRoundRect(163, 124, 151, 92, 6, CT_CARD_BG);
    tft.drawRoundRect(163, 124, 151, 92, 6, CT_CARD_BORDER);
    tft.fillRect(171, 130, 3, 10, CT_GREEN);
    tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
    tft.drawString("MINER HASHRATE", 179, 128, 2);

    hasChangedScreen = false;
  }

  drawCleanTechHeader("MARKET & PRICE", data.currentHashRate, data.currentTime);

  // Huge Price
  tft.fillRect(14, 48, 290, 48, CT_CARD_BG);
  tft.setTextColor(CT_ORANGE, CT_CARD_BG);
  tft.drawString(("$" + data.btcPrice).c_str(), 18, 50, 6);
  tft.setTextColor(CT_TEXT_LIGHT, CT_CARD_BG);
  tft.drawString(data.currentDate.c_str(), 18, 98, 2);

  // Left Card (Block)
  tft.fillRect(14, 146, 135, 66, CT_CARD_BG);
  tft.setTextColor(CT_CYAN, CT_CARD_BG);
  tft.drawString(("#" + data.blockHeight).c_str(), 18, 152, 4);
  tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
  tft.drawString("Bitcoin Mainnet", 18, 186, 2);

  // Right Card (Hashrate)
  tft.fillRect(171, 146, 135, 66, CT_CARD_BG);
  tft.setTextColor(CT_GREEN, CT_CARD_BG);
  tft.drawString(data.currentHashRate.c_str(), 171, 146, 4);
  tft.setTextColor(CT_TEXT_LIGHT, CT_CARD_BG);
  tft.drawString("KH/s", 270, 154, 2);
  tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
  tft.drawString((data.completedShares + " shares").c_str(), 171, 186, 2);
}

void esp32_2432S028R_LoadingScreen(void)
{
  tft.fillScreen(CT_BG);
  // Outer decorative card
  tft.fillRoundRect(20, 20, 280, 200, 10, CT_CARD_BG);
  tft.drawRoundRect(20, 20, 280, 200, 10, CT_CARD_BORDER);

  // Bitcoin Gold/Orange emblem
  tft.fillCircle(160, 68, 26, CT_ORANGE);
  tft.drawCircle(160, 68, 29, CT_CYAN);
  tft.setTextColor(TFT_WHITE, CT_ORANGE);
  tft.drawCentreString("B", 160, 52, 4);

  // Brand and edition titles
  tft.setTextColor(CT_TEXT_LIGHT, CT_CARD_BG);
  tft.drawCentreString("NERDMINER", 160, 106, 4);
  tft.setTextColor(CT_CYAN, CT_CARD_BG);
  tft.drawCentreString("CLEANTECH EDITION", 160, 136, 2);

  tft.setTextColor(CT_TEAL, CT_CARD_BG);
  tft.drawCentreString("Xtensa LX6 - Pipelined SHA-256", 160, 158, 2);

  tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
  tft.drawCentreString(CURRENT_VERSION, 160, 184, 2);
}

void esp32_2432S028R_SetupScreen(void)
{
  tft.fillScreen(CT_BG);
  tft.fillRoundRect(15, 15, 290, 210, 8, CT_CARD_BG);
  tft.drawRoundRect(15, 15, 290, 210, 8, CT_CARD_BORDER);

  tft.fillRect(30, 28, 4, 14, CT_CYAN);
  tft.setTextColor(CT_CYAN, CT_CARD_BG);
  tft.drawString("WIFI SETUP MODE", 40, 26, 4);

  tft.setTextColor(CT_TEXT_LIGHT, CT_CARD_BG);
  tft.drawString("1. Connect to Wi-Fi AP:", 30, 65, 2);
  tft.setTextColor(CT_ORANGE, CT_CARD_BG);
  tft.drawString("SSID: NerdMinerAP", 50, 85, 2);

  tft.setTextColor(CT_TEXT_LIGHT, CT_CARD_BG);
  tft.drawString("2. Open browser at:", 30, 115, 2);
  tft.setTextColor(CT_TEAL, CT_CARD_BG);
  tft.drawString("http://192.168.4.1", 50, 135, 2);

  tft.setTextColor(CT_TEXT_MUTED, CT_CARD_BG);
  tft.drawString("3. Configure your Wi-Fi & BTC wallet", 30, 165, 2);
  tft.setTextColor(CT_TEXT_DARK, CT_CARD_BG);
  tft.drawCentreString("Tap anywhere to reboot when ready", 160, 195, 2);
}

void esp32_2432S028R_AnimateCurrentScreen(unsigned long frame)
{
}

// Variables para controlar el parpadeo con millis()
unsigned long previousMillis = 0;
unsigned long previousTouchMillis = 0;
char currentScreen = 0;

void esp32_2432S028R_DoLedStuff(unsigned long frame)
{
  unsigned long currentMillis = millis();    
  // / Check the touch coordinates 110x185 210x240
  if (currentMillis - previousTouchMillis >= 500)
    { 
      int16_t t_x , t_y;  // To store the touch coordinates
      bool pressed = touch.getXY(t_x, t_y);
      if (pressed) {                        
          if (((t_x > 109)&&(t_x < 211)) && ((t_y > 185)&&(t_y < 241))) {
            bottomScreenBlue ^= true;
            hasChangedScreen = true;
          } else if((t_x > 235) && ((t_y > 0)&&(t_y < 16))) {
            // Touching the top right corner of the screen, roughly in the gray status label.
            // Disabling the screen backlight. 
            esp32_2432S028R_AlternateScreenState();
          }
          else
            if (t_x > 160) {
              // next screen
             // Serial.printf("Next screen touch( x:%d y:%d )\n", t_x, t_y);              
              currentDisplayDriver->current_cyclic_screen = (currentDisplayDriver->current_cyclic_screen + 1) % currentDisplayDriver->num_cyclic_screens;
            } else if (t_x < 160)
            {
              // Previus screen
             // Serial.printf("Previus screen touch( x:%d y:%d )\n", t_x, t_y);              
              /* Serial.println(currentDisplayDriver->current_cyclic_screen); */
              currentDisplayDriver->current_cyclic_screen = currentDisplayDriver->current_cyclic_screen - 1;      
              if (currentDisplayDriver->current_cyclic_screen<0) currentDisplayDriver->current_cyclic_screen = currentDisplayDriver->num_cyclic_screens - 1;              
            }
      }
      previousTouchMillis = currentMillis;
    }

    if (currentScreen != currentDisplayDriver->current_cyclic_screen) hasChangedScreen ^= true;
    currentScreen = currentDisplayDriver->current_cyclic_screen;

  switch (mMonitor.NerdStatus)
  {
  case NM_waitingConfig:
    digitalWrite(LED_PIN, LOW); // LED encendido de forma continua
    break;

  case NM_Connecting:
    if (currentMillis - previousMillis >= 500)
    { // 0.5sec blink
      previousMillis = currentMillis;
      // Serial.print("C");
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(LED_PIN_B, !digitalRead(LED_PIN)); // Cambia el estado del LED
    }
    break;

  case NM_hashing:
    if (currentMillis - previousMillis >= 500)
    { // 0.1sec blink
      // Serial.print("h");
      previousMillis = currentMillis;
      digitalWrite(LED_PIN_B, HIGH);
      digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Cambia el estado del LED      
    }
    break;
  }
  

}

CyclicScreenFunction esp32_2432S028RCyclicScreens[] = {esp32_2432S028R_MinerScreen, esp32_2432S028R_ClockScreen, esp32_2432S028R_GlobalHashScreen, esp32_2432S028R_BTCprice};

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
    WIDTH,
    HEIGHT};
#endif
