"""Board end-to-end tests; status COM12, data COM10, display COM13."""
import serial,time,json,struct,re
from pathlib import Path
log=bytearray()
def port(name,baud):
 p=serial.Serial(port=None,baudrate=baud,timeout=.05);p.dtr=False;p.rts=False;p.port=name;p.open();return p
p=port('COM13',115200);c=port('COM12',115200);u=port('COM10',921600)
def read(p,seconds):
 b=bytearray();end=time.monotonic()+seconds
 while time.monotonic()<end:b.extend(p.read(p.in_waiting or 1))
 log.extend(b);return bytes(b)
def command(cmd,seconds=1):
 p.write((cmd+'\n').encode());return read(p,seconds)
def objects(b):
 out=[]
 for line in b.splitlines():
  try:out.append(json.loads(line))
  except (ValueError,UnicodeDecodeError):pass
 return out
def stats():
 for _ in range(8):
  b=command('now stats',1.5);a=[x for x in objects(b) if 'rx_frames' in x]
  if a:return a[-1]
 return None
try:
 print(command('wifi mac').decode(errors='replace')[-600:],flush=True)
 print(command('now ui',3).decode(errors='replace')[-1200:],flush=True)
 print(command('now view curve').decode(errors='replace')[-400:],flush=True)
 base=None
 for _ in range(20):
  base=stats()
  if base and base['online']==1:break
  time.sleep(1)
 print('BASE',base,flush=True)
 assert base and base['online']==1
 raw=[]
 for i in range(1000):
  f=struct.pack('<III8fI',0xaaaaaaaa,i,i^15,*[i*.125+j for j in range(8)],0xeeeeeeee)
  raw.append(f);u.write(f);time.sleep(.01)
 time.sleep(2);a=stats();b=command('now stats');print(b.decode(errors='replace')[-2200:],flush=True)
 assert a['rx_frames']-base['rx_frames']==1000 and a['last_hex']==raw[-1].hex()
 crc=base['crc']
 for value in b''.join(raw):
  crc=~crc & 0xffffffff;crc^=value
  for _ in range(8):crc=(crc>>1)^(0x82f63b78 if crc&1 else 0)
  crc=~crc & 0xffffffff
 assert a['crc']==crc and a['rx_bytes']-base['rx_bytes']==48000
 print('FORWARD1000 PASS exact stream CRC32C',flush=True)
 u.reset_input_buffer();base_tx=a['tx_acked_frames'];expected=b''
 for i in range(20):
  f=struct.pack('<III8fI',0xaaaaaaaa,7000+i,15,*[i+.25+j for j in range(8)],0xeeeeeeee);expected+=f
  command('now send '+f.hex(),.12)
 received=read(u,2)
 assert received==expected,(len(received),len(expected))
 a=stats();assert a['tx_acked_frames']-base_tx==20
 print('REVERSE20 PASS exact UART bytes',flush=True)
 peer_data=read(c,2);peer=[x for x in objects(peer_data) if 'acked_bytes' in x][-1]
 print('PEER',peer,flush=True)
 assert peer['dropped_bytes']==0 and peer['uart_errors']==0 and peer['crc_errors']==0 and peer['fault']==0
 assert peer['queued_bytes']==0 and peer['pending_bytes']==0 and peer['crc']==a['crc'] and peer['ack_crc']==a['crc']
 command('now view rx');print(command('now stats').decode(errors='replace')[-1600:],flush=True)
 for _ in range(3):command('app ui home',2);command('now ui',2)
 a=stats();assert a and a['online']==1 and a['rx_frames']>=1000
 print('REOPEN3 PASS',flush=True)
 print(command('app ui home',8).decode(errors='replace')[-2400:],flush=True)
 print(command('app ui keymouse',6).decode(errors='replace')[-2400:],flush=True)
 print(command('bridge status',1).decode(errors='replace')[-1800:],flush=True)
 command('now ui',4);a=stats();assert a and a['online']==1
 print('ALL PASS',flush=True)
finally:
 for x in (p,c,u):x.close()
 Path('tmp/espnow-e2e.log').write_bytes(log)
