// plant_monitor.ino

#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <DHT20.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

// WiFi Konfiguration
const char* ssid = "Your-Wifi-SSID";          // Dein WiFi-SSID
const char* password = "Your-Wifi-Password";  // Dein WiFi-Passwort

// Server Konfiguration
const char* host = "192.168.2.40"; // IP-Adresse des Raspberry Pi
const int port = 5000;             // Port der Flask-App
const char* dataEndpoint = "/api/data"; // Endpoint für Daten
const char* intervalEndpoint = "/api/get_interval"; // Endpoint für Intervall

// Bodenfeuchtesensor
const int soilMoisturePin = A0; // Analog-Pin für Bodenfeuchtesensor

// EEPROM Adressen für Kalibrierungswerte
const int EEPROM_SIZE = 4; // 2 Bytes für airValue, 2 Bytes für waterValue
const int AIR_VALUE_ADDR = 0;
const int WATER_VALUE_ADDR = 2;

// Standard Kalibrierungswerte (können angepasst werden)
int airValue = 1023;   // Sensorwert in trockener Erde
int waterValue = 0;    // Sensorwert in Wasser

// DHT20 Sensor Setup
DHT20 dht20;

// Messintervall in Millisekunden (Standard 60 Sekunden)
unsigned long intervalMillis = 60000;
unsigned long previousMillis = 0;

// NTP Client Setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200, 60000); // UTC+2, Aktualisierung alle 60 Sekunden

// Standort des Sensors (Pflanze)
// Für "Monstera"
const char* location = "monstera";
IPAddress local_IP(192, 168, 2, 201); // IP-Adresse für den Monstera ESP

// Für "andere"
//const char* location = "andere";
//IPAddress local_IP(192, 168, 2, 202); // IP-Adresse für den "andere" ESP

// Für "Buschkopf"
//const char* location = "buschkopf";
//IPAddress local_IP(192, 168, 2, 203); // IP-Adresse für den Buschkopf ESP

IPAddress gateway(192, 168, 2, 1);      // IP-Adresse deines Routers
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);       // Optional
IPAddress secondaryDNS(8, 8, 4, 4);     // Optional

// Flag für Kalibrierungsmodus
bool calibrationMode = false;

