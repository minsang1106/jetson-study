import Jetson.GPIO as GPIO
import time
button_pin = 15
GPIO.setmode(GPIO.BOARD)
GPIO.setup(button_pin, GPIO.IN, GPIO.PUD_UP)

try:
    while True:
        print("1" if GPIO.input(button_pin)==GPIO.HIGH else "0")
        time.sleep(0.5)
finally:
    GPIO.cleanup()