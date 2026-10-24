#!/usr/bin/env python3
"""YOLO11 detection on IMX219, streamed headlessly to a Mac."""
import os
from pathlib import Path
import shutil
import time
import urllib.request

from camera_test import cli, load_runtime, make_parser, positive, stream, validate_args

AI_DIR = Path(__file__).resolve().parents[1]
MODEL_DIR = AI_DIR / 'models'
MODEL_NAME = 'yolo11n.pt'
MODEL_URL = 'https://github.com/ultralytics/assets/releases/download/v8.3.0/' + MODEL_NAME
# Keep library configuration/caches local and prevent automatic pip installs.
os.environ.setdefault('YOLO_CONFIG_DIR', str(AI_DIR / '.venv' / '.ultralytics'))
os.environ.setdefault('MPLCONFIGDIR', str(AI_DIR / '.venv' / '.matplotlib'))
os.environ.setdefault('CUDA_CACHE_PATH', str(AI_DIR / '.venv' / '.cuda-cache'))
os.environ['YOLO_AUTOINSTALL'] = 'false'


def download_model(directory):
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / MODEL_NAME
    if path.is_file() and path.stat().st_size:
        print(f'기존 모델 사용: {path}')
        return
    temporary = path.with_suffix('.pt.part')
    try:
        with urllib.request.urlopen(MODEL_URL, timeout=60) as response:
            with temporary.open('wb') as output:
                shutil.copyfileobj(response, output)
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)
    print(f'YOLO 모델 준비 완료: {path}')


class Detector:
    def __init__(self, cv2, path, confidence, device, imgsz):
        Path(os.environ['YOLO_CONFIG_DIR']).expanduser().mkdir(parents=True, exist_ok=True)
        import numpy as np
        import torch
        from ultralytics import YOLO, settings
        settings.update({'sync': False})
        if device == 'auto':
            device = 'cuda' if torch.cuda.is_available() else 'cpu'
        if device == 'cuda' and not torch.cuda.is_available():
            raise RuntimeError('CUDA를 사용할 수 없습니다. 설치를 확인하거나 --device cpu를 지정하세요.')
        if not path.is_file():
            raise RuntimeError(f'모델이 없습니다: {path}. 먼저 detect.py --download-model을 실행하세요.')
        self.cv2 = cv2
        self.device = device
        self.confidence = confidence
        self.imgsz = imgsz
        torch.set_num_threads(2)
        self.model = YOLO(str(path), task='detect')
        if self.model.task != 'detect':
            raise RuntimeError('객체탐지용 YOLO 모델을 지정하세요.')
        self.predict(np.zeros((imgsz, imgsz, 3), dtype=np.uint8))
        if device == 'cuda':
            torch.cuda.synchronize()
        print(f'YOLO: {path.name}, PyTorch {torch.__version__}, 추론 장치: {device}', flush=True)

    def predict(self, frame):
        return self.model.predict(
            source=frame, conf=self.confidence, imgsz=self.imgsz,
            device=0 if self.device == 'cuda' else 'cpu',
            verbose=False, save=False, show=False,
        )[0]

    def __call__(self, frame):
        cv2 = self.cv2
        started = time.monotonic()
        result = self.predict(frame)
        detections = result.boxes.data.cpu().tolist()
        height, width = frame.shape[:2]
        count = 0
        for x1, y1, x2, y2, score, class_id in detections:
            x1, x2 = (max(0, min(width - 1, int(x))) for x in (x1, x2))
            y1, y2 = (max(0, min(height - 1, int(y))) for y in (y1, y2))
            if x2 <= x1 or y2 <= y1:
                continue
            count += 1
            cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 255, 0), 2)
            cv2.putText(frame, f'{result.names[int(class_id)]} {score:.0%}',
                        (x1, max(18, y1 - 6)), cv2.FONT_HERSHEY_SIMPLEX,
                        0.5, (0, 255, 0), 2)
        elapsed = time.monotonic() - started
        cv2.putText(frame, f'YOLO {self.device.upper()}: {elapsed * 1000:.0f} ms | objects: {count}',
                    (10, height - 12), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1)
        return frame


def main():
    parser = make_parser(__doc__)
    parser.add_argument('--model-dir', type=Path, default=MODEL_DIR)
    parser.add_argument('--model', type=Path, help='사용할 객체탐지용 YOLO .pt 파일 경로')
    parser.add_argument('--confidence', type=float, default=0.5)
    parser.add_argument('--imgsz', type=positive, default=640, help='YOLO 입력 크기(32의 배수)')
    parser.add_argument('--device', choices=('cuda', 'cpu', 'auto'), default='cuda')
    parser.add_argument('--download-model', action='store_true', help='기본 YOLO11n 다운로드 후 종료')
    args = parser.parse_args()
    if args.download_model:
        if args.model:
            parser.error('--download-model은 기본 YOLO11n 전용입니다. --model 없이 실행하세요.')
        download_model(args.model_dir)
        return
    validate_args(parser, args)
    if not 0 < args.confidence <= 1:
        parser.error('--confidence는 0 초과 1 이하여야 합니다.')
    if args.imgsz % 32:
        parser.error('--imgsz는 32의 배수여야 합니다.')
    cv2, _ = load_runtime()
    cv2.setNumThreads(2)
    path = args.model if args.model else args.model_dir / MODEL_NAME
    stream(args, Detector(cv2, path, args.confidence, args.device, args.imgsz))


if __name__ == '__main__':
    cli(main)
