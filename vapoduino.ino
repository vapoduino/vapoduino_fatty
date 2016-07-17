/*
 * Created 04/28/2014 - vapoduino.ino
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

#include <SoftwareSerial.h>

#define MOSFET_GATE_PIN 5
#define BUTTON_PIN 3
#define LED_PIN 6
#define VIBRATOR_PIN 9
#define BATTERY_PIN A3
#define BT_RX_PIN 8
#define BT_TX_PIN 7

#define PID_P 11          // counter-"force"
#define PID_I 6        // counter-offset
#define PID_D 8          // dampen controlling, react on fast changes
#define PID_P_HEATING 3.6
#define PID_I_HEATING 0.2
#define PID_D_HEATING 4
#define MAX_HEATING_POWER 255

#define VOLTAGE_DIVIDER 0.01505277f

#define STANDBY_TIMEOUT 1500
#define TEMPERATURE_EEPROM_ADDRESS 0

#include <PID_v1.h>
#include <EEPROM.h>

double temp, output, desired_temp;
PID pid(&temp, &output, &desired_temp, PID_P, PID_I, PID_D, DIRECT);
SoftwareSerial btSerial(BT_RX_PIN, BT_TX_PIN);
boolean new_cycle;
int cycles;
int too_much_draw_counter;

// For smoother fading
const unsigned char cie[256] = {
        0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 
        1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 
        2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 
        3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 
        5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 
        7, 8, 8, 8, 8, 9, 9, 9, 10, 10, 
        10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 
        13, 14, 14, 15, 15, 15, 16, 16, 17, 17, 
        17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 
        22, 23, 23, 24, 24, 25, 25, 26, 26, 27, 
        28, 28, 29, 29, 30, 31, 31, 32, 32, 33, 
        34, 34, 35, 36, 37, 37, 38, 39, 39, 40, 
        41, 42, 43, 43, 44, 45, 46, 47, 47, 48, 
        49, 50, 51, 52, 53, 54, 54, 55, 56, 57, 
        58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 
        68, 70, 71, 72, 73, 74, 75, 76, 77, 79, 
        80, 81, 82, 83, 85, 86, 87, 88, 90, 91, 
        92, 94, 95, 96, 98, 99, 100, 102, 103, 105, 
        106, 108, 109, 110, 112, 113, 115, 116, 118, 120, 
        121, 123, 124, 126, 128, 129, 131, 132, 134, 136, 
        138, 139, 141, 143, 145, 146, 148, 150, 152, 154, 
        155, 157, 159, 161, 163, 165, 167, 169, 171, 173, 
        175, 177, 179, 181, 183, 185, 187, 189, 191, 193, 
        196, 198, 200, 202, 204, 207, 209, 211, 214, 216, 
        218, 220, 223, 225, 228, 230, 232, 235, 237, 240, 
        242, 245, 247, 250, 252, 255
};


void setup() {
    Serial.begin(9600);
    btSerial.begin(9600);
    Serial.println("#####################################");
    Serial.println("#####         VAPODUINO         #####");
    Serial.println("#####################################\n\n");
    
    // Set pinmodes
    pinMode(MOSFET_GATE_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT);
    pinMode(VIBRATOR_PIN, OUTPUT);

    desired_temp = get_desired_temp();
    
    // Setting up max31865
    while (!max_init()) {
        error("Could not initialize sensor!");
    }
    
    // Bugfix, don't know why...
    temp = max_get_temp();
    while (temp < 0) {
        // retrying, sometimes it takes some time
        error(" Reading negative temps... Retrying");
        temp = max_get_temp();
        Serial.print(temp);
        Serial.print(" ");
    }
    
    // Start PID control
    temp = max_get_temp();
    pid.SetMode(AUTOMATIC);
    
    
    new_cycle = true;
    too_much_draw_counter = 0;
    
    // Startup complete, waiting for userinput
    standby();
}

void loop() {
    temp = max_get_temp();
    
    digitalWrite(LED_PIN, LOW);
    digitalWrite(VIBRATOR_PIN, LOW);

    if (digitalRead(BUTTON_PIN) == HIGH) {
        // reset standby timeout     
        cycles = 0;
        
        if (new_cycle) {
            heatUpChamber();
            
            new_cycle = false;
            pid = PID(&temp, &output, &desired_temp, PID_P, PID_I, PID_D, DIRECT);
            pid.SetOutputLimits(0, 255);
            pid.SetMode(AUTOMATIC);
        }
        
        pid.Compute();
        analogWrite(MOSFET_GATE_PIN, output);
        if (output == 255) {
            too_much_draw_counter++;
            if (too_much_draw_counter > 20) {
                digitalWrite(LED_PIN, HIGH);
                digitalWrite(VIBRATOR_PIN, HIGH);
            }
        } else {
            too_much_draw_counter = 0;
            digitalWrite(LED_PIN, LOW);
            digitalWrite(VIBRATOR_PIN, LOW);
        }
    } else {
        cycles ++;
        
        // count till timeout
        if (cycles == STANDBY_TIMEOUT) {
            standby();
            cycles = 0;
        }
        pid.SetMode(MANUAL);
        output = 0;
        analogWrite(MOSFET_GATE_PIN, output);
        digitalWrite(LED_PIN, LOW);
        
        new_cycle = true;
        
        // debouncing the button
        delay(20);
    }
    printStatus();

    check_serial();
    
    delay(50);
}

void heatUpChamber() {
    // heat up chamber to desired_temp
    double heating_temp = desired_temp * 1.05;
    pid = PID(&temp, &output, &heating_temp, PID_P_HEATING, PID_I_HEATING, PID_D_HEATING, DIRECT);
    pid.SetOutputLimits(0, MAX_HEATING_POWER);
    pid.SetMode(AUTOMATIC);
    
    while (digitalRead(BUTTON_PIN) == HIGH && temp < desired_temp) {
        temp = max_get_temp();
        
        digitalWrite(LED_PIN, HIGH);
        pid.Compute();
        analogWrite(MOSFET_GATE_PIN, output);
        printStatus();
        
        delay(50);
    }
    
    if (temp > desired_temp) {
        vibrate_for_ms(200);
    }
}

void printStatus() {
    int raw = analogRead(A3);
    Serial.print((int) desired_temp);
    Serial.print(";");
    Serial.print(temp);
    Serial.print(";");
    Serial.print(output);
    Serial.print(";");
    Serial.println(raw * VOLTAGE_DIVIDER);
    
    btSerial.print((int) desired_temp);
    btSerial.print(";");
    btSerial.print(temp);
    btSerial.print(";");
    btSerial.print(output);
    btSerial.print(";");
    btSerial.println(raw * VOLTAGE_DIVIDER);
}

void error(String message) {
    Serial.println(message);
    digitalWrite(MOSFET_GATE_PIN, LOW);
}

void standby() {
    Serial.println("Standby");
    digitalWrite(MOSFET_GATE_PIN, LOW);
    while (digitalRead(BUTTON_PIN) == LOW) {
        for(int fadeValue = 0 ; fadeValue <= 255; fadeValue++) {
            if (digitalRead(BUTTON_PIN) == HIGH) {
                break;
            }
            analogWrite(LED_PIN, cie[fadeValue]);
            delay(7); 
        } 
        for(int fadeValue = 255 ; fadeValue >= 0; fadeValue--) {
            if (digitalRead(BUTTON_PIN) == HIGH) {
                break;
            }
            analogWrite(LED_PIN, cie[fadeValue]); 
            delay(7);              
        }
        delay_with_interrupt(1000, HIGH);
    }
    temp = max_get_temp();
}

void check_serial() {
    if (btSerial.available()) {
        char command = btSerial.read();
        byte value = 0;
        switch (command) {
            case 't':
                while (!btSerial.available());
                set_desired_temp(btSerial.read());
                break;
            default:
                btSerial.println("Unknown Command");
        }
    }
}

float get_desired_temp() {
    return (float) EEPROM.read(TEMPERATURE_EEPROM_ADDRESS);
}

void set_desired_temp(byte new_temp) {
    EEPROM.write(TEMPERATURE_EEPROM_ADDRESS, new_temp);
    desired_temp = (float) new_temp;
}

void delay_with_interrupt(int time, int interrupting_button_state) {
  int counter = 0;
    while (digitalRead(BUTTON_PIN) != interrupting_button_state & counter < time) {
        counter++;
        delay(1);
    }
    // debounce
    delay(20);
}

void vibrate_for_ms(int ms) {
    digitalWrite(VIBRATOR_PIN, HIGH);
    delay(ms);
    digitalWrite(VIBRATOR_PIN, LOW);
}
