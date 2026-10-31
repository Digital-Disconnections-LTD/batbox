#!/usr/bin/env python3
import argparse,http.server,json,subprocess,threading,time
from pathlib import Path
class Server(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        time.sleep(.35)
        payload=json.dumps({'results':[]}).encode()
        self.send_response(200);self.send_header('Content-Length',str(len(payload)));self.end_headers()
        try:self.wfile.write(payload)
        except (BrokenPipeError,ConnectionResetError):pass
    def log_message(self,*_):pass
parser=argparse.ArgumentParser();parser.add_argument('--probe',required=True);args=parser.parse_args()
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Server)
threading.Thread(target=server.serve_forever,daemon=True).start()
try:
    result=subprocess.run([args.probe,str(Path(__file__).with_name('fake_scrapling.py')),
        f'http://127.0.0.1:{server.server_port}/search'],capture_output=True,text=True,timeout=10)
    if result.returncode:raise AssertionError(result.stderr)
    print(result.stdout.strip())
finally:server.shutdown();server.server_close()
