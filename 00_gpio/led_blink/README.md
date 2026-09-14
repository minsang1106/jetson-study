# 회로 설계

젯슨 오린 나노 디벨로퍼 킷의 expansion pin header

!image.png

GPIO 핀 중 하나를 출력 핀으로 설정하고, 저항과 led를 직렬연결한 후, gnd에 연결할 것임.

# 파이썬 코드

```python
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

```

vscode remote-ssh를 이용하여 프로그래밍했다.

여기서 하나 알아낸 것은 젯슨에 remote-ssh를 연결하면 ~/.vscode-server라는 숨겨진 파일이 설치됨.

맥에서는 창만 띄우기 때문에 remote-ssh로 원격 접속할 때는 익스텐션을 또 깔아줘야 한다. 
이것은 젯슨에 깔아주는 것임.

### 코드 설명

`GPIO.setmode(GPIO.BOARD)` : 물리적인 핀 번호로 세팅하겠다.

`GPIO.setup(LED_PIN, GPIO.OUT)` : LED_PIN(7번 핀)을 out 핀으로 설정

`try` 구문 쓴 이유 : 무한 루프를 인터럽트로 빠져나와도 GPIO 핀 설정을 정리할 수 있도록.

# 실행했더니…

문제가 발생했다. led가 깜빡이지 않고, 긴 경고 메시지가 떴다.

일반 사용자 권한으로 실행했을 때 :

pinmux check를 위해서 `/dev/mem` 에 접근해야 하는데 권한이 없다고 뜸.

sudo로 실행했을 때 :

7번 핀을 output으로 설정했는데 pinmux에는 input으로 설정되어 있다는 경고.

# 어떻게 해결했는가?

오류 메시지에서 있던 문구가 있었다.

`sudo busybox devmem 0x3038534 w 0xA`를 쳐봐라. (주소는 임의로 바꿨다)

이 명령어를 입력하고 다시 코드를 실행했더니 성공함.

# 무엇이 문제였는가?

## 배경지식

### PAD란?

Jetson SoC 안에는 I2C controller, GPIO controller, UART controller 등의 여러 회로가 있고,
이 회로들이 PAD라는 하드웨어를 거쳐서 핀과 연결된다.

PAD는 단순한 통로가 아니라 핀을 입력신호, 출력신호로 사용할지, 핀을 어느 회로와 연결할지 등을 
결정할 수 있는 기능이 있다.

### PADCTL이란?

이름 그대로 PAD를 컨트롤하는 레지스터들. 

### GPIO controller

PADCTL과 GPIO controller는 같은 것이 아니다. 

```
GPIO Controller
      │
      │ HIGH / LOW 신호 생성
      ▼
PAD / Pinmux
      │
      │ 이 신호를 밖으로 내보냄
      ▼
Physical Pin
      │
      ▼
LED
```

## 문제를 알아보자

`GPIO.setup(7, GPIO.OUT)` : GPIO controller에게 회로 방향을 바깥으로 열어두라고 명령한다.

 `GPIO.output(7, GPIO.HIGH)` : GPIO controller는 high 신호를 생성한다.

하지만 PADCTL은 부팅될 때부터 설정되어 있었다.

```
GPIO Controller 쪽
┌─────────────────┐
│ OUTPUT으로 사용   │
└────────┬────────┘
         │
         │ HIGH/LOW 내보내고 싶음
         ▼
PADCTL
┌─────────────────┐
│ INPUT으로 설정    │
└────────┬────────┘
         │
         ▼
Physical Pin 7
```

# 그래서 어떻게 해결이 된건데?

`sudo busybox devmem 0x3038534 w 0xA` 

이 명령은 마법의 주문이었을까?

문제의 원인은 7번 핀을 관장하는 PADCTL에 “7번 핀은 input용이다”라고 설정되어 있었던 것이다.

`busybox` : 리눅스에서 자주 쓰는 여러 기본 명령어를 하나의 실행 파일에 묶어놓은 프로그램.
각각의 명령어를 전부 따로 설치하는 것보다 작고 간단하게 많은 기본 유틸리티를 제공할 수 있기 때문에 사용

`devmem` : busybox의 기능 중에 devmem을 사용해라. 
사용자 공간에서 특정 물리 주소를 직접 읽거나 쓰기 위한 디버깅 도구

`0x3038534` : 7번핀의 PADCTL MMIO 레지스터의 물리주소

`w 0xA` : 해당 레지스터에 `0xA`를 write. 32비트 레지스터라 실제로는 `0x0000000A`

### 왜 굳이 0xA를 write했을까?

PAD 설정은 다음과 같다.

GPIO로 사용할 것 → bit10 = 0

output → bit6 = 0, bit4 = 0

input  → bit6 = 1, bit4 = 1

0xA = 1010

bit6 = 0, bit4 = 0으로 만듦.

0x0이 아니라 0xA인 이유는 기존의 값에서 6번 비트, 4번 비트만 0으로 바꾼 값이기 때문.
계산해서 준 듯.

Jetson Orin Series SoC Technical Reference Manual 에 있다.
