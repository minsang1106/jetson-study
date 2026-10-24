# 멀티스레드 TCP 채팅 서버

Linux/Jetson에서 C와 POSIX threads로 실행한다. TCP 8080에서 최대 5개
클라이언트를 처리하며, 수신한 바이트를 송신자 포함 현재 접속자 모두에게 전달한다.

## 빌드와 실행

```bash
make
./build/server
# 다른 터미널에서 여러 개 실행
./build/client
# 다른 장치에 있는 서버에 연결
./build/client 192.168.0.10
```

`make`는 서버와 클라이언트를 함께 생성한다. 클라이언트 기본 주소는 127.0.0.1이다.
클라이언트는 Enter로 전송하고 `/quit` 또는 Ctrl+D로 종료한다.
서버 종료는 Ctrl+C 또는 SIGTERM이다.

클라이언트는 `poll()`로 표준 입력과 socket을 함께 기다리므로 입력 중에도
브로드캐스트를 수신한다. 별도 스레드나 공유 자원 mutex는 필요하지 않다.
서버가 연결을 닫으면 입력을 기다리지 않고 종료한다. 상태 안내는 stderr,
수신 데이터는 stdout에 표시한다. `/quit`은 클라이언트 내부 명령이며 서버에 보내지 않는다.

입력 한 줄은 줄바꿈 포함 최대 1024바이트다. 초과한 줄은 전송하지 않고 버린 뒤
다음 줄부터 계속 처리한다. EOF에서 남은 줄은 전송 후 연결을 닫는다.
종료 직전 전송한 메시지의 자기 브로드캐스트 수신까지 기다리지는 않는다.
이 입력 길이 제한은 클라이언트 UI 정책이며 서버 프로토콜 제한은 아니다.
부분 전송과 EINTR를 처리하고 MSG_NOSIGNAL을 사용한다. send에는 2초 타임아웃이
있으며, 블로킹 전송 중에는 수신 화면 갱신이 일시적으로 지연될 수 있다.

## 기존 구현과 Structure Chart 대응

기존 `main()`의 socket/bind/listen은 `initialize_server()`, 슬롯 대기와
accept/등록/스레드 생성은 `connection_manager()`로 옮겼다.
`receiver()`의 recv/큐 삽입/연결 정리는 `client_handler()`와 하위 함수로 나눴다.
미완성이던 `sender()`는 `broadcast_manager()`와 `broadcast_message()`로 완성했다.
`data_structure.c/h`의 연결 리스트 큐는 `message_queue.c/h`로 옮기고
단일 mutex와 condition variable로 producer/consumer를 동기화했다.

기존 소스는 잘못된 fd 접근과 미정의 변수 때문에 컴파일되지 않았다.
기존 README에 있던 줄 단위 처리, `[Client N]` 접두사, `/quit`, 초과 접속 거절은
리팩터링 전 소스에 구현되어 있지 않았다. 이번 변경은 수신 코드의 바이트 스트림 처리와
슬롯 대기 정책을 따른다. TCP recv 경계는 메시지/줄 경계가 아니며, 길이를 함께
보관하므로 NUL을 포함한 데이터도 전달한다. 1024바이트는 한 번의 수신 크기다.
슬롯이 모두 차면 accept 전에 기다리므로 추가 TCP 연결은 커널 backlog에서 대기한다.

## 파일별 책임

| 파일 | 책임 |
| --- | --- |
| `client.c` | IPv4 연결, 입력/수신 poll 루프, 내부 종료 명령 |
| `main.c` | 초기화 → 실행 → 종료 정리 |
| `server.c/h` | 서버 자원, listening socket 초기화, 스레드 시작과 종료 순서 |
| `connection.c/h` | accept, 클라이언트 스레드 생성, 수신, disconnect |
| `client_manager.c/h` | 목록 추가/제거/조회, 슬롯 대기, 활성 handler 종료 대기 |
| `message_queue.c/h` | 길이가 있는 메시지의 FIFO 삽입/대기 인출/종료 |
| `broadcast.c/h` | broadcaster 루프, 부분 전송 재시도, 전체 접속자 전달 |
| `tests/test_server.py` | 실제 TCP 연결을 사용하는 Linux 통합 테스트 |

## Thread / data flow

```text
main → initialize_server → run_server
                             ├─ Connection Manager Thread
                             │    accept_client → add_client → create_client_thread
                             └─ Broadcaster Thread

Client 1 Thread ─┐
Client 2 Thread ─┼─> Message Queue ─> Broadcaster Thread ─> Connected Clients
Client 3 Thread ─┘

client_handler → receive_message → enqueue_message
               → queue → dequeue_message → broadcast_message
                        → get_client_list → send_message

client_handler → disconnect detection → remove_client → close → free
```

`main`은 SIGINT/SIGTERM을 동기적으로 기다린다. 각 worker는 신호를 block한
상태를 상속한다. 종료 시 슬롯 대기와 recv를 깨우고 listening socket을 shutdown하여
Linux의 accept를 깨운다. Connection Manager를 join하고 모든 detached handler의
정리가 끝나기를 기다린 다음 큐를 닫고 broadcaster를 join한다. 마지막으로 socket과
mutex/condition variable을 해제한다. 종료 중 메시지 전달은 보장하지 않는다.

## 공유 자원과 소유권

- 큐: 하나의 mutex가 head/tail을 함께 보호한다. 빈 큐에서는 condition variable로
  기다린다. 여러 handler가 생산하고 broadcaster 하나만 소비한다. 삽입 시 데이터를
  복사하며 인출된 메시지는 broadcaster가 해제한다.
- 클라이언트 목록: 별도 mutex로 보호한다. `get_client_list()`는 잠금을 잡은 채
  fd 목록을 반환하고 `release_client_list()`가 해제한다. 전송 중 remove/close가
  진행되지 않아 닫힌 fd의 재사용을 막는다. 큐 잠금과 목록 잠금을 중첩하지 않는다.
- 각 handler 인자는 heap에 할당한다. handler가 목록 제거 후 socket/client/인자를
  정리한다. 생성 실패 시 Connection Manager가 같은 순서로 정리한다.
- active handler 수는 정리가 끝난 뒤 감소한다. 종료 대기 중 서버 메모리가 먼저
  해제되지 않으며, 슬롯 대기는 condition variable을 사용한다.
- listening socket에는 mutex를 사용하지 않는다. accept는 Connection Manager만 한다.

부분 전송과 EINTR를 처리하고 MSG_NOSIGNAL로 SIGPIPE를 막는다. 송신 실패 시
shutdown으로 수신 스레드를 깨우며 close/free는 해당 handler가 담당한다.
학습용으로 목록 mutex를 잡고 전송한다. 각 send는 2초 타임아웃을 사용하지만
느린 수신자는 다른 연결을 지연시킬 수 있다. 전체 전송 시간 제한은 아니다.
큐 용량에는 상한이 없으므로 생산 속도가 지속적으로 더 빠르면 메모리 사용량이 증가한다.

## 검증

8080 포트를 비운 상태에서 실행한다.

```bash
make CFLAGS='-std=c11 -Wall -Wextra -Wpedantic -Werror -g'
python3 tests/test_server.py
```

바이너리 데이터, 1024바이트 초과 전송, 다중 producer의 동일 순서 브로드캐스트,
5개 슬롯과 대기 연결, 재접속, 반복 연결 해제, idle/최대 접속 상태 종료를 검사한다.
클라이언트의 입력 없는 수신, 한글 송수신, `/quit`, EOF, 입력 길이 제한,
서버 종료 감지와 잘못된 인자 처리도 검사한다.
