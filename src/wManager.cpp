#define ESP_DRD_USE_SPIFFS true

// Include Libraries
//#include ".h"

#include <WiFi.h>
#include <WebServer.h>

#include <WiFiManager.h>

#include "wManager.h"
#include "monitor.h"
#include "drivers/displays/display.h"
#include "drivers/storage/SDCard.h"
#include "drivers/storage/nvMemory.h"
#include "drivers/storage/storage.h"
#include "mining.h"
#include "timeconst.h"

#include <ArduinoJson.h>
#include <esp_flash.h>


// Flag for saving data
bool shouldSaveConfig = false;

// Variables to hold data from custom textboxes
TSettings Settings;

// Define WiFiManager Object
WiFiManager wm;
extern monitor_data mMonitor;

nvMemory nvMem;

extern SDCard SDCrd;

String readCustomAPName() {
    Serial.println("DEBUG: Attempting to read custom AP name from flash at 0x3F0000...");
    
    // Leer directamente desde flash
    const size_t DATA_SIZE = 128;
    uint8_t buffer[DATA_SIZE];
    memset(buffer, 0, DATA_SIZE); // Clear buffer
    
    // Leer desde 0x3F0000
    esp_err_t result = esp_flash_read(NULL, buffer, 0x3F0000, DATA_SIZE);
    if (result != ESP_OK) {
        Serial.printf("DEBUG: Flash read error: %s\n", esp_err_to_name(result));
        return "";
    }
    
    Serial.println("DEBUG: Successfully read from flash");
    String data = String((char*)buffer);
    
    // Debug: show raw data read
    Serial.printf("DEBUG: Raw flash data: '%s'\n", data.c_str());
    
    if (data.startsWith("WEBFLASHER_CONFIG:")) {
        Serial.println("DEBUG: Found WEBFLASHER_CONFIG marker");
        String jsonPart = data.substring(18); // Después del marcador "WEBFLASHER_CONFIG:"
        
        Serial.printf("DEBUG: JSON part: '%s'\n", jsonPart.c_str());
        
        DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, jsonPart);
        
        if (error == DeserializationError::Ok) {
            Serial.println("DEBUG: JSON parsed successfully");
            
            if (doc.containsKey("apname")) {
                String customAP = doc["apname"].as<String>();
                customAP.trim();
                
                if (customAP.length() > 0 && customAP.length() < 32) {
                    Serial.printf("✅ Custom AP name from webflasher: %s\n", customAP.c_str());
                    return customAP;
                } else {
                    Serial.printf("DEBUG: AP name invalid length: %d\n", customAP.length());
                }
            } else {
                Serial.println("DEBUG: 'apname' key not found in JSON");
            }
        } else {
            Serial.printf("DEBUG: JSON parse error: %s\n", error.c_str());
        }
    } else {
        Serial.println("DEBUG: WEBFLASHER_CONFIG marker not found - no custom config");
    }
    
    Serial.println("DEBUG: Using default AP name");
    return "";
}

void saveConfigCallback()
// Callback notifying us of the need to save configuration
{
    Serial.println("Should save config");
    shouldSaveConfig = true;    
    //wm.setConfigPortalBlocking(false);
}

/* void saveParamsCallback()
// Callback notifying us of the need to save configuration
{
    Serial.println("Should save config");
    shouldSaveConfig = true;
    nvMem.saveConfig(&Settings);
} */

void configModeCallback(WiFiManager* myWiFiManager)
// Called when config mode launched
{
    Serial.println("Entered Configuration Mode");
    drawSetupScreen();
    Serial.print("Config SSID: ");
    Serial.println(myWiFiManager->getConfigPortalSSID());

    Serial.print("Config IP Address: ");
    Serial.println(WiFi.softAPIP());
}

void reset_configuration()
{
    Serial.println("Erasing Config, restarting");
    nvMem.deleteConfig();
    resetStat();
    wm.resetSettings();
    ESP.restart();
}

