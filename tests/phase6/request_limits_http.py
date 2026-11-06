#!/usr/bin/env python3
"""Endpoint request limits over loopback HTTP: no whole-prompt resend for a
local endpoint, the fallback where it is allowed, inactivity limits and 0,
and no resend of a request that timed out."""
import argparse,json,subprocess,threading,time
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer

def event(name,value):
    return f'event: {name}\ndata: {json.dumps(value)}\n\n'.encode()

START={'type':'message_start','message':{'type':'message','id':'msg-limits',
    'model':'limits-model','role':'assistant','content':[],'stop_reason':None,
    'usage':{'input_tokens':5,'output_tokens':0}}}
REST=[('content_block_start',{'type':'content_block_start','index':0,
        'content_block':{'type':'text','text':''}}),
      ('content_block_delta',{'type':'content_block_delta','index':0,
        'delta':{'type':'text_delta','text':'hello'}}),
      ('content_block_stop',{'type':'content_block_stop','index':0}),
      ('message_delta',{'type':'message_delta','delta':{'stop_reason':'end_turn'},
        'usage':{'output_tokens':3}}),
      ('message_stop',{'type':'message_stop'})]

class Handler(BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    def log_message(self,*_):pass
    def do_POST(self):
        body=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        with self.server.lock:
            self.server.requests.append({'path':self.path,'stream':body.get('stream')})
        if self.path.startswith('/slow-headers'):
            time.sleep(1.2)
            self.close_connection=True;return
        if not body.get('stream'):
            reply=json.dumps({'type':'message','id':'msg-plain','model':'limits-model',
                'role':'assistant','content':[{'type':'text','text':'plain reply'}],
                'stop_reason':'end_turn','usage':{'input_tokens':5,'output_tokens':2}}).encode()
            self.send_response(200)
            self.send_header('Content-Type','application/json')
            self.send_header('Content-Length',str(len(reply)))
            self.end_headers();self.wfile.write(reply);return
        self.send_response(200)
        self.send_header('Content-Type','text/event-stream')
        self.send_header('Connection','close')
        self.end_headers()
        self.wfile.write(event('message_start',START));self.wfile.flush()
        if self.path.startswith('/broken'):
            self.wfile.write(b'event: content_block_start\ndata: {not json\n\n')
            self.wfile.flush();return
        if self.path.startswith('/slow'):time.sleep(1.2)
        for name,value in REST:self.wfile.write(event(name,value))
        self.wfile.flush()

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--probe',required=True)
    args=parser.parse_args()
    server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
    server.lock=threading.Lock();server.requests=[]
    thread=threading.Thread(target=server.serve_forever,daemon=True)
    thread.start()
    try:
        base=f'http://127.0.0.1:{server.server_address[1]}'
        run=subprocess.run([args.probe,base],capture_output=True,text=True,timeout=60)
        if run.returncode:raise AssertionError(run.stderr[:1500])
        result=json.loads(run.stdout)
        def sent(path):return [r['stream'] for r in server.requests if r['path']==path]
        local=sent('/broken-local')
        assert local and all(local),('a local endpoint resent the prompt without streaming',local)
        assert result['brokenLocal']=='error',result
        remote=sent('/broken-remote')
        assert remote and remote[-1] is False and remote.count(False)==1,remote
        assert result['brokenRemote']=='plain reply',result
        assert sent('/slow-idle')==[True],('max_retries 0 still retried',sent('/slow-idle'))
        assert result['slowIdle']=='error',result
        assert sent('/slow-headers')==[True],('a timed-out request was resent',sent('/slow-headers'))
        assert result['slowHeaders']=='error',result
        assert sent('/slow-unlimited')==[True],sent('/slow-unlimited')
        assert result['slowUnlimited']=='hello',result
        print('local endpoints keep the stream, fallback where allowed, inactivity and 0 limits held')
    finally:
        server.shutdown();server.server_close();thread.join()

if __name__=='__main__':main()