// Prototypen der Funktionen
void calibrateDry();
void calibrateWet();
void showCalibration();
void resetCalibration();
void enterCalibrationMode();
void exitCalibrationMode();
void handleSerialCommands();
float calculateSoilMoisture(int sensorValue);
void sendData(float soilMoisture, float temperature, float humidity);
String getISOTimestamp();
void getDateTime(unsigned long epochTime, int &year, int &month, int &day, int &hour, int &minute, int &second);
void updateInterval();
void setUpOverTheAirProgramming();
void loadCalibration();
void saveCalibration();
void testSensors(); // Neue Funktion für den "TEST" Befehl

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println("----- Plant Monitor Start -----");

  // Initialisiere EEPROM
  EEPROM.begin(EEPROM_SIZE);
  loadCalibration();

  Serial.print("Kalibrierungswerte geladen - airValue: ");
  Serial.print(airValue);
  Serial.print(", waterValue: ");
  Serial.println(waterValue);

  // Statische IP-Konfiguration
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Statische IP-Konfiguration fehlgeschlagen.");
  } else {
    Serial.println("Statische IP-Konfiguration erfolgreich.");
    Serial.print("Statische IP: ");
    Serial.println(WiFi.localIP());
  }

  // Verbinde mit WiFi
  Serial.println();
  Serial.println();
  Serial.print("Verbinde mit WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  Serial.println("WiFi-Verbindungsversuch gestartet...");

  // Warte auf Verbindung
  unsigned long startAttemptTime = millis();
  const unsigned long maxWait = 15000; // Max. Wartezeit 15 Sekunden

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < maxWait) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi verbunden");
    Serial.print("Verbundene SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP Adresse: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("WiFi Verbindung fehlgeschlagen!");
  }

  setUpOverTheAirProgramming();

  // Starte den NTP Client
  timeClient.begin();
  Serial.println("NTP Client gestartet.");

  // Initialisiere den DHT20 Sensor
  Wire.begin();
  Serial.println("Initialisiere DHT20 Sensor...");
  if (!dht20.begin()) {
    Serial.println("DHT20 Sensor nicht gefunden. Bitte überprüfe die Verbindung!");
  } else {
    Serial.println("DHT20 Sensor erfolgreich initialisiert.");
  }

  // Initiales Abrufen des Intervalls
  Serial.println("Rufe initiales Messintervall ab...");
  updateInterval();

  // Initiale Sensorwerte auslesen und ausgeben
  Serial.println("Lese initiale Sensorwerte...");
  int initialSoilMoistureValue = analogRead(soilMoisturePin);
  Serial.print("Initialer Rohwert Bodenfeuchte (analogRead): ");
  Serial.println(initialSoilMoistureValue);

  float initialSoilMoisturePercent = calculateSoilMoisture(initialSoilMoistureValue);
  Serial.print("Initiale Bodenfeuchte: ");
  Serial.print(initialSoilMoisturePercent);
  Serial.println(" %");

  Serial.println("Lese initiale DHT20 Sensorwerte...");
  int initialStatus = dht20.read();
  float initialTemperature = 0;
  float initialHumidity = 0;

  if (initialStatus == DHT20_OK) {
    initialTemperature = dht20.getTemperature();
    initialHumidity = dht20.getHumidity();

    // Temperatur und Luftfeuchtigkeit auf eine Nachkommastelle runden
    initialTemperature = round(initialTemperature * 10) / 10.0;
    initialHumidity = round(initialHumidity * 10) / 10.0;

    Serial.print("Standort: ");
    Serial.println(location);
    Serial.print("Bodenfeuchte Wert: ");
    Serial.print(initialSoilMoistureValue);
    Serial.print(" | Bodenfeuchte: ");
    Serial.print(initialSoilMoisturePercent);
    Serial.print(" % | Temperatur: ");
    Serial.print(initialTemperature);
    Serial.print(" °C | Luftfeuchtigkeit: ");
    Serial.print(initialHumidity);
    Serial.println(" %");
  } else {
    Serial.print("Initialer DHT20 Fehler: ");
    Serial.println(initialStatus);
  }

  Serial.println("----- Setup abgeschlossen -----\n");

  // Übersicht der verfügbaren Befehle anzeigen
  Serial.println("----- Verfügbare Befehle -----");
  Serial.println("Normaler Modus:");
  Serial.println("  ENTER CALIBRATION MODE - Betritt den Kalibrierungsmodus.");
  Serial.println("  TEST - Gibt alle aktuellen Sensorwerte aus.");
  Serial.println("\nKalibrierungsmodus:");
  Serial.println("  CALIBRATE DRY - Setzt den aktuellen Sensorwert als trockenen Zustand.");
  Serial.println("  CALIBRATE WET - Setzt den aktuellen Sensorwert als nassen Zustand.");
  Serial.println("  SHOW CALIBRATION - Zeigt die aktuellen Kalibrierungswerte an.");
  Serial.println("  RESET CALIBRATION - Setzt die Kalibrierungswerte auf Standard.");
  Serial.println("  EXIT CALIBRATION MODE - Verlässt den Kalibrierungsmodus.");
  Serial.println("--------------------------------\n");
}

