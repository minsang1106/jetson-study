# AI 실습

`model/mnist_cnn.pth`에 모델 파일을 보관합니다.

## Python 환경 준비

저장소 루트에서 실행합니다. 기존 가상환경은 Python 3.12.3으로 생성했습니다.

```bash
python3.12 -m venv 10_AI/.venv
source 10_AI/.venv/bin/activate
python -m pip install -r 10_AI/requirements.txt
```

가상환경(`.venv/`)은 루트 `.gitignore`로 Git에서 제외하고,
설치할 패키지 목록은 `requirements.txt`로 관리합니다.
현재 가상환경에는 추가 설치된 패키지가 없어 목록이 비어 있습니다.
모델 실행에 필요한 의존성은 로딩·추론 코드를 추가할 때 기록해야 합니다.

이 가상환경에 실습용 패키지를 설치한 뒤에는 저장소 루트에서 목록을 갱신합니다.

```bash
python -m pip freeze > 10_AI/requirements.txt
```
