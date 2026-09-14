# 회로 설계

이번에는 GPIO 출력뿐만 아니라 **GPIO 입력**도 사용해본다.

구성은 다음과 같다.

```
LED 출력 : Physical Pin 7
버튼 입력 : Physical Pin 15
```

LED 회로:

```
Pin 7
  │
[330Ω]
  │
 LED
  │
 GND
```

버튼 회로는 **pull-up 방식**으로 구성했다.

```
3.3V
 │
[2kΩ pull-up]
 │
 ├──────── Pin 15
 │
[BUTTON]
 │
GND
```

버튼을 누르지 않았을 때는 pull-up 저항에 의해 Pin 15가 HIGH가 된다.

```
3.3V
 │
[2kΩ]
 │
Pin 15

→ HIGH
```

버튼을 누르면 Pin 15가 GND와 연결된다.

```
Pin 15
 │
[BUTTON]
 │
GND

→ LOW
```

따라서 동작은:

```
버튼 OFF → Pin 15 = HIGH
버튼 ON  → Pin 15 = LOW
```

이다.

---

# 파이썬 코드

우선 가장 단순한 polling 방식으로 구현했다.

```python
import Jetson.GPIO as GPIO
import time

LED_PIN = 7
BUTTON_PIN = 15

GPIO.setmode(GPIO.BOARD)

GPIO.setup(LED_PIN, GPIO.OUT, initial=GPIO.LOW)
GPIO.setup(BUTTON_PIN, GPIO.IN)

try:
    while True:
        button_state = GPIO.input(BUTTON_PIN)

        if button_state == GPIO.LOW:
            GPIO.output(LED_PIN, GPIO.HIGH)
        else:
            GPIO.output(LED_PIN, GPIO.LOW)

        time.sleep(0.01)

finally:
    GPIO.cleanup()
```

### 코드 설명

`GPIO.setup(BUTTON_PIN, GPIO.IN)`

: Physical Pin 15에 대응하는 GPIO channel을 입력으로 사용한다.

`GPIO.input(BUTTON_PIN)`

: 해당 GPIO의 현재 논리값을 읽는다.

반환값은:

```
GPIO.HIGH → 1
GPIO.LOW  → 0
```

이다.

버튼 회로가 pull-up 방식이므로:

```python
if button_state == GPIO.LOW:
```

는 곧

> 버튼이 눌렸다면
> 

이라는 의미가 된다.

버튼이 눌렸을 때 LED 출력 Pin 7을 HIGH로 설정한다.

```python
GPIO.output(LED_PIN, GPIO.HIGH)
```

버튼이 눌리지 않았을 때는 LOW로 설정한다.

---

# 왜 pull-up 저항이 필요한가?

GPIO input은 외부에서 들어오는 전압을 읽는다.

그런데 버튼을 다음처럼 단순히 연결했다고 생각해보자.

```
Pin 15
 │
[BUTTON]
 │
GND
```

버튼을 누르면 Pin 15가 GND와 연결되므로 확실하게 LOW가 된다.

하지만 버튼을 놓으면:

```
Pin 15 ───── 아무것도 연결되지 않음
```

이 되어 HIGH도 LOW도 아닌 **floating 상태**가 될 수 있다.

따라서 버튼이 열려 있을 때 기본 상태를 확실하게 정해줄 필요가 있다.

이번에는 3.3V에 pull-up했다.

```
3.3V
 │
[Pull-up resistor]
 │
 ├── Pin 15
 │
Button
 │
GND
```

따라서 기본 상태는 HIGH이고 버튼을 눌렀을 때만 LOW가 된다.

---

# 실행했더니…

처음에는 버튼 입력이 제대로 동작하지 않았다.

버튼 상태만 확인하기 위해 다음과 같이 테스트했다.

```python
import Jetson.GPIO as GPIO
import time

BUTTON_PIN = 15

GPIO.setmode(GPIO.BOARD)
GPIO.setup(BUTTON_PIN, GPIO.IN)

try:
    while True:
        print(GPIO.input(BUTTON_PIN))
        time.sleep(0.5)
finally:
    GPIO.cleanup()
```

정상이라면:

```
버튼 안 누름 → 1
버튼 누름    → 0
버튼 놓음    → 1
```

이 나와야 한다.

하지만 실제로는 버튼을 한 번 누른 뒤 계속 `0`이 읽히는 문제가 있었다.

---

# Pin 15의 PAD 설정을 확인해보자

Pin 15의 PADCTL register를 확인했다.

```bash
sudo busybox devmem 0x2440020
```

결과:

```
0x00000055
```

`0x55`를 이진수로 나타내면:

```
0x55 = 0101 0101

bit:   7 6 5 4 3 2 1 0
       0 1 0 1 0 1 0 1
         ↑   ↑ └─┘
        b6  b4 b3:2
```

NVIDIA Orin PADCTL 설정에서:

```
bit 6 = 1
bit 4 = 1
```

이므로 Pin 15는 **input에 맞는 PAD 설정**이었다.

