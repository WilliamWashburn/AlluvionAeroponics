#include "credentials.h"  //you need to create this file and #define mySSID and myPASSWORD. or comment this out and fill in below
#include "ntpFunctions.h"
#include "mqttFunctions.h"

#include <TimeLib.h>
#include <TimeAlarms.h>

#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel pixels(1, 0, NEO_GRB);

AlarmId id;

const char* ssid = mySSID;
const char* password = myPASSWORD;

int lightPins[] = { 4, 26, 25, 13 };

AlarmID_t ebbNFlowAlarmOnID;
AlarmID_t ebbNFlowAlarmOffID;
AlarmID_t aero2AlarmOnID;
AlarmID_t aero2AlarmOffID;

void setup() {
  //CONNECT TO SERIAL
  Serial.begin(115200);
  while (!Serial)
    ;  // wait for Arduino Serial Monitor

  Serial.println();
  Serial.println();

  //Set pinMode and turn off
  for (int i = 0; i < 4; i++) {
    pinMode(lightPins[i], OUTPUT);
    digitalWrite(lightPins[i], LOW);
  }

  //START NEOPIXEL
  pixels.begin();  // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.clear();  // Set all pixel colors to 'off'

  //CONNECT TO WIFI
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  int startTime = millis();
  while (millis() - startTime < 60000 && WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    pixels.setPixelColor(0, pixels.Color(0, 0, 10));  //set blue while connecting
    pixels.show();                                    // Send the updated pixel colors to the hardware.
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Failed to connect to wifi");
    pixels.setPixelColor(0, pixels.Color(10, 0, 0));
    pixels.show();  // Send the updated pixel colors to the hardware.
  }

  //CONNECT TO NTP
  configTime(0, 0, NTP_SERVER);  // See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv for Timezone codes for your region
  setenv("TZ", TZ_INFO, 1);
  getNTPtime(10);
  setTime(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, timeinfo.tm_mday, timeinfo.tm_mon + 1, 1900 + timeinfo.tm_year);  // tm_mon is 0-11 so add 1, tm_year is years since 1900 so add to 1900
  Serial.println("Time: " + String(hour()) + ":" + String(minute()) + ":" + String(second()));
  Serial.println("Date: " + String(month()) + "/" + String(day()) + "/" + String(year()));

  //CONNECT TO MQTT BROKER
  mqttClient.onMessage(onMqttMessage);  // set the message receive callback
  if (!connectToBroker()) {
    pixels.setPixelColor(0, pixels.Color(20, 0, 0));  //set to red
    pixels.show();                                    // Send the updated pixel colors to the hardware.
  }

  if(mqttClient.connected() && WiFi.status() == WL_CONNECTED){
    pixels.setPixelColor(0, pixels.Color(0, 10, 0));
    pixels.show();  // Send the updated pixel colors to the hardware.
  }

  //SET ALARMS
  //ill need to get the ID of the alarm to be able to update it
  ebbNFlowAlarmOnID = Alarm.alarmRepeat(1, 00, 0, turnEbbNFlowLightsOn);     // 1:00am every day
  ebbNFlowAlarmOffID = Alarm.alarmRepeat(17, 00, 0, turnEbbNFlowLightsOff);  // 5:00pm every day (1am -> 5pm = 18 hours)
  aero2AlarmOnID = Alarm.alarmRepeat(1, 00, 0, turnRow2LightsOn);            // 1:00am every day
  aero2AlarmOffID = Alarm.alarmRepeat(17, 00, 0, turnRow2LightsOff);         // 5:00pm every day (1am -> 5pm = 18 hours)

  resyncToSchedule();
}

void resyncToSchedule() {
  //TURN ON LIGHTS IF NEEDED
  //might have to change this logic if changing the time on/off
  if (secondsAfterMidnight() > Alarm.read(ebbNFlowAlarmOnID) && secondsAfterMidnight() < Alarm.read(ebbNFlowAlarmOffID)) {
    turnEbbNFlowLightsOn();
  } else {
    turnEbbNFlowLightsOff();
  }
  if (secondsAfterMidnight() > Alarm.read(aero2AlarmOnID) && secondsAfterMidnight() < Alarm.read(aero2AlarmOffID)) {
    turnRow2LightsOn();
  } else {
    turnRow2LightsOff();
  }
}

int secondsAfterMidnight() {
  return hour() * 3600 + minute() * 60 + second();
}

long convertTimeToSecondsAfterMidnight(int hourTime, int minuteTime, int secondTime = 0) {
  return hourTime * 3600L + minuteTime * 60L + secondTime;
}

long convertTimeToSecondsAfterMidnight(char hourTime[], char minuteTime[], char secondTime[] = "0") {
  return atoi(hourTime) * 3600L + atoi(minuteTime) * 60L + atoi(secondTime);
}

