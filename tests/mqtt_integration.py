"""Local TLS + authenticated MQTT integration using a disposable Python broker.
Does not replace physical ESP32 testing or validate the Mosquitto Docker image.
"""
import asyncio,csv,json,logging,secrets,sys,tempfile,time,zlib
from pathlib import Path
from datetime import datetime,timedelta,timezone
from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives import hashes,serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from pwdlib import PasswordHash
from amqtt.broker import Broker
import paho.mqtt.client as mqtt
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'logger'))
from logger import Store,FIELDS

def credentials(path):
    key=rsa.generate_private_key(public_exponent=65537,key_size=2048)
    name=x509.Name([x509.NameAttribute(NameOID.COMMON_NAME,'localhost')]);now=datetime.now(timezone.utc)
    cert=x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(key.public_key()).serial_number(x509.random_serial_number()).not_valid_before(now-timedelta(minutes=1)).not_valid_after(now+timedelta(days=1)).add_extension(x509.SubjectAlternativeName([x509.DNSName('localhost')]),critical=False).add_extension(x509.BasicConstraints(ca=True,path_length=None),critical=True).sign(key,hashes.SHA256())
    (path/'cert.pem').write_bytes(cert.public_bytes(serialization.Encoding.PEM));(path/'key.pem').write_bytes(key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.TraditionalOpenSSL,serialization.NoEncryption()))
    password=secrets.token_urlsafe(20);(path/'passwd').write_text('test:'+PasswordHash.recommended().hash(password))
    return password

async def until(predicate,seconds=10):
    end=time.monotonic()+seconds
    while not predicate():
        if time.monotonic()>end:raise TimeoutError('MQTT expectation timed out')
        await asyncio.sleep(.02)

async def main():
    logging.basicConfig(level=logging.ERROR)
    with tempfile.TemporaryDirectory(prefix='smartline-') as tmp:
        path=Path(tmp);password=credentials(path);store=Store(path/'logs');clients=[]
        broker=Broker({'listeners':{'default':{'type':'tcp','bind':'127.0.0.1:18884','ssl':True,'certfile':str(path/'cert.pem'),'keyfile':str(path/'key.pem')}},
                       'plugins':{'amqtt.plugins.authentication.FileAuthPlugin':{'password_file':str(path/'passwd')}}})
        await broker.start();base='smartline/sl-test';messages=[];acks=[];body=','.join(FIELDS)+'\n'
        row='1,1760000000,30,80,1008,12000,0.2,RETRACTED,0,0,-1,-1,100\n';body+=row
        def client(name):
            c=mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,client_id=name);c.username_pw_set('test',password);c.tls_set(str(path/'cert.pem'));clients.append(c);return c
        web=client('test-web');device=client('test-device');log=client('test-logger')
        def on_web(c,u,m):messages.append((m.topic,m.payload.decode(),m.retain))
        web.on_message=on_web
        def on_log(c,u,m):
            if m.topic==base+'/log/record':store.record('sl-test',m.payload.decode())
        log.on_message=on_log
        def on_device(c,u,m):
            cmd=m.payload.decode();c.publish(base+'/ack',json.dumps({'cmd':cmd,'ok':not m.retain,'msg':'rejected retained'if m.retain else'ok'}),1)
            if cmd=='READLOG' and not m.retain:
                c.publish(base+'/log/file','#BEGIN rows=1 since=0',1)
                c.publish(base+'/log/file','#CHUNK 0\n'+body,1)
                c.publish(base+'/log/file','#CHUNK 0\n'+body,1)
                c.publish(base+'/log/file',f'#END rows=1 crc32={zlib.crc32(body.encode()):08X}',1)
        device.on_message=on_device
        try:
            for c in clients:c.connect_async('localhost',18884,30);c.loop_start()
            await until(lambda:all(c.is_connected()for c in clients))
            web.subscribe(base+'/#',1);log.subscribe(base+'/log/record',1);device.subscribe(base+'/cmd',1)
            await asyncio.sleep(.2)
            device.publish(base+'/availability','online',1,True);device.publish(base+'/status','{"line":"RETRACTED"}',0,True)
            device.publish(base+'/log/record',row,1);device.publish(base+'/log/record',row,1)
            await until(lambda:store.db.execute('SELECT COUNT(*) FROM records').fetchone()[0]==1)
            web.publish(base+'/cmd','READLOG',1,False)
            await until(lambda:any(t==base+'/log/file' and s.startswith('#END')for t,s,r in messages))
            chunks={int(s.split('\n',1)[0].split()[1]):s.split('\n',1)[1]for t,s,r in messages if t==base+'/log/file'and s.startswith('#CHUNK')}
            assembled=''.join(chunks[i]for i in range(len(chunks)));assert assembled==body
            assert any(json.loads(s)['ok']for t,s,r in messages if t==base+'/ack')
            late=client('late-web');late.on_message=on_web;late.connect_async('localhost',18884,30);late.loop_start();await until(late.is_connected);late.subscribe(base+'/availability',1)
            await until(lambda:any(t==base+'/availability'and r for t,s,r in messages))
            log.disconnect();await asyncio.to_thread(log.loop_stop);await asyncio.to_thread(log.reconnect);log.loop_start();await until(log.is_connected);log.subscribe(base+'/log/record',1);await asyncio.sleep(.1)
            device.publish(base+'/log/record',row,1);device.publish(base+'/log/record',row.replace('1,1760000000','2,1760001800'),1)
            await until(lambda:store.db.execute('SELECT COUNT(*) FROM records').fetchone()[0]==2)
            store.export()
            with (path/'logs/device_log.csv').open()as f:assert len(list(csv.DictReader(f)))==2
            print('PASS: TLS/auth, retained availability, command ack, READLOG chunks/CRC, duplicate delivery, reconnect and durable CSV.')
        finally:
            for c in clients:c.disconnect();await asyncio.to_thread(c.loop_stop)
            store.db.close();await broker.shutdown()

if __name__=='__main__':asyncio.run(main())
