"""Linux TCP integration checks; build first, keep port 8080 free."""
import concurrent.futures
import os
from pathlib import Path
import signal
import select
import socket
import subprocess
import struct
import time
import unittest

SERVER = Path(os.environ.get('CHAT_SERVER', str(Path(__file__).resolve().parents[1] / 'build/server')))

CLIENT = Path(os.environ.get("CHAT_CLIENT", str(SERVER.parent / "client")))


class ServerTest(unittest.TestCase):
    def setUp(self):
        self.output = open('/tmp/chat-server-test.log', 'w+')
        self.server = subprocess.Popen([str(SERVER)], stdout=self.output, stderr=self.output)
        self.clients = []
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if self.server.poll() is not None:
                self.fail('Server failed to start; see /tmp/chat-server-test.log')
            try:
                client = socket.create_connection(('127.0.0.1', 8080), timeout=.2)
                client.close()
                self.wait_threads(3)
                return
            except (ConnectionRefusedError, TimeoutError):
                time.sleep(.02)
        self.fail('Server startup timeout')

    def tearDown(self):
        for client in self.clients:
            client.close()
        if self.server.poll() is None:
            self.server.terminate()
            try:
                self.server.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.server.kill()
                self.server.wait()
                self.fail('Server shutdown timeout')
        self.output.seek(0)
        diagnostics = self.output.read()
        self.output.close()
        self.assertEqual(self.server.returncode, 0, diagnostics)
        self.assertNotIn('runtime error:', diagnostics)

    def wait_threads(self, count):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if len(list(Path(f'/proc/{self.server.pid}/task').iterdir())) == count:
                return
            time.sleep(.02)
        self.fail(f'Expected {count} server threads')

    def connect(self, active):
        client = socket.create_connection(('127.0.0.1', 8080), timeout=3)
        self.clients.append(client)
        self.wait_threads(3 + active)
        return client

    def receive(self, client, length):
        data = b''
        while len(data) < length:
            part = client.recv(length - len(data))
            self.assertTrue(part, 'Unexpected EOF')
            data += part
        return data

    def start_client(self):
        process = subprocess.Popen([str(CLIENT)], stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        def cleanup():
            if process.poll() is None:
                process.kill()
            process.wait()
            process.stdin.close()
            process.stdout.close()
            process.stderr.close()
        self.addCleanup(cleanup)
        ready, _, _ = select.select([process.stderr], [], [], 5)
        self.assertTrue(ready, 'Client connection timeout')
        self.assertIn(b'Connected to', process.stderr.readline())
        self.wait_threads(4)
        return process

    def read_output(self, process, length):
        result = b''
        deadline = time.monotonic() + 5
        while len(result) < length:
            ready, _, _ = select.select([process.stdout], [], [], max(0, deadline - time.monotonic()))
            self.assertTrue(ready, 'Client output timeout')
            part = os.read(process.stdout.fileno(), length - len(result))
            self.assertTrue(part, 'Client exited before output')
            result += part
        return result

    def test_client_chat_and_quit(self):
        client = self.start_client()
        peer = self.connect(2)
        peer.sendall(b'hello\x00world\n')
        self.assertEqual(self.read_output(client, 12), b'hello\x00world\n')
        self.assertEqual(self.receive(peer, 12), b'hello\x00world\n')
        message = '안녕하세요\n'.encode()
        client.stdin.write(message)
        client.stdin.flush()
        self.assertEqual(self.read_output(client, len(message)), message)
        self.assertEqual(self.receive(peer, len(message)), message)
        client.stdin.write(b'/quit\r\nignored\n')
        client.stdin.flush()
        self.assertEqual(client.wait(timeout=5), 0)
        peer.settimeout(.2)
        with self.assertRaises(TimeoutError):
            peer.recv(1)

    def test_client_input_limits_and_eof(self):
        client = self.start_client()
        peer = self.connect(2)
        accepted = b'a' * 1023 + b'\n'
        client.stdin.write(accepted + b'b' * 1024 + b'\n' + b'next\n')
        client.stdin.flush()
        expected = accepted + b'next\n'
        self.assertEqual(self.receive(peer, len(expected)), expected)
        self.assertEqual(self.read_output(client, len(expected)), expected)
        client.stdin.write(b'last line')
        client.stdin.close()
        self.assertEqual(self.receive(peer, 9), b'last line')
        self.assertEqual(client.wait(timeout=5), 0)
        self.assertIn(b'Input too long', client.stderr.read())

    def test_client_exits_on_server_shutdown(self):
        client = self.start_client()
        self.server.terminate()
        self.assertEqual(self.server.wait(timeout=5), 0)
        self.assertEqual(client.wait(timeout=5), 0)
        self.assertIn(b'Server disconnected', client.stderr.read())

    def test_client_invalid_arguments(self):
        for arguments in [['invalid-address'], ['127.0.0.1', 'extra']]:
            result = subprocess.run([str(CLIENT), *arguments], capture_output=True, timeout=5)
            self.assertEqual(result.returncode, 1)
            self.assertTrue(result.stderr)

    def test_broadcast_binary_concurrent_and_reconnect(self):
        clients = [self.connect(i + 1) for i in range(3)]
        payload = b'hello\x00world\n' + bytes(range(256)) * 16
        clients[0].sendall(payload)
        for client in clients:
            self.assertEqual(self.receive(client, len(payload)), payload)
        chunks = [bytes([65+i]) * 4096 for i in range(3)]
        with concurrent.futures.ThreadPoolExecutor() as pool:
            list(pool.map(lambda pair: pair[0].sendall(pair[1]), zip(clients, chunks)))
        streams = [self.receive(client, sum(map(len, chunks))) for client in clients]
        self.assertEqual(streams[0], streams[1])
        self.assertEqual(streams[1], streams[2])
        for byte in b'ABC':
            self.assertEqual(streams[0].count(byte), 4096)
        clients[1].close()
        self.wait_threads(5)
        clients[1] = self.connect(3)
        clients[2].sendall(b'again')
        for client in clients:
            self.assertEqual(self.receive(client, 5), b'again')

    def test_capacity_and_shutdown_while_full(self):
        clients = [self.connect(i + 1) for i in range(5)]
        waiting = socket.create_connection(('127.0.0.1', 8080), timeout=3)
        self.clients.append(waiting)
        waiting.sendall(b'pending')
        waiting.settimeout(.2)
        with self.assertRaises(TimeoutError):
            waiting.recv(1)
        clients[0].close()
        waiting.settimeout(3)
        for client in clients[1:] + [waiting]:
            self.assertEqual(self.receive(client, 7), b'pending')
        self.server.send_signal(signal.SIGINT)
        self.assertEqual(self.server.wait(timeout=10), 0)
        for client in clients[1:] + [waiting]:
            self.assertEqual(client.recv(1), b'')

    def test_idle_shutdown(self):
        self.wait_threads(3)
        self.server.terminate()
        self.assertEqual(self.server.wait(timeout=5), 0)

    def test_reset_during_broadcast(self):
        sender = self.connect(1)
        broken = self.connect(2)
        broken.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
        broken.close()
        payload = b'after-reset' * 512
        sender.sendall(payload)
        self.assertEqual(self.receive(sender, len(payload)), payload)
        self.wait_threads(4)
        replacement = self.connect(2)
        replacement.sendall(b'ok')
        self.assertEqual(self.receive(sender, 2), b'ok')
        self.assertEqual(self.receive(replacement, 2), b'ok')

    def test_repeated_disconnects(self):
        for _ in range(30):
            client = self.connect(1)
            client.sendall(b'echo')
            self.assertEqual(self.receive(client, 4), b'echo')
            client.close()
            self.wait_threads(3)


if __name__ == '__main__':
    unittest.main()
