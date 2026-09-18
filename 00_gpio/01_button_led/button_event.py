import Jetson.GPIO as GPIO

LED_PIN = 7
BUTTON_PIN = 15
GPIO.setmode(GPIO.BOARD)
GPIO.setup(LED_PIN, GPIO.OUT, initial=GPIO.LOW)
GPIO.setup(BUTTON_PIN, GPIO.IN)

try:
    while True:
        GPIO.wait_for_edge(BUTTON_PIN, GPIO.FALLING)
        GPIO.output(LED_PIN, GPIO.HIGH)

        GPIO.wait_for_edge(BUTTON_PIN, GPIO.RISING)
        GPIO.output(LED_PIN, GPIO.LOW)
finally:
    GPIO.cleanup()