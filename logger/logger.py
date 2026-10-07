"""Durable MQTT ingestion. SQLite is authoritative; CSV is an atomic snapshot."""
import csv,io,json,logging,os,sqlite3,threading,time
from pathlib import Path
from dotenv import load_dotenv
import paho.mqtt.client as mqtt

FIELDS='seq epoch temp_c hum_pct press_hpa lux p_rain line action flags rain_label rain_now uptime_s'.split()

def parse_record(payload):
    values=next(csv.reader(io.StringIO(payload.strip())))
    if len(values)!=len(FIELDS): raise ValueError('Invalid CSV column count')
    r=dict(zip(FIELDS,values))
    for k in ['seq','epoch','action','flags','rain_label','rain_now','uptime_s']:r[k]=int(r[k])
    if r['seq']<1 or r['rain_label'] not in [-1,0,1] or r['rain_now'] not in [-1,0,1]:raise ValueError('Invalid sequence/label')
    for k in ['temp_c','hum_pct','press_hpa','lux','p_rain']:
        if r[k]!='':
            import math
            r[k]=float(r[k])
            if not math.isfinite(r[k]):raise ValueError('Non-finite sensor value')
    return r

class Store:
    def __init__(self,directory):
        self.directory=Path(directory);self.directory.mkdir(parents=True,exist_ok=True)
        self.lock=threading.RLock();self.dirty=True
        self.db=sqlite3.connect(self.directory/'device_log.sqlite',check_same_thread=False)
        self.db.execute('PRAGMA journal_mode=WAL');self.db.execute('PRAGMA synchronous=FULL')
        self.db.execute('CREATE TABLE IF NOT EXISTS records(device TEXT, seq INTEGER, body TEXT, PRIMARY KEY(device,seq))')
        self.db.execute('CREATE TABLE IF NOT EXISTS status(device TEXT PRIMARY KEY, body TEXT, received INTEGER)');self.db.commit()
    def record(self,device,payload):
        r=parse_record(payload)
        with self.lock,self.db:
            old=self.db.execute('SELECT body FROM records WHERE device=? AND seq=?',(device,r['seq'])).fetchone()
            if old:
                prev=json.loads(old[0])
                # A delayed QoS duplicate must not erase a human label.
                if r['rain_label']==-1 and prev['rain_label']!=-1:r['rain_label']=prev['rain_label']
            body=json.dumps(r,separators=(',',':'))
            if old and old[0]==body:return False
            self.db.execute('INSERT OR REPLACE INTO records VALUES(?,?,?)',(device,r['seq'],body));self.dirty=True
        return True
    def status(self,device,payload):
        json.loads(payload)
        with self.lock,self.db:self.db.execute('INSERT OR REPLACE INTO status VALUES(?,?,?)',(device,payload,int(time.time())))
    def export(self):
        with self.lock:
            if not self.dirty:return
            tmp=self.directory/'device_log.csv.tmp'
            with tmp.open('w',newline='',encoding='utf-8') as f:
                writer=csv.DictWriter(f,fieldnames=['deviceId']+FIELDS);writer.writeheader()
                for device,body in self.db.execute('SELECT device,body FROM records ORDER BY device,seq'):
                    writer.writerow({'deviceId':device,**json.loads(body)})
                f.flush();os.fsync(f.fileno())
            os.replace(tmp,self.directory/'device_log.csv');self.dirty=False

def main():
    load_dotenv(Path(__file__).with_name('.env'));logging.basicConfig(level=logging.INFO,format='%(asctime)s %(message)s')
    host=os.environ.get('MQTT_HOST');user=os.environ.get('MQTT_USERNAME');password=os.environ.get('MQTT_PASSWORD')
    if not all([host,user,password]):raise SystemExit('Set MQTT_HOST, MQTT_USERNAME, MQTT_PASSWORD in logger/.env')
    store=Store(os.getenv('DATA_DIR','data'));prefix=os.getenv('MQTT_PREFIX','smartline').strip('/')
    client=mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,client_id='smartline-logger',clean_session=False,manual_ack=True)
    client.username_pw_set(user,password);client.tls_set(ca_certs=os.getenv('MQTT_CA') or None);client.reconnect_delay_set(2,60)
    def on_connect(c,u,flags,reason,properties):
        if reason.is_failure:logging.error('Connection refused: %s',reason);return
        c.subscribe([(prefix+'/+/log/record',1),(prefix+'/+/status',1)])
    def on_message(c,u,msg):
        try:
            parts=msg.topic[len(prefix)+1:].split('/');device=parts[0];payload=msg.payload.decode('utf-8')
            if parts[1:]==['log','record']:store.record(device,payload)
            elif parts[1:]==['status']:store.status(device,payload)
            if msg.qos:c.ack(msg.mid,msg.qos)
        except (ValueError,UnicodeError,KeyError):
            logging.exception('Invalid record rejected')
            if msg.qos:c.ack(msg.mid,msg.qos)
        except sqlite3.Error:logging.exception('Durable write failed; message not acknowledged')
    client.on_connect=on_connect;client.on_message=on_message
    client.connect_async(host,int(os.getenv('MQTT_PORT','8883')),45);client.loop_start()
    try:
        while True:
            try:store.export()
            except OSError:logging.exception('CSV export failed; data remains in SQLite')
            time.sleep(1)
    except KeyboardInterrupt:pass
    finally:client.disconnect();client.loop_stop();store.export();store.db.close()

if __name__=='__main__':main()
