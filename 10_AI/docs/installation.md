# venv 설치 내역

설치 위치: `10_AI/.venv`

직접 선택: `torch`, `torchvision`, `numpy`. 나머지는 이들의 필수 의존성으로 pip가 설치했습니다.
시스템 Python/apt 패키지를 설치하거나 변경하지 않았습니다. 기존 cv2와 gi는 venv 내부의 심볼릭 링크로 재사용합니다.

| 패키지 | 설치 버전 |
| --- | --- |
| Jinja2 | 3.1.6 |
| MarkupSafe | 3.0.3 |
| cuda-bindings | 13.0.3 |
| cuda-pathfinder | 1.6.0 |
| filelock | 3.32.3 |
| fsspec | 2026.7.0 |
| mpmath | 1.3.0 |
| networkx | 3.6.1 |
| numpy | 1.26.4 |
| nvidia-cublas | 13.1.0.3 |
| nvidia-cuda-cupti | 13.0.85 |
| nvidia-cuda-nvrtc | 13.0.88 |
| nvidia-cuda-runtime | 13.0.96 |
| nvidia-cudnn-cu13 | 9.15.1.9 |
| nvidia-cufft | 12.0.0.61 |
| nvidia-cufile | 1.15.1.6 |
| nvidia-curand | 10.4.0.35 |
| nvidia-cusolver | 12.0.4.66 |
| nvidia-cusparse | 12.6.3.3 |
| nvidia-cusparselt-cu13 | 0.8.0 |
| nvidia-nccl-cu13 | 2.28.9 |
| nvidia-nvjitlink | 13.0.88 |
| nvidia-nvshmem-cu13 | 3.4.5 |
| nvidia-nvtx | 13.0.85 |
| pillow | 12.3.0 |
| setuptools | 78.1.0 |
| sympy | 1.14.0 |
| torch | 2.10.0+cu130 |
| torchvision | 0.25.0+cu130 |
| triton | 3.6.0 |
| typing_extensions | 4.16.0 |

별도 모델 파일: `10_AI/.archive/models/ssdlite320/`의 torchvision SSDLite 가중치.
설치 원본: [공식 PyTorch CUDA 휠](https://download.pytorch.org/whl/cu130).

## YOLO 전환 시 추가 설치

기존 torch·torchvision·numpy 버전은 유지했습니다.
YOLO11n용 Ultralytics와 아래 런타임 의존성을 venv에 추가했습니다.

| 패키지 | 버전 |
| --- | --- |
| PyYAML | 6.0.3 |
| certifi | 2026.7.22 |
| charset-normalizer | 3.5.2 |
| contourpy | 1.3.3 |
| cycler | 0.12.1 |
| fonttools | 4.66.1 |
| idna | 3.20 |
| kiwisolver | 1.5.1 |
| matplotlib | 3.11.2 |
| packaging | 26.3 |
| polars | 1.44.2 |
| polars-runtime-32 | 1.44.2 |
| psutil | 7.2.2 |
| pyparsing | 3.3.3 |
| python-dateutil | 2.9.0.post0 |
| requests | 2.34.2 |
| scipy | 1.17.1 |
| six | 1.17.0 |
| ultralytics | 8.3.232 |
| ultralytics-thop | 2.2.2 |
| urllib3 | 2.8.0 |

OpenCV는 기존 Jetson cv2 4.8.0을 연결해서 사용합니다. 따라서 pip check는
Ultralytics의 opencv-python 배포 패키지가 없다고 보고하지만 cv2 import와 GStreamer는
실제 실행으로 검증합니다. 일반 opencv-python 휠을 추가 설치하지 마세요.
전체 잠금 목록 설치에는 --no-deps를 사용해 이 대체 구성을 유지합니다.
YOLO11n 가중치: 10_AI/models/yolo11n.pt. 이전 모델 파일은 그대로 보관합니다.
