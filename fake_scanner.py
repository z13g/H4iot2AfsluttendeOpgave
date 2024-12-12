from flask import Flask, jsonify, request
import random
import datetime

app = Flask(__name__)

# Eksempelnummerplader
plates = [
    "A000AA78", "B123BB99", "C456CC12", "D789DD34", "E101EE56"
]

@app.route('/get_plate', methods=['GET'])
def get_plate():
    # Simulerer et scenario, hvor en nummerplade er fundet
    plate = random.choice(plates)
    timestamp = datetime.datetime.now().isoformat()
    return f"{plate},{timestamp}", 200

@app.route('/')
def index():
    return "Se /get_plate"

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