void loop() {
  unsigned long currentMillis = millis();

  // Bearbeite ArduinoOTA
  ArduinoOTA.handle();

  // Überprüfe auf serielle Eingaben für Kalibrierungsbefehle
  handleSerialCommands();

  // Überprüfe, ob das Intervall abgelaufen ist
  if (currentMillis - previousMillis >= intervalMillis && !calibrationMode) {
    previousMillis = currentMillis;

    Serial.println("\n----- Messzyklus gestartet -----");

    // Aktualisiere das Intervall vom Server
    Serial.println("Aktualisiere Messintervall vom Server...");
    updateInterval();

    // Aktualisiere die Zeit
    if (!timeClient.update()) {
      Serial.println("NTP Client Update fehlgeschlagen. Erzwinge Update...");
      timeClient.forceUpdate();
      if (timeClient.update()) {
        Serial.println("NTP Client erfolgreich aktualisiert.");
      } else {
        Serial.println("NTP Client Update immer noch fehlgeschlagen.");
      }
    } else {
      Serial.println("NTP Client erfolgreich aktualisiert.");
    }

    // Lese den Bodenfeuchtesensor
    Serial.println("Lese Bodenfeuchtesensor...");
    int soilMoistureValue = analogRead(soilMoisturePin);
    Serial.print("Rohwert Bodenfeuchte (analogRead): ");
    Serial.println(soilMoistureValue);

    float soilMoisturePercent = calculateSoilMoisture(soilMoistureValue);
    Serial.print("Bodenfeuchte: ");
    Serial.print(soilMoisturePercent);
    Serial.println(" %");

    // Lese den DHT20 Sensor
    Serial.println("Lese DHT20 Sensor...");
    int status = dht20.read();
    float temperature = 0;
    float humidity = 0;

    if (status == DHT20_OK) {
      temperature = dht20.getTemperature();
      humidity = dht20.getHumidity();

      // Temperatur und Luftfeuchtigkeit auf eine Nachkommastelle runden
      temperature = round(temperature * 10) / 10.0;
      humidity = round(humidity * 10) / 10.0;

      Serial.print("Standort: ");
      Serial.println(location);
      Serial.print("Bodenfeuchte Wert: ");
      Serial.print(soilMoistureValue);
      Serial.print(" | Bodenfeuchte: ");
      Serial.print(soilMoisturePercent);
      Serial.print(" % | Temperatur: ");
      Serial.print(temperature);
      Serial.print(" °C | Luftfeuchtigkeit: ");
      Serial.print(humidity);
      Serial.println(" %");

      // Sende die Daten an den Raspberry Pi
      Serial.println("Sende Daten an den Server...");
      sendData(soilMoisturePercent, temperature, humidity);
    } else {
      Serial.print("DHT20 Fehler: ");
      Serial.println(status);
    }

    Serial.println("----- Messzyklus beendet -----\n");
  }
}

// Funktion zur Handhabung von seriellen Befehlen für die Kalibrierung und TEST
void handleSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim(); // Entferne führende und folgende Leerzeichen

    if (calibrationMode) {
      if (command.equalsIgnoreCase("CALIBRATE DRY")) {
        calibrateDry();
      }
      else if (command.equalsIgnoreCase("CALIBRATE WET")) {
        calibrateWet();
      }
      else if (command.equalsIgnoreCase("SHOW CALIBRATION")) {
        showCalibration();
      }
      else if (command.equalsIgnoreCase("RESET CALIBRATION")) {
        resetCalibration();
      }
      else if (command.equalsIgnoreCase("EXIT CALIBRATION MODE")) {
        exitCalibrationMode();
      }
      else {
        Serial.println("Unbekannter Befehl im Kalibrierungsmodus.");
        Serial.println("Verfügbare Befehle:");
        Serial.println("  CALIBRATE DRY - Setzt den aktuellen Sensorwert als trockenen Zustand.");
        Serial.println("  CALIBRATE WET - Setzt den aktuellen Sensorwert als nassen Zustand.");
        Serial.println("  SHOW CALIBRATION - Zeigt die aktuellen Kalibrierungswerte an.");
        Serial.println("  RESET CALIBRATION - Setzt die Kalibrierungswerte auf Standard.");
        Serial.println("  EXIT CALIBRATION MODE - Verlässt den Kalibrierungsmodus.");
      }
    }
    else {
      if (command.equalsIgnoreCase("ENTER CALIBRATION MODE")) {
        enterCalibrationMode();
      }
      else if (command.equalsIgnoreCase("TEST")) {
        testSensors();
      }
      else {
        Serial.println("Unbekannter Befehl. Verfügbare Befehle:");
        Serial.println("  ENTER CALIBRATION MODE - Betritt den Kalibrierungsmodus.");
        Serial.println("  TEST - Gibt alle aktuellen Sensorwerte aus.");
      }
    }
  }
}

// Funktion zur Kalibrierung des Bodenfeuchtigkeitssensors im trockenen Zustand
void calibrateDry() {
  int sensorValue = analogRead(soilMoisturePin);
  airValue = sensorValue;
  Serial.print("Kalibrierung abgeschlossen: airValue = ");
  Serial.println(airValue);
  saveCalibration();
}

