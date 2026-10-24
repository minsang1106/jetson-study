#!/usr/bin/env python3
"""Expose only Jetson's existing cv2 and gi modules inside this project's venv."""
import json
from pathlib import Path
import subprocess
import sys
import sysconfig


def main():
    expected = Path(__file__).resolve().parents[1] / '.venv'
    if Path(sys.prefix).resolve() != expected.resolve() or sys.prefix == sys.base_prefix:
        raise SystemExit('10_AI/.venv/bin/python으로 실행하세요.')
    # Do not expose the entire system site-packages directory.
    paths = json.loads(subprocess.check_output([
        '/usr/bin/python3', '-c',
        "import cv2, gi, json, sys; "
        "print(json.dumps({'version': list(sys.version_info[:2]), "
        "'cv2': cv2.__path__[0], 'gi': gi.__path__[0]}))",
    ], text=True))
    if paths.pop('version') != list(sys.version_info[:2]):
        raise SystemExit('시스템 Python과 venv의 버전이 달라 바이너리를 재사용할 수 없습니다.')
    site = Path(sysconfig.get_paths()['purelib'])
    for name, source in paths.items():
        destination = site / name
        if destination.is_symlink() and destination.resolve() == Path(source).resolve():
            print(f'기존 연결: {destination} -> {source}')
        elif destination.exists() or destination.is_symlink():
            raise SystemExit(f'기존 모듈을 덮어쓰지 않습니다: {destination}')
        else:
            destination.symlink_to(source, target_is_directory=True)
            print(f'연결: {destination} -> {source}')


if __name__ == '__main__':
    main()
