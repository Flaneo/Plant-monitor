# Plant-monitor
Monitoring of environmental conditions for plants with ESP6288 NodeMCU and Raspberry Pi Webserver.


This project is designed to monitor environmental conditions for plants using an ESP8266 microcontroller. The system collects data on soil moisture, temperature, and humidity from the DHT20 and soil moisture sensors. The data is sent to a Raspberry Pi server via WiFi, where it is processed and visualized through a web dashboard.

Features:
Real-time monitoring of soil moisture, temperature, and humidity for multiple plants.
Calibration mode for soil moisture sensor to ensure accurate readings for different soil types.
Adjustable measurement interval that can be updated remotely via the server.
Over-the-Air (OTA) programming support for easy firmware updates.
Customizable threshold values for each plant, enabling notifications or alerts when conditions go out of the ideal range.
Simple console commands for testing sensor functionality and calibrating sensors.

Components:
ESP8266 microcontroller
DHT20 temperature and humidity sensor
Soil moisture sensor
Raspberry Pi for data storage, processing, and visualization
This project is ideal for home gardening or smart plant monitoring systems, offering flexibility in configuring sensors and remotely adjusting settings.
