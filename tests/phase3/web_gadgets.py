#!/usr/bin/env python3
import argparse,http.server,json,os,subprocess,tempfile,threading
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--probe',required=True);a=p.parse_args()
class Search(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  payload={'results':[{'title':'Allowed','url':'https://docs.example.org/a'},
                      {'title':'Excluded','url':'https://bad.test/b'}]}
  body=json.dumps(payload).encode();self.send_response(200);self.send_header('Content-Type','application/json');self.end_headers();self.wfile.write(body)
 def log_message(self,*args):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Search)
threading.Thread(target=server.serve_forever,daemon=True).start()
try:
 with tempfile.TemporaryDirectory() as tmp:
  log=Path(tmp)/'fetch.log'
  fixture=Path(__file__).with_name('web_fake_scrapling.py')
  fixture.chmod(0o755)
  env=dict(os.environ,BATBOX_WEB_FETCH_LOG=str(log))
  result=subprocess.run([a.probe,f'http://127.0.0.1:{server.server_port}/search',str(fixture)],
                        text=True,capture_output=True,env=env,timeout=20)
  assert result.returncode==0,(result.stdout,result.stderr)
  lines=log.read_text().splitlines()
  assert lines==['https://docs.example.org/a','https://ordinary.example.org/a','https://redirect.example.org/a'],lines
  print(result.stdout.strip())
finally:server.shutdown();server.server_close()
