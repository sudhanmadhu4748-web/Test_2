#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "DHT.h"

#define DHTPIN 12         
#define DHTTYPE DHT11     

#define LIGHT_RELAY 42    
#define FAN_RELAY 5      

const char* ssid = "workspace";         
const char* password = "passcode"; 

DHT dht(DHTPIN, DHTTYPE);
AsyncWebServer server(80);

float temperature = 0.0;
float humidity = 0.0;
bool lightState = false;
bool fanState = false;
String lastLightEvent = "N/A";
String lastFanEvent = "N/A";

float tempThreshold = 30.0;

String getFormattedTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
  return String(buffer);
}

void setup() {
  Serial.begin(115200);
  
  pinMode(LIGHT_RELAY, OUTPUT);
  pinMode(FAN_RELAY, OUTPUT);

  // Initialize relays to OFF state (HIGH for LOW-trigger relay)
  digitalWrite(LIGHT_RELAY, HIGH); 
  digitalWrite(FAN_RELAY, HIGH);

  dht.begin();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  configTime(19800, 0, "pool.ntp.org", "time.nist.gov");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<!DOCTYPE html><html><head><title>Smart Room Control</title>";
    html += "<style>body{font-family:Arial;}button{padding:10px;margin:5px;}</style></head><body>";
    
    html += "<h2>Room Environment</h2>";
    html += "<p>Temperature: " + String(temperature) + " °C</p>";
    html += "<p>Humidity: " + String(humidity) + " %</p>";
    
    html += "<h2>Light Control</h2>";
    html += "<p>Light is " + String(lightState ? "ON" : "OFF") + "</p>";
    html += "<button onclick=\"location.href='/light/on'\">Turn ON</button>";
    html += "<button onclick=\"location.href='/light/off'\">Turn OFF</button>";
    html += "<p>Last Light Event: " + lastLightEvent + "</p>";

    html += "<h2>Fan Control</h2>";
    html += "<p>Fan is " + String(fanState ? "ON" : "OFF") + "</p>";
    html += "<button onclick=\"location.href='/fan/on'\">Turn ON</button>";
    html += "<button onclick=\"location.href='/fan/off'\">Turn OFF</button>";
    html += "<p>Last Fan Event: " + lastFanEvent + "</p>";

    html += "<h2>Auto Fan Logic</h2>";
    html += "<p>Fan turns ON automatically if Temp > " + String(tempThreshold) + " °C</p>";

    html += "<meta http-equiv='refresh' content='5'>";
    html += "</body></html>";

    request->send(200, "text/html", html);
  });

  // Light Control Routes
  server.on("/light/on", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(LIGHT_RELAY, LOW);  
    lightState = true;
    lastLightEvent = getFormattedTime();
    request->redirect("/");
  });

  server.on("/light/off", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(LIGHT_RELAY, HIGH);  
    lightState = false;
    lastLightEvent = getFormattedTime();
    request->redirect("/");
  });

  // Fan Control Routes
  server.on("/fan/on", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(FAN_RELAY, LOW);  
    fanState = true;
    lastFanEvent = getFormattedTime();
    request->redirect("/");
  });

  server.on("/fan/off", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(FAN_RELAY, HIGH);  
    fanState = false;
    lastFanEvent = getFormattedTime();
    request->redirect("/");
  });

  server.begin();
}

void loop() {
  static unsigned long lastRead = 0;
  if (millis() - lastRead > 2000) {
    lastRead = millis();
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    
    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Failed to read DHT11");
      return;
    }

    Serial.printf("Temp: %.2f C, Humidity: %.2f %%\n", temperature, humidity);

    // Auto Fan Logic
    if (temperature > tempThreshold && !fanState) {
      digitalWrite(FAN_RELAY, LOW);  
      fanState = true;
      lastFanEvent = getFormattedTime();
      Serial.println("Auto Fan ON due to high temp");
    }
    else if (temperature <= tempThreshold && fanState) {
      digitalWrite(FAN_RELAY, HIGH);  
      fanState = false;
      lastFanEvent = getFormattedTime();
      Serial.println("Auto Fan OFF, temp normal");
    }
  }
}
