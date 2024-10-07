# server.py

from flask import Flask, request, jsonify, render_template
from flask_cors import CORS
from flask_socketio import SocketIO, emit
import os
import json
from datetime import datetime
import logging

# Logging konfigurieren
logging.basicConfig(level=logging.DEBUG)

app = Flask(__name__)
CORS(app)
socketio = SocketIO(app, cors_allowed_origins='*')

DATA_DIR = 'data'
CONFIG_FILE = 'config.json'
LOGS_DIR = 'logs'  # Verzeichnis für Logs
THRESHOLDS_FILE = 'thresholds.json'

# Standardintervall
DEFAULT_INTERVAL = 5

# Stelle sicher, dass die Verzeichnisse existieren
for directory in [DATA_DIR, LOGS_DIR]:
    if not os.path.exists(directory):
        os.makedirs(directory)
        print(f"Verzeichnis '{directory}' erstellt.")

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
    default_thresholds = {
        'monstera': {
            'soil_moisture': {'optimal': [30, 60]},
            'temperature': {'optimal': [18, 25]},
            'humidity': {'optimal': [40, 60]},
        },
        'buschkopf': {
            'soil_moisture': {'optimal': [40, 70]},
            'temperature': {'optimal': [20, 28]},
            'humidity': {'optimal': [50, 70]},
        },
        'andere': {
            'soil_moisture': {'optimal': [25, 50]},
            'temperature': {'optimal': [15, 22]},
            'humidity': {'optimal': [30, 50]},
        },
    }
    if os.path.exists(THRESHOLDS_FILE):
        with open(THRESHOLDS_FILE, 'r') as f:
            try:
                thresholds = json.load(f)
                # Stelle sicher, dass alle Pflanzen Schwellenwerte haben
                for plant in default_thresholds:
                    if plant not in thresholds:
                        thresholds[plant] = default_thresholds[plant]
                return thresholds
            except json.JSONDecodeError:
                pass
    # Speichere Standard-Schwellenwerte
    save_thresholds(default_thresholds)
    return default_thresholds

def save_thresholds(thresholds):
    with open(THRESHOLDS_FILE, 'w') as f:
        json.dump(thresholds, f, indent=4)

thresholds = load_thresholds()

# Endpunkt zum Empfangen der Logs
@app.route('/api/logs', methods=['POST'])
def receive_logs():
    data = request.get_json()
    if not data:
        return jsonify({'status': 'error', 'message': 'Keine Daten erhalten'}), 400

    location = data.get('location', 'unknown')
    logs = data.get('logs', '')

    if not logs:
        return jsonify({'status': 'error', 'message': 'Keine Logs bereitgestellt'}), 400

    filename = f"{location}.log"
    file_path = os.path.join(LOGS_DIR, filename)

    # Speichere die Logs in der Datei, überschreibe falls vorhanden
    try:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(logs)
        print(f"Logs von {location} empfangen und in {filename} gespeichert.")
        return jsonify({'status': 'success', 'message': f'Logs gespeichert als {filename}'}), 200
    except Exception as e:
        print(f"Fehler beim Speichern der Logs: {e}")
        return jsonify({'status': 'error', 'message': 'Fehler beim Speichern der Logs'}), 500

# Route für die Hauptseite
@app.route('/')
def index():
    print("Index-Seite aufgerufen.")
    # Lade die verfügbaren Pflanzendaten
    plants = []
    for filename in os.listdir(DATA_DIR):
        if filename.endswith('.json'):
            plants.append(filename[:-5])  # Entfernt '.json' vom Dateinamen
    # Füge die Pflanzen hinzu, die im HTML verwendet werden
    for plant in ['monstera', 'buschkopf', 'andere']:
        if plant not in plants:
            plants.append(plant)
    return render_template('index.html', plants=plants)

# Endpunkt für das Abrufen des Intervalls
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

# Endpunkt für das Abrufen der Schwellenwerte
@app.route('/get_thresholds', methods=['GET'])
def get_thresholds_endpoint():
    try:
        thresholds = load_thresholds()
        return jsonify({'status': 'success', 'thresholds': thresholds}), 200
    except Exception as e:
        print(f"Fehler beim Abrufen der Schwellenwerte: {e}")
        return jsonify({'status': 'error', 'message': str(e)}), 500

# Endpunkt zum Setzen der Schwellenwerte
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
        return jsonify({'status': 'success', 'thresholds': {location: new_thresholds}}), 200
    except Exception as e:
        print(f"Fehler beim Setzen der Schwellenwerte: {e}")
        return jsonify({'status': 'error', 'message': str(e)}), 500

# Endpunkt zum Abrufen der Daten für eine Pflanze
@app.route('/get_data/<location>', methods=['GET'])
def get_data(location):
    file_path = os.path.join(DATA_DIR, f"{location}.json")
    if os.path.exists(file_path):
        with open(file_path, 'r') as f:
            try:
                data = json.load(f)
                return jsonify(data), 200
            except json.JSONDecodeError:
                return jsonify([]), 200  # Leeres Array zurückgeben, wenn die Daten nicht geladen werden können
    else:
        return jsonify([]), 200  # Leeres Array zurückgeben, wenn die Datei nicht existiert

# Endpunkt zum Empfangen von Sensordaten
@app.route('/api/data', methods=['POST'])
def receive_data():
    data = request.get_json()
    location = data.get('location', 'unknown')
    filename = f"{location}.json"

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

    # Sende Daten an verbundene Socket.IO-Clients
    socketio.emit('new_data', {'location': location, **data})

    return jsonify({'status': 'success'}), 200

# Socket.IO-Event-Handler
@socketio.on('connect')
def handle_connect():
    print('Client verbunden:', request.sid)

@socketio.on('disconnect')
def handle_disconnect():
    print('Client getrennt:', request.sid)

# Endpunkt zum Löschen aller Sensordaten
@app.route('/delete_data', methods=['POST'])
def delete_all_data():
    try:
        # Lösche alle Dateien im Datenverzeichnis
        for filename in os.listdir(DATA_DIR):
            file_path = os.path.join(DATA_DIR, filename)
            if os.path.isfile(file_path):
                os.remove(file_path)
        print("Alle Sensordaten wurden gelöscht.")
        return jsonify({'status': 'success'}), 200
    except Exception as e:
        print(f"Fehler beim Löschen der Sensordaten: {e}")
        return jsonify({'status': 'error', 'message': str(e)}), 500

if __name__ == '__main__':
    print("Server wird gestartet...")
    socketio.run(app, host='0.0.0.0', port=5000, debug=True)