// Funktion zur Kalibrierung des Bodenfeuchtigkeitssensors im nassen Zustand
void calibrateWet() {
  int sensorValue = analogRead(soilMoisturePin);
  waterValue = sensorValue;
  Serial.print("Kalibrierung abgeschlossen: waterValue = ");
  Serial.println(waterValue);
  saveCalibration();
}

// Funktion zur Anzeige der aktuellen Kalibrierungswerte
void showCalibration() {
  Serial.print("Aktuelle Kalibrierungswerte - airValue: ");
  Serial.print(airValue);
  Serial.print(", waterValue: ");
  Serial.println(waterValue);
}

// Funktion zur Rücksetzung der Kalibrierungswerte auf Standardwerte
void resetCalibration() {
  airValue = 1023;
  waterValue = 0;
  Serial.println("Kalibrierungswerte zurückgesetzt auf Standardwerte.");
  saveCalibration();
}

// Funktion zur Eingabe in den Kalibrierungsmodus
void enterCalibrationMode() {
  calibrationMode = true;
  Serial.println("----- Kalibrierungsmodus gestartet -----");
  Serial.println("Gib 'CALIBRATE DRY' ein, wenn der Sensor in trockener Erde ist.");
  Serial.println("Gib 'CALIBRATE WET' ein, wenn der Sensor in Wasser ist.");
  Serial.println("Gib 'SHOW CALIBRATION' ein, um die aktuellen Kalibrierungswerte anzuzeigen.");
  Serial.println("Gib 'RESET CALIBRATION' ein, um die Kalibrierungswerte zurückzusetzen.");
  Serial.println("Gib 'EXIT CALIBRATION MODE' ein, um den Kalibrierungsmodus zu verlassen.");
}

// Funktion zur Verlassen des Kalibrierungsmodus
void exitCalibrationMode() {
  calibrationMode = false;
  Serial.println("----- Kalibrierungsmodus beendet -----");
  Serial.println("----- Verfügbare Befehle -----");
  Serial.println("Normaler Modus:");
  Serial.println("  ENTER CALIBRATION MODE - Betritt den Kalibrierungsmodus.");
  Serial.println("  TEST - Gibt alle aktuellen Sensorwerte aus.");
  Serial.println("\nKalibrierungsmodus:");
  Serial.println("  CALIBRATE DRY - Setzt den aktuellen Sensorwert als trockenen Zustand.");
  Serial.println("  CALIBRATE WET - Setzt den aktuellen Sensorwert als nassen Zustand.");
  Serial.println("  SHOW CALIBRATION - Zeigt die aktuellen Kalibrierungswerte an.");
  Serial.println("  RESET CALIBRATION - Setzt die Kalibrierungswerte auf Standard.");
  Serial.println("  EXIT CALIBRATION MODE - Verlässt den Kalibrierungsmodus.");
  Serial.println("--------------------------------\n");
}

// Funktion zur Berechnung der Bodenfeuchtigkeit in Prozent
float calculateSoilMoisture(int sensorValue) {
  // Konvertiere den Analogwert in einen Prozentwert (0-100%)
  // Diese Werte müssen kalibriert werden
  int mappedValue = map(sensorValue, airValue, waterValue, 0, 100);
  if (mappedValue < 0)
    mappedValue = 0;
  if (mappedValue > 100)
    mappedValue = 100;

  Serial.print("Berechneter Bodenfeuchtigkeitsprozentsatz: ");
  Serial.print(mappedValue);
  Serial.println(" %");

  return mappedValue;
}

