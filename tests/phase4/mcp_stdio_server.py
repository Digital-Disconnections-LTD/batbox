#!/usr/bin/env python3
"""Deterministic JSONL MCP server used only by the native stdio fixture."""
import json,sys,threading,time

write_lock=threading.Lock()
cancelled=set()
def send(message):
    with write_lock:
        sys.stdout.write(json.dumps(message,separators=(',',':'))+'\n')
        sys.stdout.flush()
def result(request_id,value):send({'jsonrpc':'2.0','id':request_id,'result':value})
gadgets=[
    {'name':'echo','description':'Echo the given text',
     'inputSchema':{'type':'object','properties':{'text':{'type':'string'}},'required':['text']},
     'annotations':{'readOnlyHint':True,'title':'Echo'},
     '_meta':{'test/searchHint':'line\n  one','test/alwaysLoad':True}},
    {'name':'slow','description':'Slow tool','inputSchema':{'type':'object'}},
    {'name':'error','description':'Error tool','inputSchema':{'type':'object'}},
]
def delayed(request_id,params,delay):
    time.sleep(delay)
    if request_id in cancelled:return
    value=params.get('arguments',{}).get('text','')
    result(request_id,{'content':[{'type':'text','text':value}]})
for line in sys.stdin:
    if not line.strip():continue
    try:message=json.loads(line)
    except json.JSONDecodeError:continue
    method=message.get('method');request_id=message.get('id')
    if method=='initialize':
        result(request_id,{'protocolVersion':'2025-03-26','capabilities':{'tools':{}},
                           'serverInfo':{'name':'fixture','version':'1.0'}})
    elif method=='tools/list':result(request_id,{'tools':gadgets})
    elif method=='tools/call':
        params=message.get('params',{});name=params.get('name')
        token=params.get('_meta',{}).get('progressToken')
        if token is not None:
            send({'jsonrpc':'2.0','method':'notifications/progress',
                  'params':{'progressToken':token,'progress':1,'total':2,'message':'halfway'}})
        if name=='echo':
            text=params.get('arguments',{}).get('text','')
            result(request_id,{'content':[{'type':'text','text':text},
                {'type':'resource_link','name':'Result','uri':'file:///result'}],
                '_meta':{'origin':'fixture'},'structuredContent':{'echo':text}})
        elif name=='slow':
            delay=params.get('arguments',{}).get('delay',0.2)
            threading.Thread(target=delayed,args=(request_id,params,delay),daemon=True).start()
        elif name=='error':
            result(request_id,{'isError':True,'content':[{'type':'text','text':'tool failure'}],
                               '_meta':{'reason':'fixture'}})
        else:send({'jsonrpc':'2.0','id':request_id,
                   'error':{'code':-32601,'message':'unknown tool'}})
    elif method=='notifications/cancelled':
        cancelled.add(message.get('params',{}).get('requestId'))
    elif method=='notifications/initialized':pass
    elif method=='ping' and request_id is not None:result(request_id,{})
