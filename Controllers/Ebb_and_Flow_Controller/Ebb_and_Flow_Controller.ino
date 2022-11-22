#include "credentials.h"  //you need to create this file and #define mySSID and myPASSWORD. or comment this out and fill in below
#include "ntpFunctions.h"
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <ArduinoMqttClient.h>

//WIFI
const char* ssid = mySSID;
const char* password = myPASSWORD;
WiFiUDP ntpUDP;
WiFiClient wifiClient;

//MQTT INFO
MqttClient mqttClient(wifiClient);
const char broker[] = "homeassistant.local";
int port = 1883;
const char topic[] = "Desoto/EbbNFlow/#";

//ALARMS
AlarmID_t watering1AlarmID;
AlarmID_t watering2AlarmID;
AlarmID_t watering3AlarmID;

//CONVERSIONS
long HOURTOMILLISEC = 60L * 60L * 1000L;
long MINTOMILLISEC = 60L * 1000L;

//PIN DEFINITIONS
int pumpPin = 26;
int solenoidPins[] = { 13, 12, 27, 14, 33, 15, 32 };  //solenoid pins

//SOLENOID INFORMATION
int nbrOfSolenoids = 4;                                                                                                                                                                  //number of solenoids connected
long levelDelays[] = { 8 * HOURTOMILLISEC, 8 * HOURTOMILLISEC, 8 * HOURTOMILLISEC, 8 * HOURTOMILLISEC, 8 * HOURTOMILLISEC, 8 * HOURTOMILLISEC, 8 * HOURTOMILLISEC };                     //how long to wait between watering
long levelLastWatered[] = { -24 * HOURTOMILLISEC, -24 * HOURTOMILLISEC, -24 * HOURTOMILLISEC, -24 * HOURTOMILLISEC, -24 * HOURTOMILLISEC, -24 * HOURTOMILLISEC, -24 * HOURTOMILLISEC };  //the last time that the level was watered, -24 makes it water immediately
long levelWaterDurations[] = { 6 * MINTOMILLISEC, 4 * MINTOMILLISEC, 6 * MINTOMILLISEC, 12 * MINTOMILLISEC, 12 * MINTOMILLISEC, 12 * MINTOMILLISEC, 12 * MINTOMILLISEC };                //how long each level should water for
long levelDrainDurations[] = { 21 * MINTOMILLISEC, 12 * MINTOMILLISEC, 12 * MINTOMILLISEC, 12 * MINTOMILLISEC, 22 * MINTOMILLISEC, 22 * MINTOMILLISEC, 22 * MINTOMILLISEC };             //how long each level should drain for
int pumpPWMTime = 8000;                                                                                                                                                                  //how long the pump should PWM


