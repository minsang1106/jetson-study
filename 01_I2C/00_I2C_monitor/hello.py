import board
import busio
import adafruit_ssd1306

from PIL import Image, ImageDraw, ImageFont

i2c = busio.I2C(board.SCL, board.SDA)

oled = adafruit_ssd1306.SSD1306_I2C(
    128,
    64,
    i2c,
    addr=0x3C
)

try:
    oled.fill(0)
    oled.show()

    image = Image.new("1", (oled.width, oled.height))
    draw = ImageDraw.Draw(image)
    font = ImageFont.load_default()

    draw.text(
        (10, 25),
        "Hello World",
        font=font,
        fill=255
    )

    oled.image(image)
    oled.show()

    # 프로그램이 바로 끝나지 않게 유지
    while True:
        pass

finally:
    oled.fill(0)
    oled.show()
    oled.poweroff()
    print("OLED off")