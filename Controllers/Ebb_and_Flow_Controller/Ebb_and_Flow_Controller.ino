#include "credentials.h"  //you need to create this file and #define mySSID and myPASSWORD. or comment this out and fill in below
#include "mqttFunctions.h"

#include <WiFi.h>

#include <TimeLib.h>
#include <TimeAlarms.h>

#include <ArduinoMqttClient.h>

//WIFI
const char* ssid = mySSID;
const char* password = myPASSWORD;

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
const int nbrOfSolenoids = 4;                                     //number of solenoids connected
long levelDelays[] = { 8, 8, 8, 8, 8, 8, 8 };                     //hours. how long to wait between watering
long levelLastWatered[] = { -24, -24, -24, -24, -24, -24, -24 };  //hours. the last time that the level was watered, -24 makes it water immediately
long levelWaterDurations[] = { 6, 4, 6, 12, 12, 12, 12 };         //minutes. how long each level should water for
long levelDrainDurations[] = { 21, 12, 12, 12, 22, 22, 22 };      //minutes. how long each level should drain for
int pumpPWMTime = 8000;                                           //milliseconds. How long the pump should pwm for

bool waterLevelFlags[nbrOfSolenoids + 1];  //flags to water levels. The last index is for watering all the levels

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

  //initialize flags
  for (int i = 0; i < nbrOfSolenoids + 1; i++) {
    waterLevelFlags[i] = false;
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

  //CONNECT TO MQTT BROKER
  mqttClient.onMessage(onMqttMessage);  // set the message receive callback
  connectToBroker();

  //SET ALARMS
  //ill need to get the ID of the alarm to be able to update it
  watering1AlarmID = Alarm.alarmRepeat(9, 00, 0, waterLevels);
  watering2AlarmID = Alarm.alarmRepeat(12, 00, 0, waterLevels);
  watering3AlarmID = Alarm.alarmRepeat(17, 00, 0, waterLevels);
}

//--------------LOOP---------------
void loop() {
  Alarm.delay(0);  //needed to service alarms

  mqttClient.poll();  // call poll() regularly to allow the library to receive MQTT messages and send MQTT keep alives which avoids being disconnected by the broker

  if (!mqttClient.connected()) connectToBroker();
  if (WiFi.status() != WL_CONNECTED) connectToWifi();

  checkWateringFlags();
}

