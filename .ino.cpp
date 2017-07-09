#ifdef __IN_ECLIPSE__
//This is a automatic generated file
//Please do not modify this file
//If you touch this file your change will be overwritten during the next build
//This file has been generated on 2017-04-15 00:15:40

#include "Arduino.h"
#include <Arduino.h>
#include <SPI.h>
#include <Arduino.h>
#include <SoftwareSerial.h>
#define MOSFET_GATE_PIN 5
#define BUTTON_PIN 3
#define LED_PIN 6
#define VIBRATOR_PIN 9
#define BATTERY_PIN A3
#define BT_RX_PIN 8
#define BT_TX_PIN 7
#define PID_P 19
#define PID_I 12
#define PID_D 12
#define PID_P_HEATING 3.6
#define PID_I_HEATING 0.2
#define PID_D_HEATING 4
#define MAX_HEATING_POWER 255
#define VOLTAGE_DIVIDER 0.01505277f
#define MAXIMUM_VOLTAGE 12.6f
#define MINIMUM_VOLTAGE 11.0f
#define STANDBY_TIMEOUT 1500
#define TEMPERATURE_EEPROM_ADDRESS 0
#include <PID_v1.h>
#include <EEPROM.h>
boolean max_init() ;
float max_get_temp() ;
void _spi_write(byte address, byte value) ;
byte _spi_read(byte address) ;
boolean _send_config(byte configuration) ;
float _calculate_temp(int rtd_value) ;
void _resolve_fault(byte fault) ;
int _calculate_avg(int current_value) ;
void setup() ;
void loop() ;
void heatUpChamber() ;
void printStatus() ;
void error(String message) ;
void check_serial() ;
void check_PID() ;
float get_desired_temp() ;
int get_battery_percents(float voltage) ;
void set_desired_temp(byte new_temp) ;
void delay_with_interrupt(int time, int interrupting_button_state) ;
void vibrate_for_ms(int ms) ;

#include "vapoduino_fatty.ino"

#include "max31865.ino"

#endif
