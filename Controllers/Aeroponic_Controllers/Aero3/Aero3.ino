#include "credentials.h"  //you need to create this file and #define mySSID and myPASSWORD. or comment this out and fill in below
#include "ntpFunctions.h"

#include <TimeLib.h>
#include <TimeAlarms.h>

#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoMqttClient.h>

int solenoidPins[] = { 15, 33, 27 };
int feedbackPin = 32;
int nbrOfPins = 3;

const char* ssid = mySSID;
const char* password = myPASSWORD;

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = "homeassistant.local";
int port = 1883;
const char topic[] = "Desoto/Aero3/#";                      //for reading from
const char sprayTopic1[] = "Desoto/Aero3/solenoid1/state";  //for updating when sprayed
const char sprayTopic2[] = "Desoto/Aero3/solenoid2/state";
const char sprayTopic3[] = "Desoto/Aero3/solenoid3/state";

//for turning off individual zones
bool cycleSolenoid1 = true;
bool cycleSolenoid2 = true;
bool cycleSolenoid3 = true;

int wateringDurations[] = { 7, 7, 7 };

AlarmId wateringAlarmID;
long wateringDelay = 60;  //default

void setup() {

  Serial.begin(115200);

  for (int i = 0; i < nbrOfPins; i++) {
    pinMode(solenoidPins[i], OUTPUT);
    pinMode(feedbackPin, INPUT);
    digitalWrite(solenoidPins[i], LOW);
  }

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
  } else {
    Serial.println("Failed to connect to wifi");
  }

  //CONNECT TO NTP
  configTime(0, 0, NTP_SERVER);  // See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv for Timezone codes for your region
  setenv("TZ", TZ_INFO, 1);
  getNTPtime(10);
  setTime(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, timeinfo.tm_mday, timeinfo.tm_mon + 1, 1900 + timeinfo.tm_year);  // tm_mon is 0-11 so add 1, tm_year is years since 1900 so add to 1900
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
  wateringAlarmID = Alarm.timerRepeat(wateringDelay, cycleThroughSolenoids);
  // cycleThroughSolenoids();
  // Serial.println("Value: " + String(Alarm.read(wateringAlarmID)));
}

void loop() {
  Alarm.delay(0);  //needed to service alarms

  // call poll() regularly to allow the library to receive MQTT messages and
  // send MQTT keep alives which avoids being disconnected by the broker
  mqttClient.poll();
}

void cycleThroughSolenoids() {
  //Send update to MQTT
  mqttClient.beginMessage("Desoto/Aero3/watering/lastTriggered");
  mqttClient.print("hello!");
  mqttClient.endMessage();

  for (int i = 0; i < nbrOfPins; i++) {
    if (i == 0 && cycleSolenoid1 == false) {
      Serial.println("Dont cycle solenoid 1");
    } else if (i == 1 && cycleSolenoid2 == false) {
      Serial.println("Dont cycle solenoid 2");
    } else if (i == 2 && cycleSolenoid3 == false) {
      Serial.println("Dont cycle solenoid 3");
    } else {
      digitalWrite(solenoidPins[i], HIGH);
      delayWhileSpraying(wateringDurations[i] * 1000, i + 1);
      digitalWrite(solenoidPins[i], LOW);
      customDelay(1000);
    }
  }
}

void delayWhileSpraying(long timeToDelayMilli, int solenoidSpraying) {
  long startTime = millis();
  static bool updateMessageSent = false;  //flag so that once it starts spraying, it only sends 1 mqtt message
  while (millis() - startTime < timeToDelayMilli) {
    //all of these functions need to be quick and none blocking.
    Alarm.delay(0);     //service alarms
    mqttClient.poll();  //check for mqtt messages

    if (!updateMessageSent) {
      if (updateIfSpraying(solenoidSpraying)) {
        updateMessageSent = true;
        Serial.println("updateMessageSent set true");
      }
    }
  }
  if (!updateMessageSent) {
    Serial.println("No spraying detected!");
    beginMessageForSolenoidFeedback(solenoidSpraying);
    mqttClient.print("No spraying detected!"); //if supposed to spray but doesn't
    mqttClient.endMessage();
  } else {
    beginMessageForSolenoidFeedback(solenoidSpraying);
    mqttClient.print("Not spraying"); //when the spraying stops
    mqttClient.endMessage();
    updateMessageSent = false;  //reset flag after delay
    Serial.println("updateMessageSent reset to false");
    Serial.println();
  }
}

void customDelay(long timeToDelayMilli) {
  long startTime = millis();
  while (millis() - startTime < timeToDelayMilli) {
    Alarm.delay(0);     //service alarms
    mqttClient.poll();  //check for mqtt messages
  }
}

void beginMessageForSolenoidFeedback(int solenoidSpraying) {
  if (solenoidSpraying == 1) {
    mqttClient.beginMessage(sprayTopic1);
  } else if (solenoidSpraying == 2) {
    mqttClient.beginMessage(sprayTopic2);
  } else if (solenoidSpraying == 3) {
    mqttClient.beginMessage(sprayTopic3);
  }
}

