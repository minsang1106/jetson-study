#!/usr/bin/env python3
"""Headless IMX219 capture and H.264/RTP transmission to a Mac."""

import argparse
import ipaddress
import signal
import sys
import time


def positive(value):
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("양수를 입력하세요.")
    return number


def ipv4(value):
    try:
        return str(ipaddress.IPv4Address(value))
    except ipaddress.AddressValueError as exc:
        raise argparse.ArgumentTypeError("맥의 IPv4 주소를 입력하세요.") from exc


def make_parser(description=__doc__):
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("--host", type=ipv4, help="수신할 맥의 IPv4 주소")
    parser.add_argument("--port", type=positive, default=5000)
    parser.add_argument("--width", type=positive, default=640)
    parser.add_argument("--height", type=positive, default=480)
    parser.add_argument("--fps", type=positive, default=30, help="카메라/송신 목표 FPS")
    parser.add_argument("--bitrate", type=positive, default=2000, help="H.264 kbit/s")
    parser.add_argument("--sensor-id", type=int, choices=(0, 1), default=0)
    parser.add_argument("--flip", type=int, choices=range(8), default=0)
    parser.add_argument("--frames", type=positive, help="지정한 프레임 수 전송 후 종료")
    parser.add_argument("--test-source", action="store_true", help="카메라 대신 테스트 패턴")
    return parser


def validate_args(parser, args):
    if not args.host:
        parser.error("--host 맥_IP가 필요합니다.")
    if args.port > 65535:
        parser.error("--port는 1~65535여야 합니다.")
    if args.width % 2 or args.height % 2:
        parser.error("H.264 전송에는 짝수 width/height를 사용하세요.")


def load_runtime():
    try:
        import cv2
        import gi
        gi.require_version("Gst", "1.0")
        from gi.repository import Gst
    except ImportError as exc:
        raise RuntimeError(
            "10_AI/.venv/bin/python으로 실행하세요. "
            "cv2/gi 연결이 없으면 같은 Python으로 10_AI/tools/link_jetson_runtime.py를 실행하세요."
        ) from exc
    Gst.init(None)
    if not cv2.videoio_registry.hasBackend(cv2.CAP_GSTREAMER):
        raise RuntimeError("GStreamer 지원 OpenCV가 필요합니다. pip OpenCV 대신 시스템 OpenCV를 사용하세요.")
    return cv2, Gst


def capture_pipeline(args):
    if args.test_source:
        source = (
            "videotestsrc is-live=true ! "
            f"video/x-raw,width={args.width},height={args.height},framerate={args.fps}/1 ! "
            "videoconvert ! video/x-raw,format=BGR"
        )
    else:
        # Capture at a native IMX219 mode; resize using the Jetson converter.
        source = (
            f"nvarguscamerasrc sensor-id={args.sensor_id} ! "
            "video/x-raw(memory:NVMM),width=1280,height=720,format=NV12,"
            f"framerate={args.fps}/1 ! nvvidconv flip-method={args.flip} ! "
            f"video/x-raw,width={args.width},height={args.height},format=BGRx ! "
            "videoconvert ! video/x-raw,format=BGR"
        )
    return source + " ! appsink max-buffers=1 drop=true sync=false"


