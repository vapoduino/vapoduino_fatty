################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
/home/bene/Arduino/libraries/PID/PID_v1.cpp 

LINK_OBJ += \
./libraries/PID/PID_v1.cpp.o 

CPP_DEPS += \
./libraries/PID/PID_v1.cpp.d 


# Each subdirectory must supply rules for building sources it contributes
libraries/PID/PID_v1.cpp.o: /home/bene/Arduino/libraries/PID/PID_v1.cpp
	@echo 'Building file: $<'
	@echo 'Starting C++ compile'
	"/opt/arduino_ide//arduinoPlugin/packages/arduino/tools/avr-gcc/4.9.2-atmel3.5.4-arduino2/bin/avr-g++" -c -g -Os -Wall -Wextra -std=gnu++11 -fpermissive -fno-exceptions -ffunction-sections -fdata-sections -fno-threadsafe-statics -flto -mmcu=atmega328p -DF_CPU=8000000L -DARDUINO=10609 -DARDUINO_AVR_FIO -DARDUINO_ARCH_AVR   -I"/opt/arduino_ide/arduinoPlugin/packages/arduino/hardware/avr/1.6.18/cores/arduino" -I"/opt/arduino_ide/arduinoPlugin/packages/arduino/hardware/avr/1.6.18/variants/eightanaloginputs" -I"/home/bene/Arduino/libraries/PID" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -D__IN_ECLIPSE__=1 -x c++ "$<"  -o  "$@"
	@echo 'Finished building: $<'
	@echo ' '


