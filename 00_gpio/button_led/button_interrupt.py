import Jetson.GPIO as GPIO
import time
main_led_pin = 7
button_led_pin = 33
button_pin = 15
button_led_state = GPIO.LOW

GPIO.setmode(GPIO.BOARD)
GPIO.setup(main_led_pin, GPIO.OUT, initial=GPIO.LOW)
GPIO.setup(button_led_pin, GPIO.OUT, initial=button_led_state)
GPIO.setup(button_pin, GPIO.IN)

def button_pushed(channel):
    global button_led_state
    button_led_state = GPIO.LOW if button_led_state else GPIO.HIGH
    GPIO.output(button_led_pin, button_led_state)

GPIO.add_event_detect(button_pin, GPIO.BOTH, button_pushed, bouncetime=100)

try:
    while True:
        GPIO.output(main_led_pin, GPIO.HIGH)
        time.sleep(0.5)
        GPIO.output(main_led_pin, GPIO.LOW)
        time.sleep(0.5)
finally:
    GPIO.cleanup()