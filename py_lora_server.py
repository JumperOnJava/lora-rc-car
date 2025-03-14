from flask import Flask, request, jsonify
import json
import threading
import time
from lora import LoRa

app = Flask(__name__)

# Initialize LoRa module
lora = LoRa()

def lora_module_connected():
    return lora.is_open()

def send_lora_packet(packet):
    lora.write(packet)

# API Endpoints
@app.route("/api/carAvailable", methods=["GET"])
def car_available():
    return "Ok", 200, {"Content-Type": "text/plain"}

@app.route("/api/loraConnected", methods=["GET"])
def lora_connected():
    status = "true" if lora_module_connected() else "false"
    print(f"lora {status}\n")
    return status, 200, {"Content-Type": "text/plain"}

@app.route("/api/ping", methods=["GET"])
def ping():
    packet = {"type": "PING_TO_CAR", "size": 0}
    send_lora_packet(json.dumps(packet))
    return "pinged, check result in console", 200, {"Content-Type": "text/plain"}

@app.route("/api/queue", methods=["GET"])
def queue():
    queue_data_json = {"length": len(packet_queue), "data": []}  # No actual queue implementation
    return jsonify(queue_data_json), 200

@app.route("/api/sendControls", methods=["POST"])
def send_controls():
    data = request.get_json()
    if not data:
        return "json body empty", 400, {"Content-Type": "text/html"}
    
    forward = max(-100.0, min(100.0, float(data.get("forward", 0))))
    left_right = max(0.0, min(90.0, float(data.get("leftRight", 0))))
    
    print(f"f: {forward:.2f}; lr: {left_right:.2f};")
    
    packet = {
        "type": "SEND_CONTROLS",
        "size": 8,
        "data": {"forward": forward, "leftRight": left_right}
    }
    send_lora_packet(json.dumps(packet))
    return jsonify(packet), 200

@app.route("/api/latestData", methods=["GET"])
def latest_data():
    try:
        return "Ok", 200, {"Content-Type": "text/plain"}
    except Exception as e:
        return f"Error while getting data: {str(e)}", 400, {"Content-Type": "text/plain"}

@app.route("/")
def home():
    return "Wrong endpoint", 404, {"Content-Type": "text/plain"}

# Start Flask server in a separate thread
def start_server():
    app.run(host="0.0.0.0", port=8008, threaded=True)

if __name__ == "__main__":
    # Start LoRa module
    lora.open()
    print("LoraSubServer started")
    server_thread = threading.Thread(target=start_server)
    server_thread.daemon = True
    server_thread.start()
    
    while True:
        time.sleep(1000000000)
