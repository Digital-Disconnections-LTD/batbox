#!/usr/bin/env python3
"""Requests to this machine never go through a proxy from the environment.

With http_proxy, https_proxy and ALL_PROXY pointing at a recording proxy and no
no_proxy, batbox --print reaches a loopback model endpoint directly, and
batbox doctor's probes of local model servers and SearXNG never reach the proxy.
"""
import argparse
import http.server
import json
import os
import subprocess
import tempfile
import threading
from pathlib import Path

from loopback_http import LoopbackHTTPServer


def events(model):
    return [
        ('message_start', {'type': 'message_start', 'message': {
            'type': 'message', 'id': 'msg-loopback', 'model': model, 'role': 'assistant', 'content': [],
            'stop_reason': None, 'usage': {'input_tokens': 5, 'output_tokens': 0}}}),
        ('content_block_start', {'type': 'content_block_start', 'index': 0,
                                 'content_block': {'type': 'text', 'text': ''}}),
        ('content_block_delta', {'type': 'content_block_delta', 'index': 0,
                                 'delta': {'type': 'text_delta', 'text': 'direct ok'}}),
        ('content_block_stop', {'type': 'content_block_stop', 'index': 0}),
        ('message_delta', {'type': 'message_delta', 'delta': {'stop_reason': 'end_turn'},
                           'usage': {'output_tokens': 2}}),
        ('message_stop', {'type': 'message_stop'})]


class Model(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *_):
        pass

    def do_POST(self):
        body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        with self.server.lock:
            self.server.requests.append(self.path)
        raw = b''.join(f'event: {event}\ndata: {json.dumps(value)}\n\n'.encode()
                       for event, value in events(body.get('model', 'served')))
        self.send_response(200)
        self.send_header('Content-Type', 'text/event-stream')
        self.send_header('Content-Length', str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)


class Proxy(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def record(self):
        with self.server.lock:
            self.server.requests.append(f'{self.command} {self.path}')
        self.send_response(502)
        self.send_header('Content-Length', '0')
        self.end_headers()

    do_GET = do_POST = do_CONNECT = record


def serve(handler):
    server = LoopbackHTTPServer(('127.0.0.1', 0), handler)
    server.lock = threading.Lock()
    server.requests = []
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    return server, thread


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    binary = parser.parse_args().binary.resolve(strict=True)
    model, model_thread = serve(Model)
    proxy, proxy_thread = serve(Proxy)
    proxy_url = f'http://127.0.0.1:{proxy.server_port}'
    try:
        with tempfile.TemporaryDirectory(prefix='batbox-loopback-proxy-') as temporary:
            root = Path(temporary)
            config = root / 'config'
            config.mkdir()
            work = root / 'work'
            work.mkdir()
            (config / 'native-client.json').write_text(json.dumps({
                'profile': 'local::loopback-model', 'endpoints': {'local': {
                    'label': 'local', 'base_url': f'http://localhost:{model.server_port}',
                    'auth_header': 'x-api-key', 'auth_token': 'local-key', 'max_retries': 0,
                    'context_window': 200000, 'max_output_tokens': 4096, 'models': ['loopback-model']}},
                'state_directory': str(root / 'state'), 'mcpServers': {}, 'thinking': False,
                'auto_title': False}), encoding='utf-8')
            env = {key: value for key, value in os.environ.items()
                   if not key.startswith('BATBOX_') and key.lower() not in ('no_proxy',)}
            env.update(BATBOX_CONFIG_DIR=str(config), HOME=str(root), http_proxy=proxy_url,
                       HTTP_PROXY=proxy_url, https_proxy=proxy_url, HTTPS_PROXY=proxy_url,
                       ALL_PROXY=proxy_url, all_proxy=proxy_url)
            done = subprocess.run([str(binary), '--cwd', str(work), '--no-thinking', '--print', 'Say ok'],
                                  env=env, stdin=subprocess.DEVNULL, capture_output=True, text=True,
                                  timeout=60)
            assert done.returncode == 0 and 'direct ok' in done.stdout, (done.returncode, done.stdout,
                                                                         done.stderr)
            with model.lock:
                assert model.requests, 'the loopback endpoint was not reached'
            subprocess.run([str(binary), 'doctor'], env=env, stdin=subprocess.DEVNULL,
                           capture_output=True, text=True, timeout=60)
            with proxy.lock:
                assert not proxy.requests, proxy.requests
    finally:
        for server, thread in ((model, model_thread), (proxy, proxy_thread)):
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)
    print('loopback requests and local probes bypass the proxy from the environment')


if __name__ == '__main__':
    main()
