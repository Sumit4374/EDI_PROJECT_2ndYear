#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <LittleFS.h>
#include <WebServer.h>
#include "DHT.h"
#include <time.h>
#include <ESPmDNS.h>

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

int mqPin = 34;

int ldrPin = 35;

LiquidCrystal_I2C lcd(0x27, 16, 2);

const char* ssid = "Wifi name";
const char* password = "PassWord";

String apiKey = "API key";
String cityName = "Your City";

WebServer server(80);

float lastTemp, lastHum, lastPM25, lastPressure;
int lastCO2, lastLDR;
String lastTime, lastCity;
float lastLat, lastLon;

String httpGET(String url) {
    HTTPClient http;
    http.begin(url);
    int code = http.GET();
    if (code > 0) return http.getString();
    return "";
}

String getCurrentTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "Time Error";
    }
    
    char buf[40];
    strftime(buf, sizeof(buf), "%d-%m-%Y %H:%M:%S", &timeinfo);
    return String(buf);
}

String formatUnix(long u) {
    time_t raw = u + 19800;
    struct tm * ti = gmtime(&raw);

    char buf[40];
    strftime(buf, sizeof(buf), "%d-%m-%Y %H:%M:%S", ti);
    return String(buf);
}

void setup() {
    Serial.begin(115200);

    dht.begin();
    lcd.init();
    lcd.backlight();

    lcd.print("Connecting WiFi");

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(300);
        Serial.print(".");
    }

    lcd.clear();
    lcd.print("WiFi OK!");
    
    configTime(19800, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("Syncing time with NTP...");
    delay(2000);
    
    if (!MDNS.begin("esp32")) {
        Serial.println("MDNS Failed");
    } else {
        Serial.println("MDNS OK: http://esp32.local/");
    }

    MDNS.addService("http", "tcp", 80);

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
    } else {
        Serial.println("LittleFS Mounted");
    }

    server.serveStatic("/", LittleFS, "/index.html");
    server.serveStatic("/style.css", LittleFS, "/style.css");

    server.on("/data", []() {
        String json = "{";
        json += "\"temp\":" + String(lastTemp) + ",";
        json += "\"hum\":" + String(lastHum) + ",";
        json += "\"pressure\":" + String(lastPressure) + ",";
        json += "\"pm25\":" + String(lastPM25) + ",";
        json += "\"co2\":" + String(lastCO2) + ",";
        json += "\"ldr\":" + String(lastLDR) + ",";
        json += "\"time\":\"" + lastTime + "\",";
        json += "\"city\":\"" + lastCity + "\",";
        json += "\"lat\":" + String(lastLat, 6) + ",";
        json += "\"lon\":" + String(lastLon, 6);
        json += "}";
        server.send(200, "application/json", json);
    });

    server.begin();
    Serial.println("Server started");
    
}

void loop() {

    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    int mqRaw = analogRead(mqPin);
    int ldrRaw = analogRead(ldrPin);

    String weatherURL =
        "http://api.openweathermap.org/data/2.5/weather?q=" + cityName +
        "&appid=" + apiKey + "&units=metric";

    String weatherPayload = httpGET(weatherURL);

    float pressure = 0;
    float lat_dynamic = 0;
    float lon_dynamic = 0;
    long unixTime = 0;
    String city = "";

    if (weatherPayload.length() > 20) {
        JsonDocument doc;
        doc.clear();
        deserializeJson(doc, weatherPayload);

        pressure = doc["main"]["pressure"];
        unixTime = doc["dt"];
        city = doc["name"].as<String>();

        lat_dynamic = doc["coord"]["lat"];
        lon_dynamic = doc["coord"]["lon"];
    }

    String pmURL =
        "http://api.openweathermap.org/data/2.5/air_pollution?lat=" +
        String(lat_dynamic) + "&lon=" + String(lon_dynamic) +
        "&appid=" + apiKey;

    String pmPayload = httpGET(pmURL);

    float pm25 = 0;

    if (pmPayload.length() > 20) {
        DynamicJsonDocument doc(2048);
        deserializeJson(doc, pmPayload);

        pm25 = doc["list"][0]["components"]["pm2_5"];
    }

    String formattedTime = getCurrentTime();

    lastTemp = temp;
    lastHum = hum;
    lastPressure = pressure;
    lastPM25 = pm25;
    lastCO2 = mqRaw;
    lastLDR = ldrRaw;
    lastTime = formattedTime;
    lastCity = city;
    lastLat = lat_dynamic;
    lastLon = lon_dynamic;

    Serial.println("========== SENSOR READINGS ==========");
    Serial.print("City: ");
    Serial.println(city);
    
    Serial.print("Coordinates: ");
    Serial.print(lat_dynamic, 6);
    Serial.print(", ");
    Serial.println(lon_dynamic, 6);
    
    Serial.print("Temperature: ");
    Serial.print(temp);
    Serial.println(" °C");
    
    Serial.print("Humidity: ");
    Serial.print(hum);
    Serial.println(" %");
    
    Serial.print("Pressure: ");
    Serial.print(pressure);
    Serial.println(" hPa");
    
    Serial.print("PM2.5: ");
    Serial.print(pm25);
    Serial.println(" μg/m³");
    
    Serial.print("CO2 (MQ135 Raw): ");
    Serial.println(mqRaw);
    
    Serial.print("Light Intensity (LDR): ");
    Serial.println(ldrRaw);
    
    Serial.print("Time: ");
    Serial.println(formattedTime);
    Serial.println("=====================================\n");

    static int displayState = 0;
    
    lcd.clear();
    
    if (displayState == 0) {
        lcd.setCursor(0, 0);
        lcd.print("Temp: ");
        lcd.print(temp, 1);
        lcd.print(" C");
        
        lcd.setCursor(0, 1);
        lcd.print("Hum: ");
        lcd.print(hum, 1);
        lcd.print(" %");
    } 
    else if (displayState == 1) {
        lcd.setCursor(0, 0);
        lcd.print("PM2.5: ");
        lcd.print(pm25, 1);
        
        lcd.setCursor(0, 1);
        lcd.print("Press: ");
        lcd.print(pressure, 0);
    }
    else if (displayState == 2) {
        lcd.setCursor(0, 0);
        lcd.print("CO2: ");
        lcd.print(mqRaw);
        
        lcd.setCursor(0, 1);
        lcd.print("Light: ");
        lcd.print(ldrRaw);
    }
    else if (displayState == 3) {
        lcd.setCursor(0, 0);
        lcd.print("Location:");
        
        lcd.setCursor(0, 1);
        lcd.print(city);
    }
    else if (displayState == 4) {
        lcd.setCursor(0, 0);
        lcd.print("Lat: ");
        lcd.print(lat_dynamic, 4);
        
        lcd.setCursor(0, 1);
        lcd.print("Lon: ");
        lcd.print(lon_dynamic, 4);
    }
    else if (displayState == 5) {
        lcd.setCursor(0, 0);
        lcd.print("Time (IST):");
        
        lcd.setCursor(0, 1);
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char timeBuf[20];
            strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &timeinfo);
            lcd.print(timeBuf);
        } else {
            lcd.print("Syncing...");
        }
    }
    
    displayState = (displayState + 1) % 6;

    server.handleClient();

    delay(3000);
}
