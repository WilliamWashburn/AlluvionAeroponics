
#include <Wire.h>

#define SLAVE_ADDR 0x28
#define OUTPUT_MIN 0x666        // 10% of 2^14 - 1
#define OUTPUT_MAX 0x399A       // 90% of 2^14 - 1
#define PRESSURE_MIN 0.0        // min is 0 for sensors that give absolute values
#define PRESSURE_MAX 1034213.5 // 150psi (and we want results in pascals)

uint32_t prev = 0; 
const uint32_t interval = 5000;

void setup() {
    Serial.begin(9600);
    Wire.begin();
}

struct cs_raw {
    uint8_t status;             // 2 bit
    uint16_t bridge_data;       // 14 bit
    uint16_t temperature_data;  // 11 bit
};

void loop() {
    unsigned long now = millis();
    struct cs_raw ps;
    char p_str[10], t_str[10];
    uint8_t el;
    float p, t;
    if ((now - prev > interval) && (Serial.available() <= 0)) {
        prev = now;
        el = ps_get_raw(SLAVE_ADDR, &ps);
        // for some reason my chip triggers a diagnostic fault
        // on 50% of powerups without a notable impact 
        // to the output values.
        if ( el == 4 ) {
            Serial.println("err sensor missing");
        } else {
            if ( el == 3 ) {
                Serial.print("err diagnostic fault ");
                Serial.println(ps.status, BIN);
            }
            if ( el == 2 ) {
                // if data has already been feched since the last
                // measurement cycle
                Serial.print("warn stale data ");
                Serial.println(ps.status, BIN);
            }
            if ( el == 1 ) {
                // chip in command mode
                // no clue how to end up here
                Serial.print("warn command mode ");
                Serial.println(ps.status, BIN);
            }
            Serial.print("status      ");
            Serial.println(ps.status, BIN);
            Serial.print("bridge_data ");
            Serial.println(ps.bridge_data, DEC);
            Serial.print("temp_data   ");
            Serial.println(ps.temperature_data, DEC);
            Serial.println("");
            ps_convert(ps, &p, &t, OUTPUT_MIN, OUTPUT_MAX, PRESSURE_MIN,
                   PRESSURE_MAX);
            // floats cannot be easily printed out
            dtostrf(p, 2, 2, p_str);
            dtostrf(t, 2, 2, t_str);
            Serial.print("pressure    (Pa) ");
            Serial.println(p_str);
            Serial.print("temperature (dC) ");
            Serial.println(t_str);
            Serial.println("");
        }
    }
}


/*

  TruStability HSC and SSC pressure sensor library for the Arduino.

  This library implements the following features:

   - read raw pressure and temperature count values
   - compute absolute pressure and temperature

  Author:          Petre Rodan <petre.rodan@simplex.ro>
  Available from:  https://github.com/rodan/honeywell_hsc_ssc_i2c
  License:         GNU GPLv3

  Honeywell High Accuracy Ceramic (HSC) and Standard Accuracy Ceramic
  (SSC) Series are piezoresistive silicon pressure sensors.


  GNU GPLv3 license:
  
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
   
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
   
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
   
*/



/* you must define the slave address. you can find it based on the part number:

    _SC_________XA_
    where X can be one of:

    S  - spi (this is not the right library for spi opperation)
    2  - i2c slave address 0x28
    3  - i2c slave address 0x38
    4  - i2c slave address 0x48
    5  - i2c slave address 0x58
    6  - i2c slave address 0x68
    7  - i2c slave address 0x78
*/

/// function that requests raw data from the sensor via i2c
///
/// input
///  slave_addr    - i2c slave addr of the sensor chip
/// output
///  raw           - struct containing 4 bytes of read data
/// returns
///         0 if all is fine
///         1 if chip is in command mode
///         2 if old data is being read
///         3 if a diagnostic fault is triggered in the chip
///         4 if the sensor is not hooked up
uint8_t ps_get_raw(const uint8_t slave_addr, struct cs_raw *raw)
{
    uint8_t i, val[4] = { 0, 0, 0, 0 };
    Wire.requestFrom(slave_addr, (uint8_t) 4);
    for (i = 0; i <= 3; i++) {
        delay(4);                        // sensor might be missing, do not block
        val[i] = Wire.read();            // by using Wire.available()
    }
    raw->status = (val[0] & 0xc0) >> 6;  // first 2 bits from first byte
    raw->bridge_data = ((val[0] & 0x3f) << 8) + val[1];
    raw->temperature_data = ((val[2] << 8) + (val[3] & 0xe0)) >> 5;
    if ( raw->temperature_data == 65535 ) return 4;
    return raw->status;
}


/// function that converts raw data read from the sensor into temperature and pressure values
///
/// input:
///  raw            - struct containing all 4 bytes read from the sensor
///  output_min     - output at minimal calibrated pressure (counts)
///  output_max     - output at maximum calibrated pressure (counts)
///  pressure_min   - minimal value of pressure range
///  pressure_max   - maxium value of pressure range
///
/// output:
///  pressure
///  temperature
uint8_t ps_convert(const struct cs_raw raw, float *pressure, float *temperature,
                   const uint16_t output_min, const uint16_t output_max, const float pressure_min,
                   const float pressure_max)
{
    *pressure = 1.0 * (raw.bridge_data - output_min) * (pressure_max - pressure_min) / (output_max - output_min) + pressure_min;
    *temperature = (raw.temperature_data * 0.0977) - 50;
    return 0;
}