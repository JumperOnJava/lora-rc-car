from flask import Flask, Response
import cv2
import threading

app = Flask(__name__)

# Open the USB camera
camera = cv2.VideoCapture(0)
camera.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
camera.set(cv2.CAP_PROP_FPS, 30)  # Set higher FPS if possible

frame = None
lock = threading.Lock()

def capture_frames():
    global frame
    while True:
        success, img = camera.read()
        if success:
            with lock:
                frame = img

# Start frame capture thread
threading.Thread(target=capture_frames, daemon=True).start()

def generate_frames():
    global frame
    while True:
        with lock:
            if frame is None:
                continue
            ret, buffer = cv2.imencode('.jpg', frame)
            if not ret:
                continue
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + buffer.tobytes() + b'\r\n')

@app.route('/video_feed')
def video_feed():
    return Response(generate_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/')
def index():
    return "<html><body><h1>Raspberry Pi Video Stream</h1><img src='/python/video_feed' width='640'></body></html>"


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8009, debug=False)
    
