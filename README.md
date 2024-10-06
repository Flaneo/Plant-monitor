# Plant Monitoring System with ESP8266, DHT20, and Soil Moisture Sensor

This project is designed to monitor environmental conditions for plants using an ESP8266 microcontroller. The system collects data on soil moisture, temperature, and humidity from the DHT20 and soil moisture sensors. The data is sent to a Raspberry Pi server via WiFi, where it is processed and visualized through a web dashboard.

### Features:
- **Real-time monitoring** of soil moisture, temperature, and humidity for multiple plants.
- **Calibration mode** for soil moisture sensor to ensure accurate readings for different soil types.
- **Adjustable measurement interval** that can be updated remotely via the server.
- **Over-the-Air (OTA) programming** support for easy firmware updates.
- **Customizable threshold values** for each plant, enabling notifications or alerts when conditions go out of the ideal range.
- **Simple console commands** for testing sensor functionality and calibrating sensors.

### 
### Components:
- [**ESP8266** **NodeMCU** microcontroller](https://www.amazon.de/dp/B0D8WGY1W3?ref=ppx_yo2ov_dt_b_fed_asin_title)
- [**DHT20** temperature and humidity sensor](https://www.amazon.de/dp/B0CSMX7358?ref=ppx_yo2ov_dt_b_fed_asin_title)
- [**Soil moisture sensor**](https://www.amazon.de/dp/B07V4KXZ35?ref=ppx_yo2ov_dt_b_fed_asin_title)
- [**Raspberry Pi** for data storage, processing, and visualization](https://www.amazon.de/Raspberry-1373331-Modell-Mainboard-1GB/dp/B07BFH96M3/ref=sr_1_14?dib=eyJ2IjoiMSJ9.EIRWMNAsT_JPS2JI4yVop3_AT54DGyDI1Wi-wi24XSkDPSsGipaFuqM06y8w89Rcys8jbbnFI6rbNyr6lKyOwJzwXA6hpP1FnsAZzUvdBG9KXQs4ObogNfGTsvJyPSDsQR8K77lrEaG4hdo7YxM-bx833rmORuXMU7DgBOQrGnj0aW-02zgYQmDD3nWX8JmnuFg6u-AmkRpve3wHbFNtJh38U5tfmtYjRaLG6oaZ1qM.xm3yBCPrPmQcKIfja2PgjPkxufXIY0crjEM9UmInIaI&dib_tag=se&keywords=raspberry+pi&qid=1728238210&sr=8-14)



# **Installation:**

- **Set up your Raspberry Pi and connect it to your WiFi.**


- **Go to your Router Configuration and set the IPV4 of your Raspberry Pi to static.**


- **Install Required Arduino Libraries**

Open Arduino IDE.
Go to Sketch > Include Library > Manage Libraries.
Install the following libraries:
> ArduinoJson by Benoit Blanchon
> NTPClient by Fabrice Weinberg
> DHT20 by Emerson F. S.
> ArduinoOTA (usually included with ESP8266 boards)
> EEPROM (usually included with Arduino IDE)

- **Configure the following in the script:**

WiFi Credentials:
> const char* ssid = "Your_WiFi_SSID";
> const char* password = "Your_WiFi_Password";
- Server Configuration:

> const char* host = "Your_Raspberry_Pi_IP"; // e.g., "192.168.2.40"

- **Plant selection**

Select the plat by commenting out if removing the comment flag for the following:
> // Standort des Sensors (Pflanze)
> // Für "Monstera"
> const char* location = "monstera";
> IPAddress local_IP(192, 168, 2, 201); // IP-Adresse für den Monstera ESP
> 
> // Für "andere"
> //const char* location = "andere";
> //IPAddress local_IP(192, 168, 2, 202); // IP-Adresse für den "andere" ESP
> 
> // Für "Buschkopf"
> //const char* location = "buschkopf";
> //IPAddress local_IP(192, 168, 2, 203); // IP-Adresse für den Buschkopf ESP

- **Upload the script to your NodeMCU and start the calibration the Soil moisture sensor by typing "ENTER CALIBRATION MODE" in the Serial Monitor.**


- **Clone Repository to Raspberry Pi**


- **Install the required Python packages:**

> pip3 install -r requirements.txt

- **Run the Server**

> python3 server.py

### Set Up Auto-Start (Optional)
To ensure the server starts on boot:
1. Create a systemd service file:

> sudo nano /etc/systemd/system/plant-monitor.service
1. Add the following content:

> [Unit]
> Description=Plant Monitoring Server
> After=network.target
> 
> [Service]
> ExecStart=/usr/bin/python3 /home/pi/plant-monitoring-system/server.py
> WorkingDirectory=/home/pi/plant-monitoring-system/
> StandardOutput=inherit
> StandardError=inherit
> Restart=always
> User=pi
> 
> [Install]
> WantedBy=multi-user.target
- **Note**: Adjust the paths if your project is located elsewhere.
1. Enable and start the service:

> sudo systemctl enable plant-monitor.service
> sudo systemctl start plant-monitor.service
1. Check the status:

> sudo systemctl status plant-monitor.service
