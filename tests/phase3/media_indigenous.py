#!/usr/bin/env python3
import argparse,struct,zlib,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--probe',required=True);a=p.parse_args()
def png(path,width,height):
 def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
 row=b'\x00'+bytes((i*3)%256 for i in range(width*3))
 raw=row*height
 path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))+
                  chunk(b'IDAT',zlib.compress(raw,6))+chunk(b'IEND',b''))
with tempfile.TemporaryDirectory() as tmp:
 root=Path(tmp);small=root/'small.png';large=root/'large.png';pdf=root/'two.pdf';ps=root/'two.ps';out=root/'out'
 png(small,10,10);png(large,3000,1500)
 ps.write_text('%!PS\n/Helvetica findfont 12 scalefont setfont\n72 720 moveto (Page one) show\nshowpage\n72 720 moveto (Page two) show\nshowpage\n')
 subprocess.run(['gs','-q','-dBATCH','-dNOPAUSE','-sDEVICE=pdfwrite','-sOutputFile='+str(pdf),str(ps)],check=True)
 result=subprocess.run([a.probe,str(small),str(large),str(pdf),str(out)],capture_output=True,text=True,timeout=35)
 assert result.returncode==0,(result.stdout,result.stderr)
 assert len(list((out/'pdf-fixture').glob('page-*.jpg')))==2
 print(result.stdout.strip())
