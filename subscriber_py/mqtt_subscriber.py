import sqlite3
from flask import Flask, render_template, jsonify, request
from flask_sqlalchemy import SQLAlchemy
import paho.mqtt.client as mqtt
import json

# Flask setup
app = Flask(__name__)
app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///plates.db'
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
db = SQLAlchemy(app)

# Database model
class Plate(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    plate = db.Column(db.String(20), nullable=False)
    timestamp = db.Column(db.String(50), nullable=False)

# MQTT Configuration
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "plates/detected"

# MQTT Callbacks
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT broker!")
        client.subscribe(MQTT_TOPIC)
    else:
        print(f"Failed to connect, return code {rc}")

def on_message(client, userdata, msg):
    try:
        data = msg.payload.decode('utf-8')
        print(f"Raw MQTT message: {data}")  # Log beskeden
        plate, timestamp = parse_message(data)
        save_to_database(plate, timestamp)
        print(f"Data saved: Plate={plate}, Timestamp={timestamp}")
    except Exception as e:
        print(f"Error processing message: {e}")


# Helper Functions
def parse_message(message):
    try:
        data = json.loads(message)  # Parse JSON
        plate = data['plate']       # Hent nummerpladen
        timestamp = data['timestamp']  # Hent tidsstempel
        return plate, timestamp
    except (json.JSONDecodeError, KeyError) as e:
        raise ValueError(f"Invalid message format: {message}") from e

def save_to_database(plate, timestamp):
    with app.app_context():
        new_entry = Plate(plate=plate, timestamp=timestamp)
        db.session.add(new_entry)
        db.session.commit()

# Flask Routes
@app.route('/')
def index():
    plates = Plate.query.order_by(Plate.timestamp.desc()).all()
    return render_template('index.html', plates=plates)

@app.route('/api/plates', methods=['GET'])
def api_get_plates():
    limit = request.args.get('limit', default=10, type=int)
    plates = Plate.query.order_by(Plate.timestamp.desc()).limit(limit).all()
    return jsonify([{"id": p.id, "plate": p.plate, "timestamp": p.timestamp} for p in plates])

# MQTT Setup
mqtt_client = mqtt.Client()
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

# Run MQTT Client and Flask App
if __name__ == '__main__':
    # Initialize Database
    with app.app_context():
        db.create_all()

    # Start MQTT Client
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    mqtt_client.loop_start()

    # Start Flask Web Server
    app.run(host='0.0.0.0', port=1337, debug=False)
