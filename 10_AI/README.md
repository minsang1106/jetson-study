# AI 실습

IMX219 → Jetson YOLO11n GPU 객체탐지 → GStreamer → 맥 화면.

## 폴더 구성

```text
10_AI/
├── 00_yolo/               # 실행 코드
│   ├── camera_test.py     # 카메라 영상 전송
│   └── detect.py          # YOLO 객체탐지 후 전송
├── models/                # 학습된 모델 파일
│   ├── yolo11n.pt
│   └── mnist_cnn.pth
├── tools/                 # 환경 준비 도구
│   └── link_jetson_runtime.py
├── docs/                  # 상세 안내
│   ├── usage.md
│   └── installation.md
├── requirements.txt       # 설치 패키지와 버전
├── README.md
├── .venv/                 # Python 실행 환경
└── .archive/              # 사용하지 않는 이전 모델·캐시 보관
```

## 실행

저장소 루트에서 실행합니다. `192.168.0.10`은 Jetson과 통신하는 맥 인터페이스의 IP로 바꾸세요.
맥에서는 먼저 [영상 수신 명령](docs/usage.md#1-맥에서-수신-준비)을 실행합니다.

```bash
source 10_AI/.venv/bin/activate
python 10_AI/00_yolo/detect.py --host 192.168.0.10
```

카메라 원본만 확인하려면 `detect.py` 대신 `camera_test.py`를 실행합니다.
종료는 Ctrl+C입니다. 두 프로그램에서 카메라를 동시에 열지 마세요.

- [실행 옵션·맥 수신·환경 재현](docs/usage.md)
- [설치한 패키지와 버전](docs/installation.md)

모델 경로와 YOLO 설정 저장 경로는 `00_yolo/detect.py` 상단에서 지정합니다.
라이브러리는 `.venv`에 설치하고, 모델은 `models`에서 읽습니다.
