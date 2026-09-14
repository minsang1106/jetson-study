import Jetson.GPIO as GPIO
import time

BUTTON_PIN = 15
LED_PIN = 7
GPIO.setmode(GPIO.BOARD)
GPIO.setup(BUTTON_PIN, GPIO.IN)
GPIO.setup(LED_PIN, GPIO.OUT, initial= GPIO.LOW)

try:
    while True:
        button_state = GPIO.input(BUTTON_PIN)
        if  button_state == GPIO.LOW:
            GPIO.output(LED_PIN, GPIO.HIGH)
        else:
            GPIO.output(LED_PIN, GPIO.LOW)
        time.sleep(0.01)
finally:
    GPIO.cleanup()