class Sender:
    """Timestamp each processed frame at its actual submission time."""

    def __init__(self, Gst, args):
        self.Gst = Gst
        self.pipeline = Gst.parse_launch(
            "appsrc name=source is-live=true format=time do-timestamp=true block=false "
            "! queue max-size-buffers=2 max-size-bytes=0 max-size-time=0 leaky=downstream "
            "! videoconvert ! video/x-raw,format=I420 "
            f"! x264enc tune=zerolatency speed-preset=ultrafast bitrate={args.bitrate} "
            f"key-int-max={args.fps} bframes=0 "
            "! video/x-h264,profile=baseline "
            "! rtph264pay pt=96 config-interval=1 mtu=1200 "
            f"! udpsink host={args.host} port={args.port} sync=false async=false"
        )
        self.source = self.pipeline.get_by_name("source")
        self.source.set_property("caps", Gst.Caps.from_string(
            f"video/x-raw,format=BGR,width={args.width},height={args.height},"
            f"framerate={args.fps}/1"
        ))
        self.bus = self.pipeline.get_bus()
        if self.pipeline.set_state(Gst.State.PLAYING) == Gst.StateChangeReturn.FAILURE:
            self.pipeline.set_state(Gst.State.NULL)
            raise RuntimeError("송신 GStreamer 파이프라인을 시작하지 못했습니다.")

    def check(self):
        message = self.bus.pop_filtered(self.Gst.MessageType.ERROR)
        if message:
            error, debug = message.parse_error()
            raise RuntimeError(f"GStreamer 송신 오류: {error.message}\n{debug or ''}")

    def send(self, frame):
        self.check()
        # Raw BGR rows use a four-byte stride in GStreamer.
        import numpy as np
        rows = frame.reshape(frame.shape[0], -1)
        padding = (-rows.shape[1]) % 4
        data = np.pad(rows, ((0, 0), (0, padding))).tobytes() if padding else frame.tobytes()
        buffer = self.Gst.Buffer.new_allocate(None, len(data), None)
        buffer.fill(0, data)
        if self.source.emit("push-buffer", buffer) != self.Gst.FlowReturn.OK:
            raise RuntimeError("송신 파이프라인이 프레임을 받지 못했습니다.")

    def close(self):
        try:
            self.source.emit("end-of-stream")
            message = self.bus.timed_pop_filtered(
                2 * self.Gst.SECOND, self.Gst.MessageType.EOS | self.Gst.MessageType.ERROR
            )
            if message and message.type == self.Gst.MessageType.ERROR:
                error, _ = message.parse_error()
                raise RuntimeError(f"GStreamer 송신 오류: {error.message}")
        finally:
            self.pipeline.set_state(self.Gst.State.NULL)


def stream(args, process=None):
    cv2, Gst = load_runtime()
    required = ["appsrc", "appsink", "videoconvert", "x264enc", "rtph264pay", "udpsink"]
    required += ["videotestsrc"] if args.test_source else ["nvarguscamerasrc", "nvvidconv"]
    missing = [name for name in required if not Gst.ElementFactory.find(name)]
    if missing:
        raise RuntimeError("GStreamer 플러그인 누락: " + ", ".join(missing))
    cap = cv2.VideoCapture()
    sender = None
    count = 0
    started = time.monotonic()
    report = started
    stopped = False

    def stop(signum, frame):
        nonlocal stopped
        stopped = True

    previous = {sig: signal.signal(sig, stop) for sig in (signal.SIGINT, signal.SIGTERM)}
    try:
        if not cap.open(capture_pipeline(args), cv2.CAP_GSTREAMER, [
            cv2.CAP_PROP_OPEN_TIMEOUT_MSEC, 10000,
            cv2.CAP_PROP_READ_TIMEOUT_MSEC, 5000,
        ]):
            raise RuntimeError("카메라를 열 수 없습니다. 연결, sensor-id, nvargus-daemon을 확인하세요.")
        sender = Sender(Gst, args)
        print(f"송신 시작 → {args.host}:{args.port} (H.264/RTP), Ctrl+C로 종료", flush=True)
        while not stopped and (args.frames is None or count < args.frames):
            ok, frame = cap.read()
            if not ok:
                raise RuntimeError("카메라 프레임 수신 실패/시간 초과")
            if process:
                frame = process(frame)
            sender.send(frame)
            count += 1
            now = time.monotonic()
            if now - report >= 5:
                print(f"{count} frames, 평균 {count / (now - started):.1f} FPS", flush=True)
                report = now
        sender.check()
    finally:
        cap.release()
        try:
            if sender:
                sender.close()
        finally:
            for sig, handler in previous.items():
                signal.signal(sig, handler)
    print(f"종료: {count} frames 전송")


def main():
    parser = make_parser()
    args = parser.parse_args()
    validate_args(parser, args)
    stream(args)


def cli(main_function):
    try:
        main_function()
    except KeyboardInterrupt:
        pass
    except Exception as exc:
        print(f"오류: {exc}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    cli(main)