또한 `PUPD`는 bit `[3:2]`에 저장된다.

```
00 → no pull
01 → pull-down
10 → pull-up
```

`0x55`에서는:

```
bit[3:2] = 01
```

이므로 처음에는 내부 pull-down 상태였다.

PADCTL의 pull 설정을 pull-up으로 변경해보기도 했다.

하지만 **pull-up으로 변경한 것만으로는 문제를 해결하지 못했다.**

즉 이번 문제를 단순히

> 내부 pull-down 때문에 입력이 LOW였다.
> 

라고만 설명할 수는 없었다.

---

# 외부 pull-up 저항을 바꿔보았다

처음에는 일반적으로 많이 사용하는 `10kΩ` pull-up 저항을 사용했다.

```
3.3V
 │
[10kΩ]
 │
 ├── Pin 15
```

하지만 실제 환경에서는 버튼을 한 번 누른 뒤 Pin 15가 정상적으로 HIGH로 돌아오지 않는 문제가 있었다.

그래서 외부 pull-up 저항을 더 낮은 값으로 변경했다.

```
10kΩ → 2kΩ
```

결과:

```
버튼 OFF → 1
버튼 ON  → 0
버튼 OFF → 1
```

로 정상 동작하였다.

따라서 최종 회로에서는 `2kΩ` 외부 pull-up 저항을 사용했다.

버튼을 누르면 저항을 통해 GND로 흐르는 전류는:

$$
I=\frac{3.3V}{2k\Omega}\approx1.65mA
$$

이다.

### 중요한 점

이번 실험에서는 **2kΩ에서 정상 동작했다는 것은 직접 확인했지만, 10kΩ에서 왜 HIGH가 안정적으로 형성되지 않았는지는 아직 정확한 원인을 확인하지 않았다.**

PADCTL의 내부 pull 설정을 pull-up으로 바꾼 뒤에도 동일한 현상이 있었기 때문에 단순히 내부 pull-down과 외부 저항의 분압 문제라고 결론 내릴 수는 없다.

이 부분은 추후 실제 Pin 15 전압, 입력 임계전압, PAD 설정 등을 추가로 확인해볼 필요가 있다.

---

# GPIO input에서 알게 된 것

GPIO input에서 `input`이라는 것은 핀을 LOW로 만든다는 의미가 아니다.

```
GPIO OUTPUT
→ SoC가 핀의 전압을 적극적으로 HIGH/LOW로 구동

GPIO INPUT
→ 외부에서 핀에 들어오는 전압을 읽음
```

따라서 input 핀은 기본 전압을 결정하기 위해 pull-up 또는 pull-down이 필요할 수 있다.

```
pull-up

3.3V
 │
[R]
 │
GPIO INPUT

→ 기본 HIGH
```

```
pull-down

GPIO INPUT
 │
[R]
 │
GND

→ 기본 LOW
```

---

# Polling 방식의 문제점

현재 코드는:

```python
while True:
    button_state = GPIO.input(BUTTON_PIN)
```

처럼 버튼 상태를 계속 읽는다.

즉 버튼이 아무 일도 하지 않아도 CPU는 반복적으로 GPIO 상태를 확인한다.

```
read
read
read
read
read
...
```

이런 방식을 **polling**이라고 한다.

버튼처럼 상태 변화가 드문 입력을 polling으로 계속 검사하는 것은 불필요한 작업이 발생할 수 있다.

Jetson.GPIO에서는 이를 위해 edge/event 기반 API도 제공한다.

```
GPIO.RISING
GPIO.FALLING
GPIO.BOTH
```

pull-up 방식의 버튼에서는:

```
버튼 안 누름 : HIGH
        ↓ 버튼 누름
버튼 누름    : LOW

HIGH → LOW
```

이므로 **FALLING edge**를 감지하면 버튼이 눌린 순간을 알 수 있다.

그래서 다음 단계에서는:

```python
GPIO.add_event_detect(
    BUTTON_PIN,
    GPIO.FALLING,
    callback=...
)
```

방식으로 polling 없이 버튼 입력을 처리할 수 있다.

---

# 오늘 실습에서 정리한 전체 구조

```
                     Jetson Orin Nano

3.3V
 │
[2kΩ pull-up]
 │
 ├──── Pin 15
 │        │
Button    │
 │        │
GND       │
          │
          ▼
      GPIO Input
          │
          │ GPIO.input()
          ▼
     Python Program
          │
          │ 버튼이 LOW라면
          ▼
      GPIO Output
          │
          ▼
        Pin 7
          │
        330Ω
          │
         LED
          │
         GND
```

그리고 어제 공부한 내용과 연결하면:

```
버튼
 ↓
Physical Pin 15
 ↓
PAD / PADCTL
 ↓
GPIO Controller
 ↓
Linux GPIO subsystem
 ↓
Jetson.GPIO
 ↓
Python

Python
 ↓
Jetson.GPIO
 ↓
GPIO Controller
 ↓
PAD / PADCTL
 ↓
Physical Pin 7
 ↓
LED
```
