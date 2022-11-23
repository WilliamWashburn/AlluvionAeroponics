#include <WiFi.h>
#include "credentials.h"

const char* ssid = mySSID;
const char* password = myPASSWORD;

bool checkWifiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  return true;
}

bool connectToWifi() {
  Serial.println("Connecting to wifi: " + String(ssid));
  WiFi.begin(ssid, password);
  
  long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 60000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println(" Failed:(");
  return false;
}