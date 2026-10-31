#!/usr/bin/env python3
"""Local HTTP checks for the native SDK-stream queue adapter."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import subprocess
import threading
import time

class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'
    def log_message(self, *_): pass
    def do_POST(self):
        self.rfile.read(int(self.headers.get('Content-Length', '0')))
        if self.path == '/error':
            body = b'{"error":{"message":"missing"}}'
            self.send_response(404)
            self.send_header('Content-Type','application/json')
            self.send_header('Content-Length',str(len(body)))
            self.end_headers();self.wfile.write(body);self.wfile.flush();return
        self.send_response(200)
        self.send_header('Content-Type','text/event-stream')
        self.send_header('Request-Id','request-early')
        self.send_header('Connection','close')
        self.end_headers()
        if self.path in ('/reducer','/partial'):
            self.wfile.write(b'event: message_start\ndata: {"type":"message_start","message":{"type":"message","id":"x","content":[],"stop_reason":null,"usage":{"input_tokens":2,"output_tokens":0}}}\n\n')
            if self.path == '/reducer':
                self.wfile.write(b'event: content_block_start\ndata: {"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}}\n\n')
                self.wfile.write(b'event: content_block_delta\ndata: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"hello"}}\n\n')
                self.wfile.write(b'event: content_block_stop\ndata: {"type":"content_block_stop","index":0}\n\n')
                self.wfile.write(b'event: message_delta\ndata: {"type":"message_delta","delta":{"stop_reason":"end_turn"},"usage":{"output_tokens":3}}\n\n')
                self.wfile.write(b'event: message_stop\ndata: {"type":"message_stop"}\n\n')
                self.wfile.flush();return
            self.wfile.write(b'event: content_block_start\ndata: {"type":"content_block_start","index":0,"content_block":{"type":"thinking","thinking":""}}\n\n')
            self.wfile.write(b'event: content_block_delta\ndata: {"type":"content_block_delta","index":0,"delta":{"type":"thinking_delta","thinking":"partial"}}\n\n')
            self.wfile.write(b'event: content_block_delta\ndata: {invalid json}\n\n')
            self.wfile.flush();return
        self.wfile.write(b'event: message_start\ndata: {"type":"message_start","message":{"id":"x"}}\n\n')
        self.wfile.flush()
        if self.path == '/cancel':
            time.sleep(1)
            return
        if self.path == '/broken':
            self.wfile.write(b'event: content_block_delta\ndata: {invalid json}\n\n')
        else:
            self.wfile.write(b'event: message_stop\ndata: {"type":"message_stop"}\n\n')
        self.wfile.flush()

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--probe',required=True)
    args=parser.parse_args()
    server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    base=f'http://127.0.0.1:{server.server_address[1]}'
    try:
        results={}
        for path,mode in (('/success','normal'),('/error','normal'),('/broken','normal'),('/cancel','cancel'),
                          ('/reducer','reduce'),('/partial','partial')):
            process=subprocess.run([args.probe,base,path,mode],capture_output=True,text=True,timeout=12)
            if process.returncode:raise AssertionError(f'{path}: {process.stderr[:500]}')
            results[path]=json.loads(process.stdout)
        success=results['/success']
        assert success['status']==200 and success['headers']['request-id']=='request-early'
        assert [event['type'] for event in success['events']]==['message_start','message_stop']
        assert results['/error']['errorStatus']==404 and not results['/error']['events']
        assert [event['type'] for event in results['/broken']['events']]==['message_start']
        assert results['/broken']['errorName']=='Error'
        assert [event['type'] for event in results['/cancel']['events']]==['message_start']
        assert results['/cancel']['errorName']=='APIUserAbortError'
        reduction=results['/reducer']
        assert reduction['requestId']=='request-early'
        assert reduction['state']['messages'][0]['requestId']=='request-early'
        assert reduction['state']['messages'][0]['message']['content'][0]['text']=='hello'
        assert reduction['state']['messages'][0]['message']['usage']['output_tokens']==3
        partial=results['/partial']
        assert partial['state']['blocks'][0]['thinking']=='partial'
        assert partial['error'] and not partial['state']['messages']
        print('LiveStream header timing, success, 404, partial-error, cancellation and reducer checks passed')
    finally:
        server.shutdown();server.server_close();thread.join()

if __name__=='__main__':main()
