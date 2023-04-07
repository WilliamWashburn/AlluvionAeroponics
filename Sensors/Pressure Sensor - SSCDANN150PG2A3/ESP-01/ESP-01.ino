#include "credentials.h" //all the secret stuff. You'll need to create a "credentials.h" file in the folder that this file is in and copy and paste the following in. Or just uncomment below and remove this include statement
/*
#define WIFI_NAME "XXXXXX"
#define WIFI_PASS "XXXXXX"
#define MQTT_PASS "XXXXXXX"
*/

//This should be everything that you need to change to add a new sensor
const char* ssid = WIFI_NAME;
const char* password = WIFI_PASS;
const char* mqtt_pass = MQTT_PASS;
const char whichSensor[] = "Tank_3_Pressure_Sensor"; //change to match which row you are using

#include "wifiFunctions.h"
#include "pressureFunctions.h"

long readPressurePeriod = 5;  //every 5 minutes
char sensorReadingJSON[50];

int SDA_pin = 2;
int SCL_pin = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_pin, SCL_pin);
  setup_wifi();
  setup_mqtt();
  client.setServer(mqtt_server, 1883);
}

void loop() {
  static long prevTime = -readPressurePeriod * 60L * 1000L;
  if (millis() - prevTime > 2000) {

    Serial.println("Reading pressure");
    readPressure();

    strcpy(sensorReadingJSON, "{\"pressure\":\"");
    strcat(sensorReadingJSON, String(pressurePSI).c_str());
    strcat(sensorReadingJSON, "\",\"temperature\":\"");
    strcat(sensorReadingJSON, String(tempF).c_str());
    strcat(sensorReadingJSON, "\"}");
    Serial.println("sensorReadingJSON: " + String(sensorReadingJSON));
    client.publish(sensorReadingTopic, sensorReadingJSON);

    prevTime = millis();
  }

  if (!client.connected()) {
    reconnect();
  }
  client.loop(); //mqtt
}