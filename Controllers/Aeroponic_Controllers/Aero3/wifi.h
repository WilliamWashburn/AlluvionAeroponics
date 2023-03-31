#include <WiFi.h>

void checkWiFiConnection() {
  static int connectCount = 0;
  if (!WiFi.isConnected()) {
    WiFi.reconnect();
    connectCount++;
    Serial.println("Connection lost. Reconnecting");
    //if we had to reconnect more than 10 times and we have been alive for more than 10 minutes (so that we arent constantly restarting)
    if (connectCount > 10 && millis() > 10*60*1000) {
      Serial.println("Reconnected too many times. Restarting");
      delay(2000);
      ESP.restart(); //if we have had to reconnect a bunch, just restart the controller
    }
  }
}