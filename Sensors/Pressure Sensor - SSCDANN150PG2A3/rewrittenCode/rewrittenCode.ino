// I based this off of:  https://github.com/rodan/honeywell_hsc_ssc_i2c

// I've created additional documentation here: https://docs.google.com/document/d/1rYl9e5dVmHwrdOOVXVyE1Kn_DVXwWgf81K12OaJdim4/edit?usp=sharing
// Part number: SSC-D-AN-N-150PG-2-A-3

#include <Wire.h>

#define sensorAddress 0x28  //Given by the part number (See page 13 of the datasheet). The 2 means the address is 0x28
//150PG is gage pressre from 0 psi to 150 psi
float minPressure = 0.0;    //psi
float maxPressure = 150.0;  //psi

// the sensor outputs a 14 bit number that represents the pressure
// binary: 00000000000000 (14 bits)
// for the minimum pressure, the sensor will output 10% of (2^14)-1 or 1638
// in hex this is 0x666
// for the max pressure, its 90% of (2^14)-1 or 14745
// in hex this is 0x3999

float minCount = 1638;  //see Table 7 of datasheet for given counts. I guess they just round?
float maxCount = 14746;

uint8_t bytesReceived[4] = { 0, 0, 0, 0 };
uint8_t status;           //status of sensor
uint16_t pressure_bytes;  //count -> pressure
uint16_t temp_bytes;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Wire.begin();
}

void loop() {
  static long prevTime = 0;
  if (millis() - prevTime > 2000) {
    readPressure();
    prevTime = millis();
  }
}

void readPressure() {
  grabRawData();                 //read the 4 bytes from the sensor
  rearrangeBits();               //rearrange bytes into 2 bits for the status, 14 bits for the pressure, and 11 bits for the temperature
  if (checkForError()) return;   //exit if status is not normal operation

  float outputPressure = applyPressureTransferFunction(pressure_bytes); //convert count to pressure
  float tempF = applyTemperatureTransferFunction(temp_bytes); //convert value to temperature

  Serial.print("Pressure: ");Serial.print(outputPressure);Serial.println(" psi");
  Serial.print("Temperature: ");Serial.print(tempF);Serial.println(" F");

  Serial.println();
}

bool checkForError() {
  if (status == 1) {
    Serial.println("device in command mode!");
    return true;
  } else if (status == 2) {
    Serial.println("stale data!");
    return true;
  } else if (status == 3) {
    Serial.println("diagnostic condition!");
    return true;
  }
  return false;
}

void viewDebugData() {
  //print raw data
  Serial.print(bytesReceived[0]);
  Serial.print("\t: ");
  printBits(bytesReceived[0]);
  Serial.print(bytesReceived[1]);
  Serial.print("\t: ");
  printBits(bytesReceived[1]);
  Serial.print(bytesReceived[2]);
  Serial.print("\t: ");
  printBits(bytesReceived[2]);
  Serial.print(bytesReceived[3]);
  Serial.print("\t: ");
  printBits(bytesReceived[3]);

  //print rearranged bits
  Serial.print(status);
  Serial.print("\t: ");
  printBits(status);
  Serial.print(pressure_bytes);
  Serial.print("\t: ");
  printBits(pressure_bytes);
  Serial.print(temp_bytes);
  Serial.print("\t: ");
  printBits(temp_bytes);
}

void grabRawData() {
  //request the 4 bytes
  Wire.requestFrom(sensorAddress, 4);

  for (int i = 0; i < 4; i++) {
    bytesReceived[i] = Wire.read();
  }
}

float applyPressureTransferFunction(uint16_t count) {
  float pressure = ((count - minCount) * (maxPressure - minPressure) / (maxCount - minCount)) + minPressure;
  return pressure;
}

float applyTemperatureTransferFunction(uint16_t temp) {
  float tempC = (temp / 2047.0) * 200.0 - 50.0;  //deg C
  float tempF = tempC * (9.0 / 5.0) + 32.0;      //convert to F
  return tempF;
}

void rearrangeBits() {
  status = (bytesReceived[0] & 0b11000000) >> 6;                               //grab the first 2 bits and shift to the right
  pressure_bytes = ((bytesReceived[0] & 0b00111111) << 8) + bytesReceived[1];  //grab the remaining 6 bits, shift them over 8 and then add the second part of the bridge data. This makes 14 bits
  temp_bytes = ((uint16_t)bytesReceived[2] << 3) + ((bytesReceived[3] & 0b11100000) >> 5);
}

void printBits(uint8_t byte) {
  for (int i = 7; i >= 0; i--) {
    Serial.print(bitRead(byte, i));
  }
  Serial.println();
}

void printBits(uint16_t byte) {
  for (int i = 15; i >= 0; i--) {
    Serial.print(bitRead(byte, i));
  }
  Serial.println();
}