void loop() {
  checkWifiConnection();

  if (!mqttClient.connected()) {
    pixels.setPixelColor(0, pixels.Color(20, 0, 0));  //set to red
    pixels.show();                                    // Send the updated pixel colors to the hardware.

    if (connectToBroker()) {
      pixels.setPixelColor(0, pixels.Color(0, 20, 0));  //set to red
      pixels.show();
    }
  }

  Alarm.delay(0);  //needed to service alarms

  // call poll() regularly to allow the library to receive MQTT messages and
  // send MQTT keep alives which avoids being disconnected by the broker
  mqttClient.poll();
}

void turnEbbNFlowLightsOn() {
  digitalWrite(lightPins[0], HIGH);
  //Publish to MQTT broker so we can log the event
  mqttClient.beginMessage("Desoto/Lights/EbbNFlow/state");
  mqttClient.print("{\"state\":\"on\"}");
  mqttClient.endMessage();
}
void turnEbbNFlowLightsOff() {
  digitalWrite(lightPins[0], LOW);
  mqttClient.beginMessage("Desoto/Lights/EbbNFlow/state");
  mqttClient.print("{\"state\":\"off\"}");
  mqttClient.endMessage();
}

bool ebbNFlowLightsState() {
  return digitalRead(lightPins[0]);
}

//need to update to match row 2
void turnRow1LightsOn() {
  digitalWrite(lightPins[1], HIGH);
}
void turnRow1LightsOff() {
  digitalWrite(lightPins[1], LOW);
}

void turnRow2LightsOn() {
  digitalWrite(lightPins[2], HIGH);
  mqttClient.beginMessage("Desoto/Lights/Aero2/state");
  mqttClient.print("{\"state\":\"on\"}");
  mqttClient.endMessage();
}
void turnRow2LightsOff() {
  digitalWrite(lightPins[2], LOW);
  mqttClient.beginMessage("Desoto/Lights/Aero2/state");
  mqttClient.print("{\"state\":\"off\"}");
  mqttClient.endMessage();
}

bool row2LightsState() {
  return digitalRead(lightPins[2]);
}

//need to update to match row 2
void turnRow3LightsOn() {
  digitalWrite(lightPins[3], HIGH);
}
void turnRow3LightsOff() {
  digitalWrite(lightPins[3], LOW);
}

void onMqttMessage(int messageSize) {

  String topic = mqttClient.messageTopic();
  if (topic == "Desoto/Lights/EbbNFlow/command") {
    Serial.println("Should toggle EbbNFlow lights");
    readMessage();
    Serial.println("Received message: " + String(mqttMessage));
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      if (!ebbNFlowLightsState()) {  //if not on
        turnEbbNFlowLightsOn();
      }
    } else if (strcmp(mqttMessage, "{\"state\":\"off\"}") == 0) {
      if (ebbNFlowLightsState()) {  //if on
        turnEbbNFlowLightsOff();
      }
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(mqttMessage);
    }
  } else if (topic == "Desoto/Lights/Aero2/command") {
    Serial.println("Should toggle Aero2 lights");
    readMessage();
    Serial.println("Received message: " + String(mqttMessage));
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      if (!row2LightsState()) {  //if not on
        turnRow2LightsOn();
      }
    } else if (strcmp(mqttMessage, "{\"state\":\"off\"}") == 0) {
      if (row2LightsState()) {  //if on
        turnRow2LightsOff();
      }
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(mqttMessage);
    }
  } else if (topic == "Desoto/Lights/EbbNFlow/state") {
    //do nothing
  } else if (topic == "Desoto/Lights/Aero2/state") {
    //do nothing
  } else if (topic == "Desoto/Lights/resync") {
    resyncToSchedule();
  } else if (topic == "Desoto/Lights/Aero2/turnOffTime") {
    Serial.println("Should update Aero2 light turn off timing");

    //read message
    readMessage();
    Serial.println(mqttMessage);

    //parse
    char hourOff[10];
    char minOff[10];
    strcpy(hourOff, strtok(mqttMessage, ":"));
    strcpy(minOff, strtok(NULL, ":"));
    Serial.println("time parsed - " + String(hourOff) + ":" + String(minOff));

    //update time
    Alarm.write(aero2AlarmOffID, convertTimeToSecondsAfterMidnight(hourOff, minOff));
  } else if (topic == "Desoto/Lights/Aero2/turnOnTime") {
    Serial.println("Should update Aero2 light turn on timing");

    readMessage();
    Serial.println("Received message: " + String(mqttMessage));

    //parse
    char hourOff[10];
    char minOff[10];
    strcpy(hourOff, strtok(mqttMessage, ":"));
    strcpy(minOff, strtok(NULL, ":"));
    Serial.println("time parsed - " + String(hourOff) + ":" + String(minOff));

    //update time
    Alarm.write(aero2AlarmOnID, convertTimeToSecondsAfterMidnight(hourOff, minOff));
  } else {
    Serial.print("Update not recognized: ");
    Serial.println(topic);
  }
}

void checkWifiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Disconnected from wifi!");

    pixels.setPixelColor(0, pixels.Color(10, 0, 0));
    pixels.show();  // Send the updated pixel colors to the hardware.

    WiFi.begin(ssid, password);

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      pixels.setPixelColor(0, pixels.Color(0, 10, 0));
      pixels.show();  // Send the updated pixel colors to the hardware.
    }
  }
}