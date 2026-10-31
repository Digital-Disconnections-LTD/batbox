#!/usr/bin/env python3
import argparse,json,subprocess,tempfile,shutil
from pathlib import Path
def run(probe,item):
  p=subprocess.run([probe],input=json.dumps(item),text=True,capture_output=True,check=True,timeout=10)
  return json.loads(p.stdout)
def expected(args,target):
  p=subprocess.run(['rg',*args,target],text=True,capture_output=True,check=False,timeout=10)
  return [line.rstrip('\r') for line in p.stdout.strip().split('\n') if line.rstrip('\r')]
def main():
  ap=argparse.ArgumentParser();ap.add_argument('--probe',required=True);a=ap.parse_args()
  with tempfile.TemporaryDirectory(prefix='batbox-rg-') as directory:
    root=Path(directory);(root/'nested').mkdir()
    (root/'a.txt').write_text('alpha\nbeta\n')
    (root/'nested'/'b.txt').write_text('alpha\n')
    (root/'ignored.md').write_text('alpha\n')
    checks=[
      (['--files','--glob','*.txt','--sort=modified','--no-ignore','--hidden'],str(root)),
      (['--hidden','--glob','!.git','--max-columns','500','-l','alpha'],str(root)),
      (['--hidden','--max-columns','500','-n','alpha'],str(root)),
      (['--hidden','--max-columns','500','-c','alpha'],str(root)),
      (['--hidden','--max-columns','500','-l','unlikely'],str(root)),
    ]
    for index,(args,target) in enumerate(checks):
      actual=run(a.probe,{'args':args,'target':target})
      want={'result':expected(args,target)}
      if sorted(actual.get('result',[]))!=sorted(want['result']):
        print('Mismatch',index,args);print('rg:',want);print('native:',actual);return 1
    for kind,item in [
      ('missing',{'command':'/does/not/exist/rg','target':str(root)}),
      ('timeout',{'command':shutil.which('sleep'),'target':'2','timeoutMs':20}),
      ('cancel',{'command':shutil.which('sleep'),'target':'2','timeoutMs':1000,'stop':True}),
    ]:
      actual=run(a.probe,item)
      if 'error' not in actual:
        print('Expected error',kind,actual);return 1
      if kind!='missing' and 'timed out' not in actual['error']:
        print('Wrong cancellation mapping',kind,actual);return 1
    print(f'{len(checks)+3} native ripgrep process cases passed');return 0
if __name__=='__main__':raise SystemExit(main())
