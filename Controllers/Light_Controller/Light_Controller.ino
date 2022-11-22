#include "credentials.h"  //you need to create this file and #define mySSID and myPASSWORD. or comment this out and fill in below
#include "ntpFunctions.h"
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include <TimeLib.h>
#include <TimeAlarms.h>

#include <ArduinoMqttClient.h>

#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel pixels(1, 0, NEO_GRB);

AlarmId id;

const char* ssid = mySSID;
const char* password = myPASSWORD;

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

int lightPins[] = { 4, 26, 25, 13 };

const char broker[] = "homeassistant.local";
int port = 1883;
const char topic[] = "Desoto/Lights/#";

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
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    pixels.setPixelColor(0, pixels.Color(0, 10, 0));
    pixels.show();  // Send the updated pixel colors to the hardware.
  } else {
    Serial.println("Failed to connect to wifi");
    pixels.setPixelColor(0, pixels.Color(10, 0, 0));
    pixels.show();  // Send the updated pixel colors to the hardware.
  }

  //CONNECT TO NTP
  configTime(0, 0, NTP_SERVER);  // See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv for Timezone codes for your region
  setenv("TZ", TZ_INFO, 1);
  getNTPtime(10);
  setTime(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, timeinfo.tm_mon + 1, timeinfo.tm_mday, 1900 + timeinfo.tm_year);  // tm_mon is 0-11 so add 1, tm_year is years since 1900 so add to 1900
  Serial.println("Time: " + String(hour()) + ":" + String(minute()) + ":" + String(second()));
  Serial.println("Date: " + String(month()) + "/" + String(day()) + "/" + String(year()));

  //CONNECT TO MQTT BROKER
  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);
  mqttClient.setUsernamePassword(mqttUser, mqttPass);
  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());

    while (1)
      ;
  }
  Serial.println("You're connected to the MQTT broker!");
  Serial.println();
  // set the message receive callback
  mqttClient.onMessage(onMqttMessage);
  Serial.print("Subscribing to topic: ");
  Serial.println(topic);
  Serial.println();
  // subscribe to a topic
  mqttClient.subscribe(topic);

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
  // // we received a message, print out the topic and contents
  // Serial.println("Received a message with topic '");
  // Serial.print(mqttClient.messageTopic());
  // Serial.print("', length ");
  // Serial.print(messageSize);
  // Serial.println(" bytes:");

  // // use the Stream interface to print the contents
  // while (mqttClient.available()) {
  //   Serial.print((char)mqttClient.read());
  // }
  // Serial.println();

  // Serial.println();

  String topic = mqttClient.messageTopic();
  if (topic == "Desoto/Lights/EbbNFlow/command") {
    Serial.println("Should toggle EbbNFlow lights");
    char message[50];
    int inx = 0;
    while (mqttClient.available()) {
      message[inx] = (char)mqttClient.read();
      inx++;
      // Serial.print((char)mqttClient.read());
    }
    message[inx] = '\0';  //null terminate
    Serial.println(message);
    if (strcmp(message, "{\"state\":\"on\"}") == 0) {
      if (!ebbNFlowLightsState()) {  //if not on
        turnEbbNFlowLightsOn();
      }
    } else if (strcmp(message, "{\"state\":\"off\"}") == 0) {
      if (ebbNFlowLightsState()) {  //if on
        turnEbbNFlowLightsOff();
      }
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/Lights/Aero2/command") {
    Serial.println("Should toggle Aero2 lights");
    char message[50];
    int inx = 0;
    while (mqttClient.available()) {
      message[inx] = (char)mqttClient.read();
      inx++;
      // Serial.print((char)mqttClient.read());
    }
    message[inx] = '\0';  //null terminate
    Serial.println(message);
    if (strcmp(message, "{\"state\":\"on\"}") == 0) {
      if (!row2LightsState()) {  //if not on
        turnRow2LightsOn();
      }
    } else if (strcmp(message, "{\"state\":\"off\"}") == 0) {
      if (row2LightsState()) {  //if on
        turnRow2LightsOff();
      }
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
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
    char message[50];
    int inx = 0;
    while (mqttClient.available()) {
      message[inx] = (char)mqttClient.read();
      inx++;
      // Serial.print((char)mqttClient.read());
    }
    message[inx] = '\0';  //null terminate

    Serial.println(message);

    //parse
    char hourOff[10];
    char minOff[10];
    strcpy(hourOff, strtok(message, ":"));
    strcpy(minOff, strtok(NULL, ":"));
    Serial.println("time parsed - " + String(hourOff) + ":" + String(minOff));

    //update time
    Alarm.write(aero2AlarmOffID, convertTimeToSecondsAfterMidnight(hourOff, minOff));
  } else if (topic == "Desoto/Lights/Aero2/turnOnTime") {
    Serial.println("Should update Aero2 light turn on timing");

    //read message
    char message[50];
    int inx = 0;
    while (mqttClient.available()) {
      message[inx] = (char)mqttClient.read();
      inx++;
      // Serial.print((char)mqttClient.read());
    }
    message[inx] = '\0';  //null terminate

    Serial.println("Received message: " + String(message));

    //parse
    char hourOff[10];
    char minOff[10];
    strcpy(hourOff, strtok(message, ":"));
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

void checkHomeassistantConnection() {
  if (!mqttClient.connected()) {
    if (!mqttClient.connect(broker, port)) {
      Serial.print("MQTT connection failed! Error code = ");
      Serial.println(mqttClient.connectError());
    }
    Serial.println("You're connected to the MQTT broker!");
    Serial.println();
    // set the message receive callback
    mqttClient.onMessage(onMqttMessage);
    Serial.print("Subscribing to topic: ");
    Serial.println(topic);
    Serial.println();
    // subscribe to a topic
    mqttClient.subscribe(topic);
  }
}