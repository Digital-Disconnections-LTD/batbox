#!/usr/bin/env python3
"""Concurrent owner-loop model service through actual loopback HTTP and SSE."""
import argparse,json,subprocess,threading
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer

class Handler(BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    def log_message(self,*_):pass
    def do_POST(self):
        body=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        with self.server.lock:
            self.server.requests.append(body)
            self.server.auth.append(self.headers.get('Authorization','').startswith('Bearer '))
            self.server.active+=1
            self.server.max_active=max(self.server.max_active,self.server.active)
        self.server.barrier.wait(timeout=12)
        self.send_response(200)
        self.send_header('Content-Type','text/event-stream')
        self.send_header('Request-Id','local-stream')
        self.send_header('Connection','close')
        self.end_headers()
        events=[
          ('message_start',{'type':'message_start','message':{
            'type':'message','id':'msg-local','model':'batbox-model',
            'role':'assistant','content':[],'stop_reason':None,
            'usage':{'input_tokens':5,'output_tokens':0}}}),
          ('content_block_start',{'type':'content_block_start','index':0,
            'content_block':{'type':'text','text':''}}),
          ('content_block_delta',{'type':'content_block_delta','index':0,
            'delta':{'type':'text_delta','text':'hello'}}),
          ('content_block_stop',{'type':'content_block_stop','index':0}),
          ('message_delta',{'type':'message_delta',
            'delta':{'stop_reason':'end_turn'},'usage':{'output_tokens':3}}),
          ('message_stop',{'type':'message_stop'})]
        for event,value in events:
            self.wfile.write(f'event: {event}\ndata: {json.dumps(value)}\n\n'.encode())
        self.wfile.flush()
        with self.server.lock:self.server.active-=1

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--probe',required=True)
    args=parser.parse_args()
    server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
    server.lock=threading.Lock()
    server.barrier=threading.Barrier(4)
    server.requests=[];server.auth=[];server.active=0;server.max_active=0
    thread=threading.Thread(target=server.serve_forever,daemon=True)
    thread.start()
    try:
        base=f'http://127.0.0.1:{server.server_address[1]}'
        run=subprocess.run([args.probe,base],capture_output=True,text=True,
                           timeout=20)
        if run.returncode:
            raise AssertionError(run.stderr[:1200])
        output=json.loads(run.stdout)
        assert output['answers']==4,output
        assert output['activeRequests']==0,output
        assert output['schemaPrompts']==1,output
        assert output['totalCostUSD']==0,output
        assert server.max_active==4,server.max_active
        assert len(server.requests)==4 and all(server.auth)
        assert sum(bool(body.get('tools')) for body in server.requests)==1
        for body in server.requests:
            assert body['stream'] is True
            assert body['model']=='batbox-model'
            assert body['max_tokens']==50000
            assert body['messages'][0]['role']=='user'
            content=body['messages'][0]['content']
            assert isinstance(content,list) and content[0]['type']=='text'
            assert content[0]['text'].startswith('hello ')
        print('4 concurrent native model cursors completed over real HTTP/SSE')
    finally:
        server.shutdown();server.server_close();thread.join()

if __name__=='__main__':main()
