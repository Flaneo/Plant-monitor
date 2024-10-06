# server.py

from flask import Flask, request, jsonify, render_template
from flask_cors import CORS
from flask_socketio import SocketIO, emit
import json
import os
import shutil
from datetime import datetime
import logging

# Logging konfigurieren
logging.basicConfig(level=logging.DEBUG)

app = Flask(__name__)
CORS(app)

# SocketIO initialisieren mit eventlet und CORS erlauben
socketio = SocketIO(app, cors_allowed_origins='*')

DATA_DIR = 'data'
CONFIG_FILE = 'config.json'
THRESHOLDS_FILE = 'thresholds.json'

# Standardintervall in Minuten
DEFAULT_INTERVAL = 5

# Standard-Schwellenwerte
DEFAULT_THRESHOLDS = {
    "monstera": {
        "soil_moisture": {
            "optimal": [40, 70],
            "warning": [30, 80]
        },
        "temperature": {
            "optimal": [18, 25],
            "warning": [15, 30]
        },
        "humidity": {
            "optimal": [40, 60],
            "warning": [30, 70]
        }
    },
    "buschkopf": {
        "soil_moisture": {
            "optimal": [40, 70],
            "warning": [30, 80]
        },
        "temperature": {
            "optimal": [18, 25],
            "warning": [15, 30]
        },
        "humidity": {
            "optimal": [40, 60],
            "warning": [30, 70]
        }
    },
    "andere": {
        "soil_moisture": {
            "optimal": [40, 70],
            "warning": [30, 80]
        },
        "temperature": {
            "optimal": [18, 25],
            "warning": [15, 30]
        },
        "humidity": {
            "optimal": [40, 60],
            "warning": [30, 70]
        }
    }
}

# Stelle sicher, dass das Datenverzeichnis existiert
if not os.path.exists(DATA_DIR):
    os.makedirs(DATA_DIR)
    print(f"Verzeichnis '{DATA_DIR}' erstellt.")

# Lade oder initialisiere die Konfigurationsdatei
def load_config():
    if os.path.exists(CONFIG_FILE):
        with open(CONFIG_FILE, 'r') as f:
            try:
                config = json.load(f)
                return config
            except json.JSONDecodeError:
                pass
    # Standardkonfiguration
    config = {'interval': DEFAULT_INTERVAL}
    save_config(config)
    return config

def save_config(config):
    with open(CONFIG_FILE, 'w') as f:
        json.dump(config, f, indent=4)

config = load_config()

# Lade oder initialisiere die Schwellenwerte
def load_thresholds():
    if os.path.exists(THRESHOLDS_FILE):
        with open(THRESHOLDS_FILE, 'r') as f:
            try:
                thresholds = json.load(f)
                return thresholds
            except json.JSONDecodeError:
                pass
    # Standardschwellenwerte
    thresholds = DEFAULT_THRESHOLDS
    save_thresholds(thresholds)
    return thresholds

def save_thresholds(thresholds):
    with open(THRESHOLDS_FILE, 'w') as f:
        json.dump(thresholds, f, indent=4)

thresholds = load_thresholds()

@app.route('/api/data', methods=['POST'])
def receive_data():
    data = request.get_json()
    location = data.get('location', 'unknown')
    filename = f"{location}.json"

    # Temperatur und Luftfeuchtigkeit auf eine Nachkommastelle runden
    data['temperature'] = round(data.get('temperature', 0), 1)
    data['humidity'] = round(data.get('humidity', 0), 1)
    data['soil_moisture'] = round(data.get('soil_moisture', 0), 1)

    # Füge einen Zeitstempel hinzu, falls nicht vorhanden
    if 'received_at' not in data:
        data['received_at'] = datetime.utcnow().isoformat() + 'Z'

    # Speicherort der Datei
    file_path = os.path.join(DATA_DIR, filename)

    # Lade vorhandene Daten, falls die Datei existiert
    if os.path.exists(file_path):
        with open(file_path, 'r') as f:
            try:
                existing_data = json.load(f)
                if not isinstance(existing_data, list):
                    existing_data = [existing_data]
            except json.JSONDecodeError:
                existing_data = []
    else:
        existing_data = []

    # Füge den neuen Datensatz hinzu
    existing_data.append(data)

    # Speichere die aktualisierte Liste
    with open(file_path, 'w') as f:
        json.dump(existing_data, f, indent=4)

    print(f"Daten von {location} empfangen und in {filename} gespeichert.")

    # Sende Daten an verbundene WebSocket-Clients
    socketio.emit('new_data', data)
    print(f"Daten an Clients gesendet: {data}")

    return jsonify({'status': 'success'}), 200

