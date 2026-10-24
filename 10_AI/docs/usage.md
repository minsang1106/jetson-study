# IMX219 → Jetson → Mac

Jetson에는 모니터가 필요 없습니다. OpenCV는 카메라 캡처와 영상 처리에 사용하고, 추론은 PyTorch로 수행합니다.
 GStreamer로 H.264/RTP 영상을 UDP 전송합니다. 화면은 맥에서 엽니다.

- `camera_test.py`: IMX219 원본 영상 전송
- `detect.py`: Ultralytics YOLO11n 객체탐지 후 박스·분류명·신뢰도를 그려 전송
- 기본값: 640×480, 카메라 30 FPS, 2 Mbps, UDP 5000번 포트
- 탐지는 PyTorch CUDA(GPU), H.264 인코딩은 CPU에서 실행합니다. 실제 탐지
  영상 FPS는 처리 속도에 따라 낮아집니다. TensorRT는 사용하지 않습니다.
- COCO 80종(사람, 자동차, 자전거, 개, 고양이, 의자 등)을 탐지합니다.
  모든 종류의 물건을 탐지하는 모델은 아닙니다.

## 1. 맥에서 수신 준비

Jetson과 맥을 서로 통신 가능한 같은 LAN/Wi-Fi에 연결합니다.
맥의 시스템 설정 → 네트워크 → 연결된 인터페이스 → 세부사항에서 IPv4 주소를
확인하세요. 아래 `192.168.0.10`은 예시이므로 실제 맥 IP로 바꿔야 합니다.

맥 터미널에서 Homebrew로 설치합니다.

```bash
brew install gstreamer
```

먼저 다음 수신 명령을 실행한 채로 둡니다. 두 파이썬 프로그램에 공통입니다.

```bash
gst-launch-1.0 -v \
  udpsrc port=5000 caps="application/x-rtp,media=video,encoding-name=H264,payload=96,clock-rate=90000" \
  ! rtpjitterbuffer latency=100 drop-on-latency=true \
  ! rtph264depay ! h264parse ! avdec_h264 \
  ! videoconvert ! autovideosink sync=false
```

macOS에서 네트워크/방화벽 접근을 물으면 허용하세요. Jetson 측에는 `DISPLAY`,
`imshow`, SSH X11 포워딩이 필요하지 않습니다. UDP는 수신 확인을 하지 않으므로
Jetson에 송신 로그가 보여도 맥 수신 성공을 의미하지는 않습니다.

## 2. Jetson에서 카메라 확인

저장소 루트(`/home/minsang/jetson-study`)에서 실행합니다.
**`10_AI/.venv/bin/python`을 사용하세요.** 이 venv에 PyTorch와 NumPy를
설치하고, GStreamer 지원 OpenCV와 `gi`는 Jetson에 기존 설치된 모듈을
venv 안의 심볼릭 링크로 재사용합니다. 시스템 Python 패키지는 변경하지 않습니다.

```bash
10_AI/.venv/bin/python 10_AI/00_yolo/camera_test.py --host 192.168.0.10
```

맥에 영상이 나오면 Jetson에서 Ctrl+C로 종료하고 다음 탐지를 실행합니다.
카메라를 두 프로그램에서 동시에 열지 마세요.

## 3. Jetson에서 객체탐지

최초 한 번 Ultralytics 공식 YOLO11n 모델(약 5.4 MB)을 다운로드합니다. 파일은 실행 위치와
무관하게 `10_AI/models/`에 저장되고, 기존 파일은 재사용합니다.

```bash
10_AI/.venv/bin/python 10_AI/00_yolo/detect.py --download-model
10_AI/.venv/bin/python 10_AI/00_yolo/detect.py --host 192.168.0.10
```

탐지 신뢰도 조절 예시:

```bash
10_AI/.venv/bin/python 10_AI/00_yolo/detect.py \
  --host 192.168.0.10 --confidence 0.6
```

## 옵션과 문제 해결

두 프로그램 모두 `--help`로 전체 옵션을 볼 수 있습니다.