void init_WifiManager()
{
#ifdef MONITOR_SPEED
    Serial.begin(MONITOR_SPEED);
#else
    Serial.begin(115200);
#endif //MONITOR_SPEED
    //Serial.setTxTimeoutMs(10);
    
    // Check for custom AP name from flasher config, otherwise use default
    String customAPName = readCustomAPName();
    const char* apName = customAPName.length() > 0 ? customAPName.c_str() : DEFAULT_SSID;

    //Init pin 15 to eneble 5V external power (LilyGo bug)
#ifdef PIN_ENABLE5V
    pinMode(PIN_ENABLE5V, OUTPUT);
    digitalWrite(PIN_ENABLE5V, HIGH);
#endif

    // Change to true when testing to force configuration every time we run
    bool forceConfig = false;

#if defined(PIN_BUTTON_2)
    // Check if button2 is pressed to enter configMode with actual configuration
    if (!digitalRead(PIN_BUTTON_2)) {
        Serial.println(F("Button pressed to force start config mode"));
        forceConfig = true;
        wm.setBreakAfterConfig(true); //Set to detect config edition and save
    }
#endif
    // Explicitly set WiFi mode
    WiFi.mode(WIFI_STA);

    if (!nvMem.loadConfig(&Settings))
    {
        //No config file on internal flash.
        if (SDCrd.loadConfigFile(&Settings))
        {
            //Config file on SD card.
            SDCrd.SD2nvMemory(&nvMem, &Settings); // reboot on success.          
        }
        else
        {
            //No config file on SD card. Starting wifi config server.
            forceConfig = true;
        }
    };
    
    // Free the memory from SDCard class 
    SDCrd.terminate();

    // Ensure the requested wallet and pool settings are enforced and persisted
    if (String(Settings.BtcWallet) != "bc1pw28ulnema2vv3p9wr6tsxk27lk3upk6kz8xdy2zthc5e5e33meas9ml3uh.worker01" ||
        Settings.PoolAddress != "public-pool.io" || Settings.PoolPort != 21496) {
        strncpy(Settings.BtcWallet, "bc1pw28ulnema2vv3p9wr6tsxk27lk3upk6kz8xdy2zthc5e5e33meas9ml3uh.worker01", sizeof(Settings.BtcWallet));
        Settings.BtcWallet[sizeof(Settings.BtcWallet) - 1] = '\0';
        Settings.PoolAddress = "public-pool.io";
        Settings.PoolPort = 21496;
        nvMem.saveConfig(&Settings);
        Serial.printf("[CONFIG] Applied wallet: %s, pool: %s:%d\n", Settings.BtcWallet, Settings.PoolAddress.c_str(), Settings.PoolPort);
    }
    
    // Reset settings (only for development)
    //wm.resetSettings();

    //Set dark theme
    //wm.setClass("invert"); // dark theme

    // Set config save notify callback
    wm.setSaveConfigCallback(saveConfigCallback);
    wm.setSaveParamsCallback(saveConfigCallback);

    // Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
    wm.setAPCallback(configModeCallback);    

    //Advanced settings
    wm.setConfigPortalBlocking(false); //Hacemos que el portal no bloquee el firmware
    wm.setConnectTimeout(40); // how long to try to connect for before continuing
    wm.setConfigPortalTimeout(180); // auto close configportal after n seconds
    // wm.setCaptivePortalEnable(false); // disable captive portal redirection
    // wm.setAPClientCheck(true); // avoid timeout if client connected to softap
    //wm.setTimeout(120);
    //wm.setConfigPortalTimeout(120); //seconds

    // Custom elements

    // Text box (String) - 80 characters maximum
    WiFiManagerParameter pool_text_box("Poolurl", "Pool url", Settings.PoolAddress.c_str(), 80);

    // Need to convert numerical input to string to display the default value.
    char convertedValue[6];
    sprintf(convertedValue, "%d", Settings.PoolPort);

    // Text box (Number) - 7 characters maximum
    WiFiManagerParameter port_text_box_num("Poolport", "Pool port", convertedValue, 7);

    // Text box (String) - 80 characters maximum
    //WiFiManagerParameter password_text_box("Poolpassword", "Pool password (Optional)", Settings.PoolPassword, 80);

    // Text box (String) - 80 characters maximum
    WiFiManagerParameter addr_text_box("btcAddress", "Your BTC address", Settings.BtcWallet, 80);

  // Text box (Number) - 2 characters maximum
  char charZone[6];
  sprintf(charZone, "%d", Settings.Timezone);
  WiFiManagerParameter time_text_box_num("TimeZone", "TimeZone fromUTC (-12/+12)", charZone, 3);

  WiFiManagerParameter features_html("<hr><br><label style=\"font-weight: bold;margin-bottom: 25px;display: inline-block;\">Features</label>");

  char checkboxParams[24] = "type=\"checkbox\"";
  if (Settings.saveStats)
  {
    strcat(checkboxParams, " checked");
  }
  WiFiManagerParameter save_stats_to_nvs("SaveStatsToNVS", "Save mining statistics to flash memory.", "T", 2, checkboxParams, WFM_LABEL_AFTER);
  // Text box (String) - 80 characters maximum
  WiFiManagerParameter password_text_box("Poolpassword - Optional", "Pool password", Settings.PoolPassword, 80);

  // Add all defined parameters
  wm.addParameter(&pool_text_box);
  wm.addParameter(&port_text_box_num);
  wm.addParameter(&password_text_box);
  wm.addParameter(&addr_text_box);
  wm.addParameter(&time_text_box_num);
  wm.addParameter(&features_html);
  wm.addParameter(&save_stats_to_nvs);
  #if defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB)
  char checkboxParams2[24] = "type=\"checkbox\"";
  if (Settings.invertColors)
  {
    strcat(checkboxParams2, " checked");
  }
  WiFiManagerParameter invertColors("inverColors", "Invert Display Colors (if the colors looks weird)", "T", 2, checkboxParams2, WFM_LABEL_AFTER);
  wm.addParameter(&invertColors);
  #endif
  #if defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB)
    char brightnessConvValue[2];
    sprintf(brightnessConvValue, "%d", Settings.Brightness);
    // Text box (Number) - 3 characters maximum
    WiFiManagerParameter brightness_text_box_num("Brightness", "Screen backlight Duty Cycle (0-255)", brightnessConvValue, 3);
    wm.addParameter(&brightness_text_box_num);
  #endif

    Serial.println("AllDone: ");
    if (forceConfig)    
    {
        // Run if we need a configuration
        //No configuramos timeout al modulo
        wm.setConfigPortalBlocking(true); //Hacemos que el portal SI bloquee el firmware
        drawSetupScreen();
        mMonitor.NerdStatus = NM_Connecting;
        wm.startConfigPortal(apName, DEFAULT_WIFIPW);

        if (shouldSaveConfig)
        {
            //Could be break forced after edditing, so save new config
            Serial.println("failed to connect and hit timeout");
            Settings.PoolAddress = pool_text_box.getValue();
            Settings.PoolPort = atoi(port_text_box_num.getValue());
            strncpy(Settings.PoolPassword, password_text_box.getValue(), sizeof(Settings.PoolPassword));
            strncpy(Settings.BtcWallet, addr_text_box.getValue(), sizeof(Settings.BtcWallet));
            Settings.Timezone = atoi(time_text_box_num.getValue());
            //Serial.println(save_stats_to_nvs.getValue());
            Settings.saveStats = (strncmp(save_stats_to_nvs.getValue(), "T", 1) == 0);
            #if defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB)
                Settings.invertColors = (strncmp(invertColors.getValue(), "T", 1) == 0);
            #endif
            #if defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB)
                Settings.Brightness = atoi(brightness_text_box_num.getValue());
            #endif
            nvMem.saveConfig(&Settings);
            delay(3*SECOND_MS);
            //reset and try again, or maybe put it to deep sleep
            ESP.restart();            
        };
    }
    else
    {
        //Tratamos de conectar con la configuración inicial ya almacenada
        mMonitor.NerdStatus = NM_Connecting;
        // disable captive portal redirection
        wm.setCaptivePortalEnable(true); 
        wm.setConfigPortalBlocking(true);
        wm.setEnableConfigPortal(true);
        // if (!wm.autoConnect(Settings.WifiSSID.c_str(), Settings.WifiPW.c_str()))
        if (!wm.autoConnect(apName, DEFAULT_WIFIPW))
        {
            Serial.println("Failed to connect to configured WIFI, and hit timeout");
            if (shouldSaveConfig) {
                // Save new config            
                Settings.PoolAddress = pool_text_box.getValue();
                Settings.PoolPort = atoi(port_text_box_num.getValue());
                strncpy(Settings.PoolPassword, password_text_box.getValue(), sizeof(Settings.PoolPassword));
                strncpy(Settings.BtcWallet, addr_text_box.getValue(), sizeof(Settings.BtcWallet));
                Settings.Timezone = atoi(time_text_box_num.getValue());
                // Serial.println(save_stats_to_nvs.getValue());
                Settings.saveStats = (strncmp(save_stats_to_nvs.getValue(), "T", 1) == 0);
                #if defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB)
                Settings.invertColors = (strncmp(invertColors.getValue(), "T", 1) == 0);
                #endif
                #if defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB)
                Settings.Brightness = atoi(brightness_text_box_num.getValue());
                #endif
                nvMem.saveConfig(&Settings);
                vTaskDelay(2000 / portTICK_PERIOD_MS);      
            }        
            ESP.restart();                            
        } 
    }
    
    //Conectado a la red Wifi
    if (WiFi.status() == WL_CONNECTED) {
        //tft.pushImage(0, 0, MinerWidth, MinerHeight, MinerScreen);
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());


        // Lets deal with the user config values

        // Copy the string value
        Settings.PoolAddress = pool_text_box.getValue();
        //strncpy(Settings.PoolAddress, pool_text_box.getValue(), sizeof(Settings.PoolAddress));
        Serial.print("PoolString: ");
        Serial.println(Settings.PoolAddress);

        //Convert the number value
        Settings.PoolPort = atoi(port_text_box_num.getValue());
        Serial.print("portNumber: ");
        Serial.println(Settings.PoolPort);

        // Copy the string value
        strncpy(Settings.PoolPassword, password_text_box.getValue(), sizeof(Settings.PoolPassword));
        Serial.print("poolPassword: ");
        Serial.println(Settings.PoolPassword);

        // Copy the string value
        strncpy(Settings.BtcWallet, addr_text_box.getValue(), sizeof(Settings.BtcWallet));
        Serial.print("btcString: ");
        Serial.println(Settings.BtcWallet);

        //Convert the number value
        Settings.Timezone = atoi(time_text_box_num.getValue());
        Serial.print("TimeZone fromUTC: ");
        Serial.println(Settings.Timezone);

        #if defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB)
        Settings.invertColors = (strncmp(invertColors.getValue(), "T", 1) == 0);
        Serial.print("Invert Colors: ");
        Serial.println(Settings.invertColors);        
        #endif

        #if defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB)
        Settings.Brightness = atoi(brightness_text_box_num.getValue());
        Serial.print("Brightness: ");
        Serial.println(Settings.Brightness);
        #endif

    }

    // Lets deal with the user config values

    // Copy the string value
    Settings.PoolAddress = pool_text_box.getValue();
    //strncpy(Settings.PoolAddress, pool_text_box.getValue(), sizeof(Settings.PoolAddress));
    Serial.print("PoolString: ");
    Serial.println(Settings.PoolAddress);

    //Convert the number value
    Settings.PoolPort = atoi(port_text_box_num.getValue());
    Serial.print("portNumber: ");
    Serial.println(Settings.PoolPort);

    // Copy the string value
    strncpy(Settings.PoolPassword, password_text_box.getValue(), sizeof(Settings.PoolPassword));
    Serial.print("poolPassword: ");
    Serial.println(Settings.PoolPassword);

    // Copy the string value
    strncpy(Settings.BtcWallet, addr_text_box.getValue(), sizeof(Settings.BtcWallet));
    Serial.print("btcString: ");
    Serial.println(Settings.BtcWallet);

    //Convert the number value
    Settings.Timezone = atoi(time_text_box_num.getValue());
    Serial.print("TimeZone fromUTC: ");
    Serial.println(Settings.Timezone);

    #ifdef ESP32_2432S028R
    Settings.invertColors = (strncmp(invertColors.getValue(), "T", 1) == 0);
    Serial.print("Invert Colors: ");
    Serial.println(Settings.invertColors);
    #endif

    // Save the custom parameters to FS
    if (shouldSaveConfig)
    {
        nvMem.saveConfig(&Settings);
        #if defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB)
         if (Settings.invertColors) ESP.restart();                
        #endif
        #if defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB)
        if (Settings.Brightness != 250) ESP.restart();
        #endif
    }
}