bool updateIfSpraying(int solenoidSpraying) {
  if (digitalRead(feedbackPin)) {  //if spraying
    beginMessageForSolenoidFeedback(solenoidSpraying);
    mqttClient.print("Spraying");
    mqttClient.endMessage();
    Serial.println("mqtt message sent");
    return true;
  }
  return false;
}

//TURN ON SOLENOID
void turnOnSolenoid1() {
  digitalWrite(solenoidPins[0], HIGH);
}
void turnOnSolenoid2() {
  digitalWrite(solenoidPins[1], HIGH);
}
void turnOnSolenoid3() {
  digitalWrite(solenoidPins[2], HIGH);
}


//TURN OFF SOLENOID
void turnOffSolenoid1() {
  digitalWrite(solenoidPins[0], LOW);
}
void turnOffSolenoid2() {
  digitalWrite(solenoidPins[1], LOW);
}
void turnOffSolenoid3() {
  digitalWrite(solenoidPins[2], LOW);
}

//MQTT
void onMqttMessage(int messageSize) {
  String topic = mqttClient.messageTopic();
  if (topic == "Desoto/Aero3/solenoid1/command") {
    Serial.println("Should toggle solenoid1");
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
      turnOnSolenoid1();
    } else if (strcmp(message, "{\"state\":\"off\"}") == 0) {
      turnOffSolenoid1();
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/Aero3/solenoid2/command") {
    Serial.println("Should toggle solenoid2");
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
      turnOnSolenoid2();
    } else if (strcmp(message, "{\"state\":\"off\"}") == 0) {
      turnOffSolenoid2();
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/Aero3/solenoid3/command") {
    Serial.println("Should toggle solenoid 3");
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
      turnOnSolenoid3();
    } else if (strcmp(message, "{\"state\":\"off\"}") == 0) {
      turnOffSolenoid3();
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/Aero3/watering/solenoid1/command") {
    Serial.println("Should toggle solenoid 1 water cycle");
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
      Serial.println("solenoid 1 cycle is on!");
      cycleSolenoid1 = true;
    } else if (strcmp(message, "{\"state\":\"off\"}") == 0) {
      Serial.println("solenoid 1 cycle is off!");
      cycleSolenoid1 = false;
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/Aero3/watering/solenoid2/command") {
    Serial.println("Should toggle solenoid 2 water cycle");
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
      Serial.println("solenoid 2 cycle is on!");
      cycleSolenoid2 = true;
    } else if (strcmp(message, "{\"state\":\"off\"}") == 0) {
      Serial.println("solenoid 2 cycle is off!");
      cycleSolenoid2 = false;
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/Aero3/watering/solenoid3/command") {
    Serial.println("Should toggle solenoid 3 water cycle");
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
      Serial.println("solenoid 3 cycle is on!");
      cycleSolenoid3 = true;
    } else if (strcmp(message, "{\"state\":\"off\"}") == 0) {
      Serial.println("solenoid 3 cycle is off!");
      cycleSolenoid3 = false;
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/Aero3/watering/solenoid1/duration") {
    Serial.println("Should update solenoid 1 water duration");
    char message[50];
    int inx = 0;
    while (mqttClient.available()) {
      message[inx] = (char)mqttClient.read();
      inx++;
      // Serial.print((char)mqttClient.read());
    }
    message[inx] = '\0';  //null terminate
    Serial.println(message);

    wateringDurations[0] = atoi(message);

  } else if (topic == "Desoto/Aero3/watering/solenoid2/duration") {
    Serial.println("Should update solenoid 2 water duration");
    char message[50];
    int inx = 0;
    while (mqttClient.available()) {
      message[inx] = (char)mqttClient.read();
      inx++;
      // Serial.print((char)mqttClient.read());
    }
    message[inx] = '\0';  //null terminate
    Serial.println(message);

    wateringDurations[1] = atoi(message);
  } else if (topic == "Desoto/Aero3/watering/solenoid3/duration") {
    Serial.println("Should update solenoid 3 water duration");
    char message[50];
    int inx = 0;
    while (mqttClient.available()) {
      message[inx] = (char)mqttClient.read();
      inx++;
      // Serial.print((char)mqttClient.read());
    }
    message[inx] = '\0';  //null terminate
    Serial.println(message);

    wateringDurations[2] = atoi(message);
  } else if (topic == "Desoto/Aero3/watering/delay") {
    Serial.println("Should update the watering delay");
    char message[50];
    int inx = 0;
    while (mqttClient.available()) {
      message[inx] = (char)mqttClient.read();
      inx++;
      // Serial.print((char)mqttClient.read());
    }
    message[inx] = '\0';  //null terminate
    Serial.println(message);

    wateringDelay = atoi(message);
    Serial.println("Updating delay to :" + String(wateringDelay));
    Alarm.write(wateringAlarmID, wateringDelay);
  } else {
    Serial.print("Update not recognized: ");
    Serial.println(topic);
  }
}