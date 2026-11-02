#!/usr/bin/env python3
"""Fixture MCP server; exercises the transport without browsing or credentials."""
import json
import os
import sys
import time
import threading

PAGE_SCHEMA={'type':'object','required':['url'],'properties':{
    'url':{'type':'string'},
    'extraction_type':{'type':'string','default':'markdown','enum':['text','html','markdown']}}}
# Scrapling 0.4.7's tools; FAKE_SCRAPLING_TOOLS=unknown advertises no plain fetch tool.
TOOLS={'0.4.7':['open_session','close_session','list_sessions','get','bulk_get','fetch',
                'bulk_fetch','stealthy_fetch','bulk_stealthy_fetch','screenshot'],
       'unknown':['browser_open','browser_fetch']}[os.environ.get('FAKE_SCRAPLING_TOOLS','0.4.7')]
lock=threading.Lock()
cancelled=[]
pending_ping={}
def respond(identifier,result):
    response=json.dumps({'jsonrpc':'2.0','id':identifier,'result':result})+'\n'
    halfway=len(response)//2
    with lock:
        sys.stdout.write(response[:halfway]);sys.stdout.flush()
        sys.stdout.write(response[halfway:]);sys.stdout.flush()

def late(identifier):
    time.sleep(.2)
    respond(identifier,{'late':True})

for line in sys.stdin:
    request=json.loads(line)
    if request.get('method')=='notifications/cancelled':
        cancelled.append(request['params']['requestId']);continue
    if 'id' not in request:continue
    if 'method' not in request:
        if request['id'] in pending_ping:
            assert request['result']=={}
            respond(pending_ping.pop(request['id']),{'ping':'okay'})
        continue
    method=request['method']
    if method=='initialize':result={'protocolVersion':'2025-03-26','serverInfo':{'name':'fixture','version':'1'},'capabilities':{'tools':{}}}
    elif method=='tools/list':result={'tools':[{'name':name,'inputSchema':PAGE_SCHEMA if name in ('get','fetch','stealthy_fetch') else {'type':'object'}} for name in TOOLS]}
    elif method=='tools/call':
        name=request['params']['name']
        if name=='disconnect':sys.exit(0)
        if name=='hang':time.sleep(10)
        if name=='slow_reply':
            threading.Thread(target=late,args=(request['id'],),daemon=True).start();continue
        if name=='ping_then_reply':
            pending_ping['server-ping']=request['id']
            with lock:print(json.dumps({'jsonrpc':'2.0','id':'server-ping','method':'ping'}),flush=True)
            continue
        if name=='cancel_status':respond(request['id'],{'cancelled':cancelled,'pid':os.getpid()});continue
        if name=='after_delay':
            time.sleep(.35);respond(request['id'],{'after':True});continue
        result={'content':[{'type':'text','text':json.dumps(request['params'],ensure_ascii=False)}],
                'isError':name=='tool_error'}
    else:
        print(json.dumps({'jsonrpc':'2.0','id':request['id'],'error':{'code':-32601,'message':'missing method'}}),flush=True)
        continue
    # A notification and split response test framing and non-response handling.
    with lock:print(json.dumps({'jsonrpc':'2.0','method':'notifications/message','params':{'level':'info','data':'fixture progress'}}),flush=True)
    respond(request['id'],result)