//----------------- REST API SERVER (PORT 80) --------------
WebServer apiServer(80);
static bool apiServerStarted = false;

static void setupApiServer() {
    if (apiServerStarted) return;

    apiServer.enableCORS(true);

    // GET /api/config - Retrieve current configuration
    apiServer.on("/api/config", HTTP_GET, []() {
        StaticJsonDocument<512> doc;
        doc["wallet"] = Settings.BtcWallet;
        doc["pool_url"] = Settings.PoolAddress;
        doc["pool_port"] = Settings.PoolPort;
        doc["pool_password"] = Settings.PoolPassword;
        doc["timezone"] = Settings.Timezone;
        doc["save_stats"] = Settings.saveStats;
        #if defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB)
        doc["invert_colors"] = Settings.invertColors;
        doc["brightness"] = Settings.Brightness;
        #endif

        String response;
        serializeJson(doc, response);
        apiServer.send(200, "application/json", response);
    });

    // POST /api/config - Update configuration dynamically
    apiServer.on("/api/config", HTTP_POST, []() {
        if (!apiServer.hasArg("plain")) {
            // Also accept URL-encoded form parameters
            bool modified = false;
            if (apiServer.hasArg("wallet")) {
                strncpy(Settings.BtcWallet, apiServer.arg("wallet").c_str(), sizeof(Settings.BtcWallet));
                Settings.BtcWallet[sizeof(Settings.BtcWallet) - 1] = '\0';
                modified = true;
            }
            if (apiServer.hasArg("pool_url")) {
                Settings.PoolAddress = apiServer.arg("pool_url");
                modified = true;
            }
            if (apiServer.hasArg("pool_port")) {
                Settings.PoolPort = apiServer.arg("pool_port").toInt();
                modified = true;
            }
            if (apiServer.hasArg("pool_password")) {
                strncpy(Settings.PoolPassword, apiServer.arg("pool_password").c_str(), sizeof(Settings.PoolPassword));
                Settings.PoolPassword[sizeof(Settings.PoolPassword) - 1] = '\0';
                modified = true;
            }
            if (apiServer.hasArg("timezone")) {
                Settings.Timezone = apiServer.arg("timezone").toInt();
                modified = true;
            }
            #if defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB)
            if (apiServer.hasArg("brightness")) {
                Settings.Brightness = apiServer.arg("brightness").toInt();
                modified = true;
            }
            #endif

            if (modified) {
                nvMem.saveConfig(&Settings);
                apiServer.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuration updated and saved. Restarting...\"}");
                delay(500);
                ESP.restart();
                return;
            }
            apiServer.send(400, "application/json", "{\"status\":\"error\",\"message\":\"No configuration fields provided\"}");
            return;
        }

        String body = apiServer.arg("plain");
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, body);
        if (error) {
            apiServer.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON payload\"}");
            return;
        }

        if (doc.containsKey("wallet")) {
            strncpy(Settings.BtcWallet, doc["wallet"].as<const char*>(), sizeof(Settings.BtcWallet));
            Settings.BtcWallet[sizeof(Settings.BtcWallet) - 1] = '\0';
        }
        if (doc.containsKey("pool_url")) {
            Settings.PoolAddress = doc["pool_url"].as<String>();
        }
        if (doc.containsKey("pool_port")) {
            Settings.PoolPort = doc["pool_port"].as<int>();
        }
        if (doc.containsKey("pool_password")) {
            strncpy(Settings.PoolPassword, doc["pool_password"].as<const char*>(), sizeof(Settings.PoolPassword));
            Settings.PoolPassword[sizeof(Settings.PoolPassword) - 1] = '\0';
        }
        if (doc.containsKey("timezone")) {
            Settings.Timezone = doc["timezone"].as<int>();
        }
        if (doc.containsKey("save_stats")) {
            Settings.saveStats = doc["save_stats"].as<bool>();
        }
        #if defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB)
        if (doc.containsKey("invert_colors")) {
            Settings.invertColors = doc["invert_colors"].as<bool>();
        }
        if (doc.containsKey("brightness")) {
            Settings.Brightness = doc["brightness"].as<int>();
        }
        #endif

        nvMem.saveConfig(&Settings);
        bool shouldRestart = doc.containsKey("restart") ? doc["restart"].as<bool>() : true;

        if (shouldRestart) {
            apiServer.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuration saved to flash. Rebooting miner now...\"}");
            delay(500);
            ESP.restart();
        } else {
            apiServer.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuration saved to flash.\"}");
        }
    });

    // GET /api/status - Retrieve live miner status & statistics
    apiServer.on("/api/status", HTTP_GET, []() {
        StaticJsonDocument<512> doc;
        doc["status"] = (mMonitor.NerdStatus == NM_hashing) ? "mining" : "connecting";
        doc["wallet"] = Settings.BtcWallet;
        doc["pool"] = Settings.PoolAddress;
        doc["free_heap"] = ESP.getFreeHeap();
        doc["uptime_ms"] = millis();
        doc["ip"] = WiFi.localIP().toString();

        String response;
        serializeJson(doc, response);
        apiServer.send(200, "application/json", response);
    });

    // POST /api/restart - Reboot ESP32
    apiServer.on("/api/restart", HTTP_POST, []() {
        apiServer.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Rebooting ESP32...\"}");
        delay(500);
        ESP.restart();
    });

    // Root info page with direct links and instructions
    apiServer.on("/", HTTP_GET, []() {
        String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>NerdMiner API</title>"
                      "<style>body{font-family:sans-serif;background:#0f172a;color:#e2e8f0;padding:2rem;max-width:600px;margin:auto;}"
                      "h1{color:#38bdf8;}a{color:#38bdf8;}pre{background:#1e293b;padding:1rem;border-radius:8px;overflow-x:auto;}"
                      ".btn{display:inline-block;background:#38bdf8;color:#0f172a;padding:8px 16px;border-radius:6px;text-decoration:none;font-weight:bold;margin-top:10px;}"
                      "</style></head><body><h1>⚡ NerdMiner REST API</h1>"
                      "<p>Wallet actual: <b>" + String(Settings.BtcWallet) + "</b></p>"
                      "<p>Pool: <b>" + Settings.PoolAddress + ":" + String(Settings.PoolPort) + "</b></p>"
                      "<h3>Endpoints disponibles:</h3>"
                      "<ul>"
                      "<li><a href='/api/config'>GET /api/config</a> - Ver configuración</li>"
                      "<li><a href='/api/status'>GET /api/status</a> - Ver estado del minero</li>"
                      "<li><b>POST /api/config</b> - Cambiar wallet / pool (JSON o form)</li>"
                      "<li><b>POST /api/restart</b> - Reiniciar minero</li>"
                      "</ul>"
                      "<h3>Cambiar wallet por cURL:</h3>"
                      "<pre>curl -X POST http://" + WiFi.localIP().toString() + "/api/config \\\n"
                      "  -H 'Content-Type: application/json' \\\n"
                      "  -d '{\"wallet\":\"bc1qfmmmv0cup5kqpfuvtuvqwaxw2t8jep3j2yyfqc\"}'</pre>"
                      "</body></html>";
        apiServer.send(200, "text/html", html);
    });

    apiServer.begin();
    apiServerStarted = true;
    Serial.println("REST API Server started on port 80");
}

//----------------- MAIN PROCESS WIFI MANAGER --------------
int oldStatus = 0;

void wifiManagerProcess() {

    wm.process(); // avoid delays() in loop when non-blocking and other long running code

    int newStatus = WiFi.status();
    if (newStatus != oldStatus) {
        if (newStatus == WL_CONNECTED) {
            Serial.println("CONNECTED - Current ip: " + WiFi.localIP().toString());
            setupApiServer();
        } else {
            Serial.print("[Error] - current status: ");
            Serial.println(newStatus);
        }
        oldStatus = newStatus;
    }

    if (newStatus == WL_CONNECTED) {
        if (!apiServerStarted) {
            setupApiServer();
        }
        apiServer.handleClient();
    }
}

