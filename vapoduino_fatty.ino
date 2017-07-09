#include <Arduino.h>

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

#define PID_P 19 // counter-"force" http://www.ascii-code.com/
#define PID_I 12 // counter-offset
#define PID_D 12 // dampen controlling, react on fast changes
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

double temp, output, desired_temp;
double pid_p, pid_i, pid_d;
PID pid(&temp, &output, &desired_temp, pid_p, pid_i, pid_d, DIRECT);
SoftwareSerial bt_serial(BT_RX_PIN, BT_TX_PIN);
boolean new_cycle;
int cycles;
int too_much_draw_counter;

void setup() {
    Serial.begin(9600);
    bt_serial.begin(9600);
    Serial.println("#####################################");
    Serial.println("#####         VAPODUINO         #####");
    Serial.println("#####################################\n\n");

    // Set pinmodes
    pinMode(MOSFET_GATE_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT);
    pinMode(VIBRATOR_PIN, OUTPUT);

    desired_temp = get_desired_temp();

    pid_p = PID_P;
    pid_i = PID_I;
    pid_d = PID_D;

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

    digitalWrite(LED_PIN, HIGH);
    vibrate_for_ms(500);
    digitalWrite(LED_PIN, LOW);
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
            pid = PID(&temp, &output, &desired_temp, pid_p, pid_i, pid_d, DIRECT);
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
            vibrate_for_ms(1000);
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
    Serial.print(pid_p);
    Serial.print(";");
    Serial.print(pid_i);
    Serial.print(";");
    Serial.print(pid_d);
    Serial.print(";");
    Serial.print(temp);
    Serial.print(";");
    Serial.print(output);
    Serial.print(";");
    Serial.print(raw * VOLTAGE_DIVIDER);
    Serial.print(";");
    Serial.println(get_battery_percents(raw * VOLTAGE_DIVIDER));

    bt_serial.print((int) desired_temp);
    bt_serial.print(";");
    bt_serial.print(temp);
    bt_serial.print(";");
    bt_serial.print(output);
    bt_serial.print(";");
    bt_serial.print(raw * VOLTAGE_DIVIDER);
    bt_serial.print(";");
    bt_serial.println(get_battery_percents(raw * VOLTAGE_DIVIDER));
}

void error(String message) {
    Serial.println(message);
    digitalWrite(MOSFET_GATE_PIN, LOW);
}

void check_serial() {
    if (bt_serial.available()) {
        digitalWrite(MOSFET_GATE_PIN, LOW);
        char command = bt_serial.read();
        byte value = 0;
        switch (command) {
            case 't':
                while (!bt_serial.available());
                set_desired_temp(bt_serial.read());
                break;
            default:
                bt_serial.println("Unknown Command");
        }
    }

    if (Serial.available()) {
        digitalWrite(MOSFET_GATE_PIN, LOW);
        if (Serial.read() == 'c') {
            check_PID();
        }
    }
}

void check_PID() {
    while (!Serial.available());
    switch(Serial.read()) {
        case 'p':
            while (!Serial.available());
            pid_p = ((float) Serial.read()) / 10;
            break;
        case 'i':
            while (!Serial.available());
            pid_i = ((float) Serial.read()) / 10;
            break;
        case 'd':
            while (!Serial.available());
            pid_d = ((float) Serial.read()) / 10;
            break;
    }
}

float get_desired_temp() {
    return (float) EEPROM.read(TEMPERATURE_EEPROM_ADDRESS);
}

int get_battery_percents(float voltage) {
    int percent = (voltage - MINIMUM_VOLTAGE)/(MAXIMUM_VOLTAGE - MINIMUM_VOLTAGE) * 100;
    return percent < 0 ? 0 : percent;
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
