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