// Funktion zum Senden der Daten an den Server
void sendData(float soilMoisture, float temperature, float humidity) {
  WiFiClient client;
  Serial.print("Verbinde mit Server: ");
  Serial.println(host);

  if (!client.connect(host, port)) {
    Serial.println("Verbindung zum Server fehlgeschlagen");
    return;
  }

  Serial.println("Verbindung erfolgreich hergestellt.");

  // Erstelle JSON-Daten
  StaticJsonDocument<256> doc;
  doc["location"] = location;
  doc["soil_moisture"] = soilMoisture;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["timestamp"] = getISOTimestamp();

  String jsonData;
  serializeJson(doc, jsonData);

  Serial.print("JSON Daten, die gesendet werden: ");
  Serial.println(jsonData);

  // Sende HTTP POST Anfrage
  client.println(String("POST ") + dataEndpoint + " HTTP/1.1");
  client.println(String("Host: ") + host);
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(jsonData.length());
  client.println();
  client.println(jsonData);

  // Warte auf Antwort
  Serial.println("Warte auf Antwort vom Server...");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }

  // Lese die Antwort des Servers
  String response = client.readString();
  Serial.println("Antwort vom Server:");
  Serial.println(response);

  client.stop();
  Serial.println("Verbindung zum Server geschlossen.");
}

// Funktion zur Generierung des ISO Timestamps
String getISOTimestamp() {
  // Holt die aktuelle Zeit vom NTP Client und formatiert sie als ISO 8601
  String isoTimestamp = "";
  if (timeClient.isTimeSet()) {
    unsigned long epochTime = timeClient.getEpochTime();
    // Verwandle epochTime in ein DateTime Objekt
    int year, month, day, hour, minute, second;
    getDateTime(epochTime, year, month, day, hour, minute, second);
    char buffer[30];
    sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d+02:00", year, month, day, hour, minute, second);
    isoTimestamp = String(buffer);
    Serial.print("Generierter ISO Timestamp: ");
    Serial.println(isoTimestamp);
  } else {
    // Fallback, falls die Zeit nicht gesetzt ist
    isoTimestamp = "1970-01-01T00:00:00+00:00";
    Serial.println("Zeit nicht gesetzt. Fallback ISO Timestamp: 1970-01-01T00:00:00+00:00");
  }
  return isoTimestamp;
}

// Funktion zur Umwandlung von epochTime in Datum und Uhrzeit
void getDateTime(unsigned long epochTime, int &year, int &month, int &day, int &hour, int &minute, int &second) {
  // Berechne Datum und Uhrzeit aus epochTime
  time_t rawTime = epochTime;
  struct tm * ti;
  ti = gmtime(&rawTime); // Verwende UTC Zeit

  year = ti->tm_year + 1900;
  month = ti->tm_mon + 1;
  day = ti->tm_mday;
  hour = ti->tm_hour;
  minute = ti->tm_min;
  second = ti->tm_sec;

  Serial.print("Datum und Uhrzeit: ");
  Serial.print(year); Serial.print("-");
  Serial.print(month); Serial.print("-");
  Serial.print(day); Serial.print(" ");
  Serial.print(hour); Serial.print(":");
  Serial.print(minute); Serial.print(":");
  Serial.println(second);
}

// Funktion zur Aktualisierung des Messintervalls vom Server
void updateInterval() {
  WiFiClient client;
  Serial.print("Verbinde mit Server für Intervallabfrage: ");
  Serial.println(host);

  if (client.connect(host, port)) {
    Serial.println("Verbindung erfolgreich. Sende GET Anfrage für Intervall...");
    client.println(String("GET ") + intervalEndpoint + " HTTP/1.1");
    client.println(String("Host: ") + host);
    client.println("Connection: close");
    client.println();

    // Warte auf Antwort
    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        break;
      }
    }

    // Lese die Antwort des Servers
    String response = client.readString();
    Serial.println("Antwort vom Server (Intervall):");
    Serial.println(response);

    // Parse das JSON
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, response);
    if (!error) {
      int intervalMinutes = doc["interval"];
      intervalMillis = intervalMinutes * 60000UL;
      Serial.print("Neues Intervall gesetzt: ");
      Serial.print(intervalMinutes);
      Serial.println(" Minuten");
    } else {
      Serial.print("Fehler beim Parsen des Intervalls: ");
      Serial.println(error.c_str());
    }

    client.stop();
    Serial.println("Verbindung zum Server geschlossen (Intervallabfrage).");
  } else {
    Serial.println("Verbindung zum Server fehlgeschlagen (Intervall abrufen).");
  }
}

