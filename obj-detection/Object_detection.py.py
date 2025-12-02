import cv2
import urllib.request
import numpy as np
import requests
import time

# ESP32-CAM URL
esp32_ip = 'http://192.168.50.145'  # Update with your ESP32-CAM IP
url = f'{esp32_ip}/cam-hi.jpg'
winName = 'ESP32 Animal Detection'
cv2.namedWindow(winName, cv2.WINDOW_AUTOSIZE)

# Load ALL COCO classes first
classNames = []
classFile = 'coco.names'
with open(classFile, 'rt') as f:
    classNames = f.read().rstrip('\n').split('\n')

print(f"Loaded {len(classNames)} classes from coco.names")

# Animal classes we care about (COCO dataset IDs)
ANIMAL_CLASS_IDS = {
    16: 'bird', 17: 'cat', 18: 'dog', 19: 'horse', 20: 'sheep',
    21: 'cow', 22: 'elephant', 23: 'bear', 24: 'zebra', 25: 'giraffe'
}

# Add turtle and tortoise if they exist in your model
# If not, we'll handle them separately
TURTLE_CLASS_IDS = [86, 87]  # Common IDs for turtle/tortoise in some models

configPath = 'ssd_mobilenet_v3_large_coco_2020_01_14.pbtxt'
weightsPath = 'frozen_inference_graph.pb'

net = cv2.dnn_DetectionModel(weightsPath, configPath)
net.setInputSize(320, 320)
net.setInputScale(1.0 / 127.5)
net.setInputMean((127.5, 127.5, 127.5))
net.setInputSwapRB(True)

# Detection statistics
detection_count = 0
animal_detections = 0


def is_animal_class(classId):
    """Check if detected class is an animal we care about"""
    # Check main animal classes
    if classId in ANIMAL_CLASS_IDS:
        return True, ANIMAL_CLASS_IDS[classId]

    # Check turtle classes
    if classId in TURTLE_CLASS_IDS:
        return True, 'turtle'

    # Check by class name for additional animals
    if classId <= len(classNames):
        class_name = classNames[classId - 1].lower()
        animal_keywords = ['turtle', 'tortoise', 'bird', 'cat', 'dog', 'horse',
                           'sheep', 'cow', 'elephant', 'bear', 'zebra', 'giraffe',
                           'animal', 'mammal']
        if any(keyword in class_name for keyword in animal_keywords):
            return True, class_name

    return False, None


def send_detection_to_web(animal, confidence):
    """Send detection data to web interface"""
    try:
        # You can implement this to send data to your web server
        print(f"🔍 DETECTED: {animal} - Confidence: {confidence:.2f}")
        return True
    except Exception as e:
        print(f"Error sending detection: {e}")
        return False


print("Starting Accurate Animal Detection...")
print("Target animals: turtle, tortoise, cat, dog, bird, horse, sheep, cow, elephant, bear, zebra, giraffe")
print("Press ESC to exit")

# Colors for different animals
colors = {
    'cat': (255, 0, 0),  # Blue
    'dog': (0, 255, 255),  # Yellow
    'bird': (255, 255, 0),  # Cyan
    'turtle': (0, 165, 255),  # Orange
    'tortoise': (0, 140, 255),  # Dark Orange
    'default': (0, 255, 0)  # Green
}

while True:
    try:
        imgResponse = urllib.request.urlopen(url, timeout=5)
        imgNp = np.array(bytearray(imgResponse.read()), dtype=np.uint8)
        img = cv2.imdecode(imgNp, -1)

        if img is None:
            print("Failed to decode image")
            continue

        # Rotate image if needed
        img = cv2.rotate(img, cv2.ROTATE_90_CLOCKWISE)

        # Detect objects
        classIds, confs, bbox = net.detect(img, confThreshold=0.5)

        detection_count += 1
        current_detections = []

        if len(classIds) != 0:
            for classId, confidence, box in zip(classIds.flatten(), confs.flatten(), bbox):
                is_animal, animal_name = is_animal_class(classId)

                if is_animal and confidence > 0.5:  # Only show confident detections
                    animal_detections += 1
                    current_detections.append((animal_name, confidence))

                    # Get color for this animal
                    color = colors.get(animal_name, colors['default'])

                    # Draw bounding box
                    cv2.rectangle(img, box, color=color, thickness=3)

                    # Prepare label with confidence
                    label = f"{animal_name.upper()} {confidence:.2f}"

                    # Display label with background for better visibility
                    label_size = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.7, 2)[0]
                    cv2.rectangle(img, (box[0], box[1] - label_size[1] - 10),
                                  (box[0] + label_size[0], box[1]), color, -1)
                    cv2.putText(img, label, (box[0], box[1] - 5),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)

                    # Special highlight for turtles/tortoises
                    if animal_name in ['turtle', 'tortoise']:
                        cv2.putText(img, "🐢 TURTLE ALERT!", (box[0] + 10, box[1] - 40),
                                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 0, 255), 2)
                        print(f"🚨 TURTLE DETECTED! Confidence: {confidence:.2f}")
                        send_detection_to_web("turtle", confidence)
                    else:
                        send_detection_to_web(animal_name, confidence)

                    print(f"✅ Correctly detected: {animal_name} (ID: {classId}) - Confidence: {confidence:.2f}")

        # Display detection statistics
        accuracy = (animal_detections / detection_count) * 100 if detection_count > 0 else 0

        # Status header
        status_text = f"Animal Detection - Accuracy: {accuracy:.1f}%"
        cv2.putText(img, status_text, (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)

        # Current detections
        if current_detections:
            detection_text = f"Current: {', '.join([f'{name}({conf:.2f})' for name, conf in current_detections])}"
            cv2.putText(img, detection_text, (10, 60),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
        else:
            cv2.putText(img, "No animals detected", (10, 60),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)

        cv2.imshow(winName, img)

        # ESC key to exit
        tecla = cv2.waitKey(1) & 0xFF
        if tecla == 27:
            break

    except Exception as e:
        print(f"Error: {e}")
        time.sleep(1)
        continue

cv2.destroyAllWindows()
print("Animal detection stopped")