//---------------SETUP--------------------
void setup() {
  Serial.begin(115200);
  Serial.println("Beginning...");

  pinMode(pumpPin, OUTPUT);
  digitalWrite(pumpPin, LOW);  //turn pump off

  for (int i = 0; i < nbrOfSolenoids; i++) {
    pinMode(solenoidPins[i], OUTPUT);
    digitalWrite(solenoidPins[i], LOW);  //turn solenoids off
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
  //ill need to get the ID of the alarm to be able to update it
  watering1AlarmID = Alarm.alarmRepeat(9, 00, 0, waterLevels);
  watering2AlarmID = Alarm.alarmRepeat(12, 00, 0, waterLevels);
  watering3AlarmID = Alarm.alarmRepeat(17, 00, 0, waterLevels);
}

//--------------LOOP---------------
void loop() {
  checkWifiConnection();
  Alarm.delay(0);  //needed to service alarms

  // call poll() regularly to allow the library to receive MQTT messages and
  // send MQTT keep alives which avoids being disconnected by the broker
  mqttClient.poll();

  static long lastUpdate = 0;
  if (millis() - lastUpdate > 5000) {
    // printInfo();
    Serial.println("waiting...");
    lastUpdate = millis();
  }

  if (!mqttClient.connected()) {
    Serial.println("MQTT connection lost");
    if (!mqttClient.connect(broker, port)) {
      Serial.print("MQTT reconnection error ");
      Serial.println(mqttClient.connectError());
    }
  }
}

void turnOffSolenoids() {
  for (int i = 0; i < nbrOfSolenoids; i++) {
    digitalWrite(solenoidPins[i], LOW);
  }
}

void printInfo() {
  //Print time left
  for (int level = 0; level < nbrOfSolenoids; level++) {
    Serial.print("Time Left for level " + String(level) + " : ");
    Serial.print((levelDelays[level] - (millis() - levelLastWatered[level])) / (1000L * 60L));
    Serial.println(" minutes");
  }

  //print last watered
  Serial.print("Last Watered: ");
  for (int level = 0; level < nbrOfSolenoids; level++) {
    Serial.print(levelLastWatered[level]);
  }
  Serial.println(" milliseconds");
}

void turnOnSolenoid1() {
  digitalWrite(solenoidPins[0], HIGH);
}
void turnOffSolenoid1() {
  digitalWrite(solenoidPins[0], LOW);
}
bool solenoid1State() {
  return digitalRead(solenoidPins[0]);
}

void turnOnSolenoid2() {
  digitalWrite(solenoidPins[1], HIGH);
}
void turnOffSolenoid2() {
  digitalWrite(solenoidPins[1], LOW);
}
bool solenoid2State() {
  return digitalRead(solenoidPins[1]);
}

void turnOnSolenoid3() {
  digitalWrite(solenoidPins[2], HIGH);
}
void turnOffSolenoid3() {
  digitalWrite(solenoidPins[2], LOW);
}
bool solenoid3State() {
  return digitalRead(solenoidPins[2]);
}

void turnOnSolenoid4() {
  digitalWrite(solenoidPins[3], HIGH);
}
void turnOffSolenoid4() {
  digitalWrite(solenoidPins[3], LOW);
}
bool solenoid4State() {
  return digitalRead(solenoidPins[3]);
}


void turnOnPump() {
  digitalWrite(pumpPin, HIGH);
}
void turnOffPump() {
  digitalWrite(pumpPin, LOW);
}
bool pumpState() {
  return digitalRead(pumpPin);
}

void waterLevel1() {
  waterLevel(1);
  drainLevel(1);
}
void waterLevel2() {
  waterLevel(2);
  drainLevel(2);
}
void waterLevel3() {
  waterLevel(3);
  drainLevel(3);
}
void waterLevel4() {
  waterLevel(4);
  drainLevel(4);
}

void drainLevels() {
  for (int i = 1; i <= nbrOfSolenoids; i++) {
    drainLevel(i);
  }
}

//this is a mock delay function that still does whatever we need to do such as maintain connection with mqtt
void myDelay(long delayTime, bool shouldPrint = false) {
  long startTime = millis();
  while (millis() - startTime < delayTime) {
    //pass
    static long printTime = 0;
    if (millis() - printTime > 1000 && shouldPrint == true) {
      Serial.println("Time left: " + String((delayTime - (millis() - startTime)) / 1000) + " seconds");
      printTime = millis();
    }
    mqttClient.poll();
  }
}

long draingTime = 120000;
void drainLevel(int sol) {
  digitalWrite(solenoidPins[sol - 1], HIGH);
  myDelay(draingTime, true);  //custom delay function that still keeps connection with mqtt
  digitalWrite(solenoidPins[sol - 1], LOW);
}

void waterLevels() {
  for (int i = 1; i <= nbrOfSolenoids; i++) {
    waterLevel(i);
  }


  // mqttClient.beginMessage(updateTopic);
  // mqttClient.print("final draining");
  // mqttClient.endMessage();
  drainLevels();

  // mqttClient.beginMessage(updateTopic);
  // mqttClient.print("watering cycle complete");
  // mqttClient.endMessage();
}

void pwmPump() {
  long pwmStartTime = millis();
  Serial.println("toggling pump");
  while (millis() - pwmStartTime < pumpPWMTime) {
    digitalWrite(pumpPin, LOW);
    delay(200);  // how long to stay off

    digitalWrite(pumpPin, HIGH);
    delay(100);  // how long to stay on
  }
}

void waterLevel(int level) {
  turnOffSolenoids();  //just incase one is open

  //build mqtt topic for updating state of watering
  char updateTopic[50] = "Desoto/EbbNFlow/watering/";
  char levelNbrAsChar[4];
  strcat(updateTopic, itoa(level, levelNbrAsChar, 10));
  strcat(updateTopic, "/wateringUpdate");

  level = level - 1;  //adjust so level 1 is index 0
  Serial.println("Starting to water for " + String(levelWaterDurations[level] / 1000L) + " seconds");
  levelLastWatered[level] = millis();  //update record of timing

  //update to mqtt
  mqttClient.beginMessage(updateTopic);
  mqttClient.print("starting to water");
  mqttClient.endMessage();

  //open solenoid
  digitalWrite(solenoidPins[level], HIGH);

  //PWM the pump
  pwmPump();

  //turn pump on and wait
  digitalWrite(pumpPin, HIGH);
  long wateringStartTime = millis();
  while (millis() - wateringStartTime < levelWaterDurations[level]) {
    int secondsLeft = (int)(levelWaterDurations[level] - (millis() - wateringStartTime)) / 1000;
    static int prevSecondsLeft = 0;
    if (secondsLeft != prevSecondsLeft) {
      Serial.println(String(secondsLeft) + " seconds left in watering");
      prevSecondsLeft = secondsLeft;
    }
    mqttClient.poll();
  }
  // delay(levelWaterDurations[level]);

  //turn off solenoid and pump
  digitalWrite(pumpPin, LOW);
  // delay(1000);  //Relief pressure? Dont knowTime if its a problem really but doesnt hurt

  mqttClient.beginMessage(updateTopic);
  mqttClient.print("starting to drain");
  mqttClient.endMessage();
  long startDrain = millis();
  while (millis() - startDrain < levelDrainDurations[level]) {
    //pass
    int secondsLeft = (int)(levelDrainDurations[level] - (millis() - startDrain)) / 1000;
    static int prevSecondsLeft = 0;
    if (secondsLeft != prevSecondsLeft) {
      Serial.println(String(secondsLeft) + " seconds left in draining");
      prevSecondsLeft = secondsLeft;
    }
    mqttClient.poll();
  }
  digitalWrite(solenoidPins[level], LOW);
  mqttClient.beginMessage(updateTopic);
  mqttClient.print("draining ended");
  mqttClient.endMessage();

  Serial.println("End watering");
}

char message[50];  //for storing incoming mqtt message
void readMessage() {
  int inx = 0;
  while (mqttClient.available()) {
    message[inx] = (char)mqttClient.read();
    inx++;
    // Serial.print((char)mqttClient.read());
  }
  message[inx] = '\0';  //null terminate
}

void onMqttMessage(int messageSize) {

  String topic = mqttClient.messageTopic();
  if (topic == "Desoto/EbbNFlow/solenoids/1/command") {
    Serial.println("Should toggle solenoid 1");
    readMessage();
    Serial.println(message);
    if (strcmp(message, "{\"state\":\"on\"}") == 0) {
      if (!solenoid1State()) {  //if not on
        turnOnSolenoid1();
      }
    } else if (strcmp(message, "{\"state\":\"off\"}") == 0) {
      if (solenoid1State()) {  //if on
        turnOffSolenoid1();
      }
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/EbbNFlow/solenoids/2/command") {
    Serial.println("Should toggle solenoid 2");
    readMessage();
    Serial.println(message);
    if (strcmp(message, "{\"state\":\"on\"}") == 0) {
      if (!solenoid2State()) {  //if not on
        turnOnSolenoid2();
      }
    } else if (strcmp(message, "{\"state\":\"off\"}") == 0) {
      if (solenoid2State()) {  //if on
        turnOffSolenoid2();
      }
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/EbbNFlow/solenoids/3/command") {
    Serial.println("Should toggle solenoid 3");
    readMessage();
    Serial.println(message);
    if (strcmp(message, "{\"state\":\"on\"}") == 0) {
      if (!solenoid3State()) {  //if not on
        turnOnSolenoid3();
      }
    } else if (strcmp(message, "{\"state\":\"off\"}") == 0) {
      if (solenoid3State()) {  //if on
        turnOffSolenoid3();
      }
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/EbbNFlow/solenoids/4/command") {
    Serial.println("Should toggle solenoid 4");
    readMessage();
    Serial.println(message);
    if (strcmp(message, "{\"state\":\"on\"}") == 0) {
      if (!solenoid4State()) {  //if not on
        turnOnSolenoid4();
      }
    } else if (strcmp(message, "{\"state\":\"off\"}") == 0) {
      if (solenoid4State()) {  //if on
        turnOffSolenoid4();
      }
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/EbbNFlow/pump/command") {
    Serial.println("Should toggle pump");
    readMessage();
    Serial.println(message);
    if (strcmp(message, "{\"state\":\"on\"}") == 0) {
      if (!pumpState()) {  //if not on
        turnOnPump();
      }
    } else if (strcmp(message, "{\"state\":\"off\"}") == 0) {
      if (pumpState()) {  //if on
        turnOffPump();
      }
    } else if (strcmp(message, "{\"state\":\"start\"}") == 0) {
      if (!pumpState()) {  //if on
        pwmPump();
        turnOnPump();
      }
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/EbbNFlow/watering/1/command") {
    Serial.println("Should water level 1");
    readMessage();
    Serial.println(message);
    if (strcmp(message, "{\"state\":\"on\"}") == 0) {
      waterLevel1();
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/EbbNFlow/watering/2/command") {
    Serial.println("Should water level 2");
    readMessage();
    Serial.println(message);
    if (strcmp(message, "{\"state\":\"on\"}") == 0) {
      waterLevel2();
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/EbbNFlow/watering/3/command") {
    Serial.println("Should water level 3");
    readMessage();
    Serial.println(message);
    if (strcmp(message, "{\"state\":\"on\"}") == 0) {
      waterLevel3();
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/EbbNFlow/watering/4/command") {
    Serial.println("Should water level 4");
    readMessage();
    Serial.println(message);
    if (strcmp(message, "{\"state\":\"on\"}") == 0) {
      waterLevel4();
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/EbbNFlow/watering/all/command") {
    Serial.println("Should water all levels");
    readMessage();
    Serial.println(message);
    if (strcmp(message, "{\"state\":\"on\"}") == 0) {
      waterLevels();
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/EbbNFlow/watering/1/duration") {
    Serial.println("Should update level 1 watering duration");
    readMessage();
    levelWaterDurations[0] = atoi(message) * MINTOMILLISEC;
    Serial.println(levelWaterDurations[0]);
  } else if (topic == "Desoto/EbbNFlow/watering/2/duration") {
    Serial.println("Should update level 2 watering duration");
    readMessage();
    levelWaterDurations[1] = atoi(message) * MINTOMILLISEC;
    Serial.println(levelWaterDurations[1]);
  } else if (topic == "Desoto/EbbNFlow/watering/3/duration") {
    Serial.println("Should update level 3 watering duration");
    readMessage();
    levelWaterDurations[2] = atoi(message) * MINTOMILLISEC;
    Serial.println(levelWaterDurations[2]);
  } else if (topic == "Desoto/EbbNFlow/watering/4/duration") {
    Serial.println("Should update level 4 watering duration");
    readMessage();
    levelWaterDurations[3] = atoi(message) * MINTOMILLISEC;
    Serial.println(levelWaterDurations[3]);
  } else if (topic == "Desoto/EbbNFlow/watering/drain") {
    Serial.println("Should drain levels");
    readMessage();
    Serial.println(message);
    drainLevels();
  } else if (topic == "Desoto/EbbNFlow/watering/finalDrainTime") {
    Serial.println("Should update drain time");
    readMessage();
    draingTime = int(atof(message) * MINTOMILLISEC);
    Serial.println(draingTime);

  } else {
    Serial.print("Update not recognized: ");
    Serial.println(topic);
  }
}

void checkWifiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Disconnected from wifi!");

    WiFi.begin(ssid, password);

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
    }
  }
}