// Funktion zur Einrichtung der Over-The-Air Programmierung
void setUpOverTheAirProgramming() {
  // Ändere den OTA-Port (Standard: 8266)
  // ArduinoOTA.setPort(8266);

  // Ändere den Hostnamen für die Anzeige in der Arduino IDE
  String programming_name = String("esp-") + location;
  ArduinoOTA.setHostname(programming_name.c_str());
  Serial.print("OTA Hostname gesetzt auf: ");
  Serial.println(programming_name);

  // Passwort für OTA (optional)
  // ArduinoOTA.setPassword("dein_passwort");

  ArduinoOTA.onStart([]() {
    Serial.println("OTA Update gestartet...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Update beendet.");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Fortschritt: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Fehler[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Authentifizierungsfehler");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Fehler");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Verbindungsfehler");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Empfangsfehler");
    else if (error == OTA_END_ERROR) Serial.println("End Fehler");
  });

  ArduinoOTA.begin();
  Serial.println("Over-The-Air Programmierung (OTA) bereit.");
}

// Funktion zur Laden der Kalibrierungswerte aus dem EEPROM
void loadCalibration() {
  if (EEPROM.length() < EEPROM_SIZE) {
    Serial.println("EEPROM Größe ist zu klein. Kalibrierungswerte können nicht geladen werden.");
    airValue = 1023;
    waterValue = 0;
    return;
  }

  airValue = (EEPROM.read(AIR_VALUE_ADDR) << 8) | EEPROM.read(AIR_VALUE_ADDR + 1);
  waterValue = (EEPROM.read(WATER_VALUE_ADDR) << 8) | EEPROM.read(WATER_VALUE_ADDR + 1);

  // Überprüfe, ob die Werte plausibel sind
  if (airValue < 0 || airValue > 1023) {
    airValue = 1023;
    Serial.println("Ungültiger airValue erkannt. Setze auf Standardwert 1023.");
  }
  if (waterValue < 0 || waterValue > 1023) {
    waterValue = 0;
    Serial.println("Ungültiger waterValue erkannt. Setze auf Standardwert 0.");
  }
}

// Funktion zur Speicherung der Kalibrierungswerte im EEPROM
void saveCalibration() {
  EEPROM.write(AIR_VALUE_ADDR, highByte(airValue));
  EEPROM.write(AIR_VALUE_ADDR + 1, lowByte(airValue));
  EEPROM.write(WATER_VALUE_ADDR, highByte(waterValue));
  EEPROM.write(WATER_VALUE_ADDR + 1, lowByte(waterValue));
  EEPROM.commit();
  Serial.println("Kalibrierungswerte im EEPROM gespeichert.");
}

// Neue Funktion: TEST - Gibt alle aktuellen Sensorwerte aus
void testSensors() {
  Serial.println("----- TEST-Befehl ausgeführt -----");

  // Lese Bodenfeuchtesensor
  int soilMoistureValue = analogRead(soilMoisturePin);
  Serial.print("Rohwert Bodenfeuchte (analogRead): ");
  Serial.println(soilMoistureValue);

  float soilMoisturePercent = calculateSoilMoisture(soilMoistureValue);
  Serial.print("Bodenfeuchte: ");
  Serial.print(soilMoisturePercent);
  Serial.println(" %");

  // Lese DHT20 Sensor
  Serial.println("Lese DHT20 Sensor...");
  int status = dht20.read();
  float temperature = 0;
  float humidity = 0;

  if (status == DHT20_OK) {
    temperature = dht20.getTemperature();
    humidity = dht20.getHumidity();

    // Temperatur und Luftfeuchtigkeit auf eine Nachkommastelle runden
    temperature = round(temperature * 10) / 10.0;
    humidity = round(humidity * 10) / 10.0;

    Serial.print("Standort: ");
    Serial.println(location);
    Serial.print("Bodenfeuchte Wert: ");
    Serial.print(soilMoistureValue);
    Serial.print(" | Bodenfeuchte: ");
    Serial.print(soilMoisturePercent);
    Serial.print(" % | Temperatur: ");
    Serial.print(temperature);
    Serial.print(" °C | Luftfeuchtigkeit: ");
    Serial.print(humidity);
    Serial.println(" %");
  } else {
    Serial.print("DHT20 Fehler: ");
    Serial.println(status);
  }

  Serial.println("----- Ende des TEST-Befehls -----\n");
}
