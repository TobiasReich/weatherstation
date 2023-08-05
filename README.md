# weatherstation
A local weather station for testing ESP32 sensors

## Overview

The idea is to
- use a cheap and low energy consuming ESP32 as base
- use a bunch of sensors for getting an idea of the environment
- draw the data on a waveshare e-paper display


## Devices

ESP32 device:
https://www.waveshare.com/wiki/E-Paper_ESP32_Driver_Board

E-Paper display:
https://www.waveshare.com/4.2inch-e-paper-module.htm

Sensors:
https://www.sunfounder.com/products/sensor-kit-v2-for-arduino?_pos=2&_sid=1bdf16cc9&_ss=r

I use explicitly the
- Barometer
- Raindrop Sensor
- Humidity Sensor

Furthermore the device uses a soil moisture senesor from here:

https://www.seeedstudio.com/Grove-Capacitive-Moisture-Sensor-Corrosion-Resistant.html


# Pinouts (for now)

## Barometer

Black - GROUND
Red - 3.3V
White - GPIO 21
Brown - GPIO 22

## Raindrop Sensor

This is connected via a Raindrop Sensor (which connects again to the device).

Black - GROUND
Red - 3.3V
White - GPIO 36 (ANALOG AO)
Brown - GPIO 25 (D0)


## Humiture Sensor

## Soil Moisture Sensor