void checkWateringFlags() {
  for (int i = 0; i < nbrOfSolenoids; i++) {
    if (waterLevelFlags[i]) {
      Serial.println("Watering level " + String(i));
      waterLevel(i);
      waterLevelFlags[i] = false;
    }
  }
  //this last flag is for watering all the levels
  if (waterLevelFlags[nbrOfSolenoids]) {
    Serial.println("Watering all levels ");
    waterLevels();
    waterLevelFlags[nbrOfSolenoids] = false;
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
    Serial.print(((levelDelays[level] * HOURTOMILLISEC) - (millis() - (levelLastWatered[level] * HOURTOMILLISEC))) / (1000L * 60L));
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
void waterLevels() {
  for (int i = 1; i <= nbrOfSolenoids; i++) {
    waterLevel(i);
  }
  drainLevels();
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
  Serial.println("Starting to water for " + String((levelWaterDurations[level] * MINTOMILLISEC) / 1000L) + " seconds");
  levelLastWatered[level] = millis();  //update record of timing

  //update to mqtt
  mqttClient.beginMessage(updateTopic);
  mqttClient.print("starting to water");
  mqttClient.endMessage();

  //open solenoid
  digitalWrite(solenoidPins[level], HIGH);

  //PWM the pump
  pwmPump();

  //turn solenoid on and wait
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

  //turn off pump
  digitalWrite(pumpPin, LOW);

  mqttClient.beginMessage(updateTopic);
  mqttClient.print("starting to drain");
  mqttClient.endMessage();
  long startDrain = millis();
  while (millis() - startDrain < levelDrainDurations[level] * MINTOMILLISEC) {
    //pass
    int secondsLeft = (int)((levelDrainDurations[level] * MINTOMILLISEC) - (millis() - startDrain)) / 1000;
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


void printTimeAndDate() {
  Serial.println("Time: " + String(hour()) + ":" + String(minute()) + ":" + String(second()));
  Serial.println("Date: " + String(month()) + "/" + String(day()) + "/" + String(year()));
}

void saveTime(char* inputString) {
  char timeStr[50];
  strcpy(timeStr, inputString);
  Serial.println("We received: ");
  Serial.println(timeStr);  //expecting of form "2022-11-23, 13:34"
  char* yearHA = strtok(timeStr, "-");
  char* monthHA = strtok(NULL, "-");
  char* dayHA = strtok(NULL, ",");
  char* hourHA = strtok(NULL, ":");
  char* minuteHA = strtok(NULL, ":");
  setTime(atoi(hourHA), atoi(minuteHA), 0, atoi(dayHA), atoi(monthHA), atoi(yearHA));
  printTimeAndDate();
}

void waterLevelXsetFlag(int levelNbr) {
  waterLevelFlags[levelNbr - 1] = true;  //level 1 should be index 0

  Serial.println("The flags are now");
  for (int i = 0; i < nbrOfSolenoids + 1; i++) {
    Serial.print(String(waterLevelFlags[i])); Serial.print(",");    
  }
  Serial.println();
}

bool parsingError = false;  //flag for parsing error. Either topic or message not recognized

//everything in this function should be executed quickly. If there is any need for a delay, think about setting a flag in this function and responding in the loop
void onMqttMessage(int messageSize) {
  long startResponse = millis();

  String topic = mqttClient.messageTopic();
  Serial.println();
  Serial.println(topic);

  //MANUALLY CONTROLL SOLENOIDS/PUMP
  if (topic == "Desoto/EbbNFlow/solenoids/1/command") {
    readMessage();
    Serial.println(mqttMessage);
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      if (!solenoid1State()) {  //if not on
        Serial.println("Should turn on solenoid 1");
        turnOnSolenoid1();
      }
    } else if (strcmp(mqttMessage, "{\"state\":\"off\"}") == 0) {
      if (solenoid1State()) {  //if on
        Serial.println("Should turn off solenoid 1");
        turnOffSolenoid1();
      }
    } else parsingError = true;
  } else if (topic == "Desoto/EbbNFlow/solenoids/2/command") {
    readMessage();
    Serial.println(mqttMessage);
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      if (!solenoid2State()) {  //if not on
        Serial.println("Should turn on solenoid 2");
        turnOnSolenoid2();
      }
    } else if (strcmp(mqttMessage, "{\"state\":\"off\"}") == 0) {
      if (solenoid2State()) {  //if on
        Serial.println("Should turn off solenoid 2");
        turnOffSolenoid2();
      }
    } else parsingError = true;
  } else if (topic == "Desoto/EbbNFlow/solenoids/3/command") {
    readMessage();
    Serial.println(mqttMessage);
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      if (!solenoid3State()) {  //if not on
        Serial.println("Should turn on solenoid 3");
        turnOnSolenoid3();
      }
    } else if (strcmp(mqttMessage, "{\"state\":\"off\"}") == 0) {
      if (solenoid3State()) {  //if on
        Serial.println("Should turn off solenoid 3");
        turnOffSolenoid3();
      }
    } else parsingError = true;
  } else if (topic == "Desoto/EbbNFlow/solenoids/4/command") {
    readMessage();
    Serial.println(mqttMessage);
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      if (!solenoid4State()) {  //if not on
        Serial.println("Should turn on solenoid 4");
        turnOnSolenoid4();
      }
    } else if (strcmp(mqttMessage, "{\"state\":\"off\"}") == 0) {
      if (solenoid4State()) {  //if on
        Serial.println("Should turn on solenoid 4");
        turnOffSolenoid4();
      }
    } else parsingError = true;
  } else if (topic == "Desoto/EbbNFlow/pump/command") {
    readMessage();
    Serial.println(mqttMessage);
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      if (!pumpState()) {  //if not on
        Serial.println("Should turn pump on");
        turnOnPump();
      }
    } else if (strcmp(mqttMessage, "{\"state\":\"off\"}") == 0) {
      if (pumpState()) {  //if on
        Serial.println("Should turn pump off");
        turnOffPump();
      }
    } else if (strcmp(mqttMessage, "{\"state\":\"start\"}") == 0) {
      if (!pumpState()) {  //if on
        Serial.println("Should start the pump");
        pwmPump();
        turnOnPump();
      }
    } else parsingError = true;
  }

  //MANUALLY TRIGGGER WATERINGS
  else if (topic == "Desoto/EbbNFlow/watering/1/command") {
    readMessage();
    Serial.println(mqttMessage);
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      Serial.println("Should water level 1");
      waterLevelXsetFlag(1);
    } else parsingError = true;
  } else if (topic == "Desoto/EbbNFlow/watering/2/command") {
    readMessage();
    Serial.println(mqttMessage);
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      Serial.println("Should water level 2");
      waterLevelXsetFlag(2);
    } else parsingError = true;
  } else if (topic == "Desoto/EbbNFlow/watering/3/command") {
    readMessage();
    Serial.println(mqttMessage);
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      Serial.println("Should water level 3");
      waterLevelXsetFlag(3);
    } else parsingError = true;
  } else if (topic == "Desoto/EbbNFlow/watering/4/command") {
    readMessage();
    Serial.println(mqttMessage);
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      Serial.println("Should water level 4");
      waterLevelXsetFlag(4);
    } else parsingError = true;
  } else if (topic == "Desoto/EbbNFlow/watering/all/command") {
    readMessage();
    Serial.println(mqttMessage);
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      Serial.println("Should water all levels");
      waterLevelXsetFlag(nbrOfSolenoids + 1);  //this will set the flag of index: nbrOfSolenoids
    } else parsingError = true;
  }

  // UPDATE WATER DURATIONS
  else if (topic == "Desoto/EbbNFlow/watering/1/duration") {
    Serial.println("Should update level 1 watering duration");
    readMessage();
    levelWaterDurations[0] = atoi(mqttMessage) * MINTOMILLISEC;
    Serial.println(levelWaterDurations[0]);
  } else if (topic == "Desoto/EbbNFlow/watering/2/duration") {
    Serial.println("Should update level 2 watering duration");
    readMessage();
    levelWaterDurations[1] = atoi(mqttMessage) * MINTOMILLISEC;
    Serial.println(levelWaterDurations[1]);
  } else if (topic == "Desoto/EbbNFlow/watering/3/duration") {
    Serial.println("Should update level 3 watering duration");
    readMessage();
    levelWaterDurations[2] = atoi(mqttMessage) * MINTOMILLISEC;
    Serial.println(levelWaterDurations[2]);
  } else if (topic == "Desoto/EbbNFlow/watering/4/duration") {
    Serial.println("Should update level 4 watering duration");
    readMessage();
    levelWaterDurations[3] = atoi(mqttMessage) * MINTOMILLISEC;
    Serial.println(levelWaterDurations[3]);
  }

  //TRIGGER DRAINING
  else if (topic == "Desoto/EbbNFlow/watering/drain") {
    Serial.println("Should drain levels");
    readMessage();
    Serial.println(mqttMessage);
    drainLevels();
  }

  // FINAL DRAIN TIME
  else if (topic == "Desoto/EbbNFlow/watering/finalDrainTime") {
    Serial.println("Should update drain time");
    readMessage();
    draingTime = int(atof(mqttMessage) * MINTOMILLISEC);
    Serial.println(draingTime);
  }

  //UPDATE TIME AND DATE
  else if (topic == "homeassistant/dateAndTime") {
    readMessage();
    Serial.println("Received message: " + String(mqttMessage));
    saveTime(mqttMessage);
  }

  //IF TOPIC NOT RECOGNIZED
  else {
    Serial.print("Update not recognized: ");
    Serial.println(topic);
  }

  //IF MESSAGE IN A TOPIC WAS NOT RECOGNIZED
  if (parsingError) {
    Serial.print("message not recognized for topic: ");
    Serial.print(topic);
    Serial.print(" and message: ");
    Serial.println(mqttMessage);
    parsingError = false;
  }

  Serial.println("Parse/response time: " + String(millis() - startResponse) + " milliseconds");
  Serial.println();
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