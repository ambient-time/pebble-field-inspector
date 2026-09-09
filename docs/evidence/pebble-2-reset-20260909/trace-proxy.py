import asyncio,struct,json,time
from pathlib import Path
OUT=Path('/tmp/signal-reset-firmware/wire.jsonl').open('a',buffering=1)
def log(**v):OUT.write(json.dumps(dict(at=time.time(),**v))+'\n')
class Parser:
 def __init__(self,direction):self.q=bytearray();self.p=bytearray();self.direction=direction
 def feed(self,b):
  self.q.extend(b)
  while len(self.q)>=8:
   if self.q[:2]!=b'\xfe\xed':del self.q[0];continue
   proto,n=struct.unpack('>HH',self.q[2:6])
   if len(self.q)<n+8:return
   assert self.q[6+n:8+n]==b'\xbe\xef'
   b=bytes(self.q[6:6+n]);del self.q[:n+8]
   if proto!=1:
    log(direction=self.direction,qemu_protocol=proto,length=n);continue
   self.p.extend(b)
   while len(self.p)>=4:
    n,ep=struct.unpack('>HH',self.p[:4])
    if len(self.p)<n+4:break
    body=bytes(self.p[4:4+n]);del self.p[:n+4]
    item=dict(direction=self.direction,endpoint=ep,length=n)
    if self.direction=='phone-to-watch':
     if ep==2003:item['reset_command']=body.hex()
     if ep in [45531,45787] and body:
      item['command']=body[0]
      if len(body)>3:item['database']=body[3]
     if ep==48879 and body:
      item['command']=body[0]
      if body[0]==1 and len(body)>5:item['object_type']=body[5]
    log(**item)
class SerialShim:
 def __init__(self):self.pending=bytearray()
 def feed(self,b):
  self.pending.extend(b);out=bytearray()
  while len(self.pending)>=8:
   assert self.pending[:2]==b'\xfe\xed'
   proto,n=struct.unpack('>HH',self.pending[2:6])
   if len(self.pending)<n+8:break
   frame=bytearray(self.pending[:n+8]);del self.pending[:n+8]
   payload=frame[6:6+n]
   if proto==1 and len(payload)>=5 and payload[2:4]==b'\x00\x10' and payload[4]==1:
    assert len(payload)==struct.unpack('>H',payload[:2])[0]+4
    assert payload[112:124]==bytes(12), 'Only modify blank emulator identity'
    frame[118:130]=b'SS-EMU-00001'
    log(event='synthetic-emulator-serial',serial='SS-EMU-00001')
   out.extend(frame)
  return bytes(out)
async def client(reader,writer):
 upstream,uw=await asyncio.open_connection('127.0.0.1',63270)
 log(event='connected')
 async def pipe(r,w,name):
  parser=Parser(name);shim=SerialShim() if name=='watch-to-phone' else None
  try:
   while b:=await r.read(8192):
    if shim:b=shim.feed(b)
    parser.feed(b);w.write(b);await w.drain()
  finally:w.close()
 await asyncio.gather(pipe(reader,uw,'phone-to-watch'),pipe(upstream,writer,'watch-to-phone'),return_exceptions=True)
 log(event='disconnected')
async def main():
 server=await asyncio.start_server(client,'127.0.0.1',63274)
 async with server:await server.serve_forever()
asyncio.run(main())
