/*
 * Created 05/30/2014 - max31865.ino
 * 
 * Vapoduino - Arduino based controller for a Vaporizer.
 * 
 * Copyright (C) 2014 Benedikt Schlagberger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <SPI.h>
 
/*******************************************************************************
 **                          CONFIGURATION PARAMETERS                         **
 *******************************************************************************/

#define DRDY_PIN 4
#define CS_PIN 10
#define REFERENCE_RESISTOR 400
#define PT_RESISTANCE 100
#define USE_AVERGAGE_TEMP 1
#define AVG_VALUES_COUNT 5

/**
 * Configuration of the MAX31865 from MSB to LSB:
 * BIT      FUNCTION            ASSIGNMENT
 *  7       VBIAS               0=OFF            1=ON
 *  6       Conversion Mode     0=Normally OFF   1=AUTO
 *  5       1-Shot              0= -             1=1-Shot
 *  4       3-Wire              0=2- or 4-Wire   1=3-wire
 * 3,2      Faultdetection      set both to 0
 *  1       Fault Status Clear  set to 1
 *  0       50/60Hz filter      0=60Hz           1=50Hz
 */
#define MAX31865_CONFIG 0b11000011

//*******************************************************************************


// Registers defined in Table 1 on page 12 of the data sheet
#define CONFIGURATION_REGISTER 0x00
#define RTD_MSB_REGISTER 0x01
#define RTD_LSB_REGISTER 0x02
#define HIGH_FAULT_THRESHOLD_MSB_REGISTER 0x03
#define HIGH_FAULT_THRESHOLD_LSB_REGISTER 0x04
#define LOW_FAULT_THRESHOLD_MSB_REGISTER 0x05
#define LOW_FAULT_THRESHOLD_LSB_REGISTER 0x06
#define FAULT_STATUS_REGISTER 0x07

#define READ_MODE 0x00
#define WRITE_MODE 0x80

// Selfclearing bits for the configuation register
#define START_1_SHOT 0x20
#define CLEAR_FAULT_STATUS 0x02

// Constants for Callendar-Van Dusen equation
static float a = 0.00390830;
static float b = -0.0000005775;
static float c = -0.00000000000418301;

#if (USE_AVERGAGE_TEMP)
static boolean use_average_temp = true;
#else
static boolean use_average_temp = false;
#endif

// Array for averaging the reads
int values[AVG_VALUES_COUNT];
int counter;

boolean max_init() {
    // Configuring in- and outputs
    pinMode(DRDY_PIN, INPUT);
    pinMode(CS_PIN, OUTPUT);
    
    // Deselecting MAX31865
    digitalWrite(CS_PIN, HIGH);
  
    // Configuring SPI
    SPI.begin();
    SPI.setDataMode(SPI_MODE1);
//    SPI.setClockDivider(SPI_CLOCK_DIV128);
    
    // Wait for MAX31865 to set up
    delay(200);
    
    // Initial configuration of MAX31865
    if (!_send_config(MAX31865_CONFIG)) {
        Serial.println("Communication failed!");
        Serial.println(_spi_read(CONFIGURATION_REGISTER), BIN);
        return false;
    }
    
    // fill average data
    for (int i = 0; i < AVG_VALUES_COUNT; i++) {
        max_get_temp();
    }
    
    // Setting up average calculation
    counter = 0;
    
    // TODO: Set threshold for temperature reading
    
//    _spi_write(LOW_FAULT_THRESHOLD_MSB_REGISTER, 0x00);
//    _spi_write(LOW_FAULT_THRESHOLD_LSB_REGISTER, 0x00);
//    _spi_write(HIGH_FAULT_THRESHOLD_MSB_REGISTER, 0xFF);
//    _spi_write(HIGH_FAULT_THRESHOLD_LSB_REGISTER, 0xFF);
    return true;
}

/**
 * Reads the rtd value from the MAX31865 chip and calculates the temperature by using
 * the Callendar-Van Dusen equation for approximation.
 */
float max_get_temp() {
    // read value
    byte lsb = _spi_read(RTD_LSB_REGISTER);
    unsigned int rtd_value = _spi_read(RTD_MSB_REGISTER) << 7;
    
    // shift out the fault bit
    rtd_value += (lsb >> 1);
    
    if ((lsb & 1)) {
        Serial.print("\nFault Detected! RTD value: ");
        Serial.println(rtd_value);
        delay(100);
        byte fault = _spi_read(FAULT_STATUS_REGISTER);
        
        _resolve_fault(fault);
        
        // return NaN
        return rtd_value;
    } else {
        // check if data is valid
        if (rtd_value == 0) {
            Serial.println("MAX31865 is stuck or not responding. Trying to reconfigure.");
            if (!_send_config(MAX31865_CONFIG)) {
                Serial.println("Communication failed!");
                Serial.println(_spi_read(CONFIGURATION_REGISTER), BIN);
                while(true);
            }
            // return NaN
            return 0.0 / 0.0;
        }
        
        if (use_average_temp) {
            rtd_value = _calculate_avg(rtd_value);
        }
        
        return _calculate_temp(rtd_value);
    }
}