| 옵션 | 용도 |
| --- | --- |
| `--imgsz 320` | YOLO 입력 크기 축소(기본 640, 32의 배수) |
| `--model /path/to/best.pt` | 직접 학습한 객체탐지용 YOLO 모델 사용 |
| `--device cpu` | CPU 추론을 명시적으로 사용(`detect.py`만 해당, 기본 CUDA) |
| `--sensor-id 1` | 다른 CSI 카메라 인덱스 사용(기본 0) |
| `--flip 2` | 영상 180도 회전 |
| `--width 640 --height 360` | 카메라 16:9 비율 유지(기본 출력은 4:3로 리사이즈) |
| `--bitrate 1000` | 전송 비트레이트 1 Mbps |
| `--port 5001` | 수신 명령의 포트도 동일하게 변경 |
| `--test-source` | IMX219 대신 테스트 패턴을 보내 네트워크/코덱 확인 |
| `--frames 60` | 60프레임 전송 후 종료 |

카메라 없이 송수신 경로부터 확인하려면:

```bash
10_AI/.venv/bin/python 10_AI/00_yolo/camera_test.py \
  --host 192.168.0.10 --test-source
```

- 검은 화면/영상 없음: 맥 IP, 같은 네트워크, macOS 방화벽, 수신 포트 확인.
  SSH의 `-L` TCP 포워딩으로 이 UDP 스트림을 받을 수는 없습니다.
- 카메라 열기 실패: 다른 카메라 사용 프로그램 종료, CSI 연결과 `--sensor-id`
  확인. 기본 센서 입력은 1280×720@30입니다. `--fps`는 센서가 지원하는 값이어야 합니다.
- 센서 연결을 바꿀 때는 Jetson 전원을 끄세요.
- 플러그인 누락: `gst-inspect-1.0 nvarguscamerasrc`,
  `gst-inspect-1.0 x264enc`로 확인. NVIDIA 카메라 플러그인은 JetPack 구성요소입니다.

## venv 재현 및 설치 내역

저장소 루트에서 실행합니다. 기존 환경에는 이미 설치와 연결을 완료했습니다.

```bash
python3.12 -m venv 10_AI/.venv
10_AI/.venv/bin/python -m pip install --no-deps -r 10_AI/requirements.txt
10_AI/.venv/bin/python 10_AI/tools/link_jetson_runtime.py
10_AI/.venv/bin/python 10_AI/00_yolo/detect.py --download-model
```

`requirements.txt`는 직접 사용한 torch·torchvision·numpy·ultralytics와 그 런타임 의존성의
설치 버전을 기록합니다. 오디오 및 모델 내보내기용 추가 패키지는 설치하지 않았습니다.
OpenCV 4.8.0과 PyGObject는 pip 설치 목록에 없으며, 기존 Jetson 모듈만 연결합니다.
`include-system-site-packages=false`를 유지하므로 시스템 패키지 전체를 노출하지 않습니다.
시스템 Python과 venv는 동일한 Python 3.12여야 합니다. NumPy 1.26.4는 기존
OpenCV 바이너리와 호환되도록 고정했습니다.

이전 Caffe/SSDLite 모델은 `10_AI/.archive/models/mobilenet_ssd/`, `10_AI/.archive/models/ssdlite320/`에
보관되어 있지만 현재 코드는 `10_AI/models/yolo11n.pt`만 사용합니다.

최초 설치 및 YOLO 전환 시 추가한 패키지와 버전은 [설치 내역](installation.md)에 기록했습니다.

OpenCV는 기존 Jetson 모듈을 재사용하므로 `pip check`에서 `opencv-python` 누락이
표시됩니다. pip 휠 대신 연결된 `cv2`로 실행하며, `--no-deps`는 잠금 목록 전체를
설치하면서 OpenCV가 덮어써지는 것을 방지합니다.
실제 IMX219 원본 및 GPU 탐지 영상 각각 30프레임의 로컬 RTP 송수신·디코딩을 확인했습니다.
맥 자체의 수신은 사용자의 네트워크에서 확인해야 합니다.

## 참고

- [NVIDIA CSI/GStreamer 파이프라인](https://docs.nvidia.com/jetson/archives/r36.3/DeveloperGuide/SD/Multimedia/AcceleratedGstreamer.html)
- [Ultralytics YOLO11](https://docs.ultralytics.com/models/yolo11/)
- [공식 PyTorch CUDA 설치 버전](https://pytorch.org/get-started/previous-versions/)
- [GStreamer H.264 RTP](https://gstreamer.freedesktop.org/documentation/rtp/rtph264pay.html)
- [Homebrew GStreamer 설치](https://formulae.brew.sh/formula/gstreamer)
