import Jetson.GPIO as GPIO
import time

LED_PIN = 7
GPIO.setmode(GPIO.BOARD)
GPIO.setup(LED_PIN, GPIO.OUT)

try:
    while True:
        GPIO.output(LED_PIN, GPIO.HIGH)
        time.sleep(0.5)

        GPIO.output(LED_PIN, GPIO.LOW)
        time.sleep(0.5)
finally:
    GPIO.cleanup()