/**
 * Sends the given value to the given registeraddress via SPI.
 */
void _spi_write(byte address, byte value) {
    digitalWrite(CS_PIN, LOW);
    SPI.transfer(WRITE_MODE + address);
    SPI.transfer(value);
    digitalWrite(CS_PIN, HIGH);
}

/**
 * Reads from the given registeraddress via SPI and returns
 * the value.
 */
byte _spi_read(byte address) {
    digitalWrite(CS_PIN, LOW);
    SPI.transfer(READ_MODE + address);
    byte value = SPI.transfer(0x00);
    digitalWrite(CS_PIN, HIGH);
    digitalWrite(8, LOW);
    return value;
}

/** 
 * Sends the given configuration to the MAX31865.
 */
boolean _send_config(byte configuration) {
    _spi_write(CONFIGURATION_REGISTER, configuration);
    
    byte returned_value = _spi_read(CONFIGURATION_REGISTER);
    
    // exclude self clearing bit (fault status clear)
    if ((returned_value & 0b11111101) == configuration) {
      return true;
    }
}

/**
 * Calculates the temperature from the given rtd value by using the 
 * Callendar-Van Dusen equation for approximation.
 */ 
float _calculate_temp(int rtd_value) {    
    // calculate recistance of RTD
    float resistance = (((float) rtd_value) * ((float) REFERENCE_RESISTOR)) / 32768;
    
    // Use Callendar-Van Dusen equation for approximation
    float temp = -PT_RESISTANCE * a
            + sqrt(PT_RESISTANCE * PT_RESISTANCE * a * a - 4 * PT_RESISTANCE * b * (PT_RESISTANCE - resistance));
    temp /= (2 * PT_RESISTANCE * b);

//    float temp = rtd_value / 32 - 256;
    return temp;
}

/**
 * Prints information for the given fault status register value to Serial and resets
 * the configuration register to the initial value.
 */
void _resolve_fault(byte fault) {
    byte temp = (fault & 0x80);
    if (temp > 0) {
        Serial.print("High Threshold reached. RTD is possibly disconnected from RTD+ or RTD-.\nThreshold is set to ");
        int threshold = _spi_read(HIGH_FAULT_THRESHOLD_MSB_REGISTER) << 7;
        threshold += _spi_read(HIGH_FAULT_THRESHOLD_LSB_REGISTER) >> 1;
        Serial.println(threshold);
    }
    temp = fault & 0x40;
    if (temp > 0) {
        Serial.print("Low Threshold reached. RTD+ and RTD- is possibly shorted.\nThreshold is set to ");
        int threshold = _spi_read(LOW_FAULT_THRESHOLD_MSB_REGISTER) << 7;
        threshold += _spi_read(LOW_FAULT_THRESHOLD_LSB_REGISTER) >> 1;
        Serial.println(threshold);
    }
    temp = fault & 0x20;
    if (temp > 0) {
        Serial.println("REFIN- > 0.85 * Vbias.");
    }
    temp = fault & 0x10;
    if (temp > 0) {
        Serial.println("REFIN- < 0.85 * Vbias. FORCE- is possibly open.");
    }
    temp = fault & 0x08;
    if (temp > 0) {
        Serial.println("RTDIN- < 0.85 x Vbias. FORCE- is possibly open.");
    }
    temp = fault & 0x04;
    if (temp > 0) {
        Serial.println("Overvoltage/undervoltage fault");
    }
    
    // rewriting config and clearing fault bit
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    Serial.println("Resetting");
    _send_config(MAX31865_CONFIG);
    digitalWrite(LED_PIN, LOW);
}

/**
 * Uses an array to store the last AVG_VALUES_COUNT values and return
 * the mean result of these values.
 */
int _calculate_avg(int current_value) {
  if (counter == AVG_VALUES_COUNT) {
    counter = 0;
  }
    values[counter] = current_value;
    counter++;
    
    long rtd_sum = 0;    
    for (int i = 0; i < AVG_VALUES_COUNT; i++) {
        rtd_sum += values[i];
    }
    
    return (unsigned int) (rtd_sum / AVG_VALUES_COUNT);
}
