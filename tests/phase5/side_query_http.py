import argparse,collections,http.server,json,pathlib,subprocess,tempfile,threading,time
p=argparse.ArgumentParser();p.add_argument('--probe',required=True);a=p.parse_args()
calls=collections.Counter();requests=[]
class Handler(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_POST(self):
  body=json.loads(self.rfile.read(int(self.headers['content-length'])))
  case=body.get('case','memory');calls[case]+=1
  requests.append((case,body,dict(self.headers)))
  headers={};status=200;answer={'ok':True}
  if case=='memory':
   time.sleep(.015)
   if calls[case]==1:status=503;headers={'retry-after-ms':'2'};answer={'error':{'message':'warmup'}}
   else:answer={'content':[{'type':'text','text':'{"selected_memories":["notes.md"]}'}]}
  elif case=='permanent':status=500;headers={'x-should-retry':'false'}
  elif case=='exhaust':status=429;headers={'retry-after-ms':'2'}
  elif case=='force' and calls[case]==1:status=400;headers={'x-should-retry':'true','retry-after-ms':'2'}
  elif case=='cancel':status=503;headers={'retry-after-ms':'60000'}
  raw=b'{broken' if case=='malformed' else json.dumps(answer).encode()
  self.send_response(status)
  for k,v in headers.items():self.send_header(k,v)
  self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(raw)))
  self.send_header('Connection','close');self.end_headers();self.wfile.write(raw)
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
try:
 with tempfile.TemporaryDirectory(prefix='batbox-side-query-') as directory:
  result=subprocess.run([a.probe,f'http://127.0.0.1:{server.server_port}',directory],text=True,capture_output=True,timeout=30)
  if result.returncode:raise RuntimeError(f'{result.returncode}: {result.stderr}\n{result.stdout}')
  output=json.loads(result.stdout);assert output=={'memory':True,'heartbeat':True,'completionCount':1,'retryCases':5},output
 assert calls=={'memory':2,'permanent':1,'exhaust':3,'force':2,'malformed':1,'cancel':1},calls
 for case,body,headers in requests:
  if case!='memory':continue
  assert body['model']=='local' and body['max_tokens']==256
  assert body['output_config']['format']['type']=='json_schema'
  assert len(body['system'])==1 and 'selecting memories' in body['system'][0]['text']
  assert 'metadata' not in body and 'betas' not in body
  assert headers.get('batbox-beta')=='memory-beta'
 print('6 native HTTP retry and composed memory retrieval scenarios passed')
finally:
 server.shutdown();server.server_close();thread.join()