@app.route('/')
def index():
    print("Index-Seite aufgerufen.")
    return render_template('index.html')

@app.route('/get_data/<location>')
def get_data(location):
    filename = f"{location}.json"
    file_path = os.path.join(DATA_DIR, filename)

    if os.path.exists(file_path):
        with open(file_path, 'r') as f:
            try:
                data = json.load(f)
                return jsonify(data), 200
            except json.JSONDecodeError:
                return jsonify({'status': 'error', 'message': 'Fehler beim Lesen der Daten.'}), 500
    else:
        return jsonify({'status': 'error', 'message': 'Keine Daten vorhanden.'}), 404

@app.route('/delete_data', methods=['POST'])
def delete_data():
    try:
        # Lösche den Inhalt des Datenverzeichnisses
        if os.path.exists(DATA_DIR):
            shutil.rmtree(DATA_DIR)
            os.makedirs(DATA_DIR)
        print("Alle Sensordaten wurden gelöscht.")
        return jsonify({'status': 'success'}), 200
    except Exception as e:
        print(f"Fehler beim Löschen der Daten: {e}")
        return jsonify({'status': 'error', 'message': str(e)}), 500

# Endpunkt zum Abrufen des aktuellen Intervalls
@app.route('/get_interval', methods=['GET'])
def get_interval():
    try:
        config = load_config()
        interval = config.get('interval', DEFAULT_INTERVAL)
        return jsonify({'status': 'success', 'interval': interval}), 200
    except Exception as e:
        print(f"Fehler beim Abrufen des Intervalls: {e}")
        return jsonify({'status': 'error', 'message': str(e)}), 500

# Endpunkt zum Setzen des Intervalls
@app.route('/set_interval', methods=['POST'])
def set_interval():
    try:
        data = request.get_json()
        interval = int(data.get('interval', DEFAULT_INTERVAL))
        if interval < 1:
            return jsonify({'status': 'error', 'message': 'Intervall muss mindestens 1 Minute betragen.'}), 400
        config = load_config()
        config['interval'] = interval
        save_config(config)
        print(f"Intervall auf {interval} Minuten gesetzt.")
        return jsonify({'status': 'success', 'interval': interval}), 200
    except Exception as e:
        print(f"Fehler beim Setzen des Intervalls: {e}")
        return jsonify({'status': 'error', 'message': str(e)}), 500

# Endpunkt für die ESPs, um das Intervall abzurufen
@app.route('/api/get_interval', methods=['GET'])
def api_get_interval():
    try:
        config = load_config()
        interval = config.get('interval', DEFAULT_INTERVAL)
        return jsonify({'interval': interval}), 200
    except Exception as e:
        print(f"Fehler beim Abrufen des Intervalls für ESP: {e}")
        return jsonify({'error': str(e)}), 500

# Endpunkt zum Abrufen der Schwellenwerte
@app.route('/get_thresholds', methods=['GET'])
def get_thresholds():
    try:
        thresholds = load_thresholds()
        return jsonify({'status': 'success', 'thresholds': thresholds}), 200
    except Exception as e:
        print(f"Fehler beim Abrufen der Schwellenwerte: {e}")
        return jsonify({'status': 'error', 'message': str(e)}), 500

# Endpunkt zum Setzen der Schwellenwerte für eine Pflanze
@app.route('/set_thresholds', methods=['POST'])
def set_thresholds():
    try:
        data = request.get_json()
        location = data.get('location')
        new_thresholds = data.get('thresholds')

        if not location or not new_thresholds:
            return jsonify({'status': 'error', 'message': 'Ungültige Daten.'}), 400

        thresholds = load_thresholds()
        thresholds[location] = new_thresholds
        save_thresholds(thresholds)
        print(f"Schwellenwerte für {location} aktualisiert.")
        return jsonify({'status': 'success'}), 200
    except Exception as e:
        print(f"Fehler beim Setzen der Schwellenwerte: {e}")
        return jsonify({'status': 'error', 'message': str(e)}), 500

if __name__ == '__main__':
    print("Server wird gestartet...")
    socketio.run(app, host='0.0.0.0', port=5000, debug=True)
