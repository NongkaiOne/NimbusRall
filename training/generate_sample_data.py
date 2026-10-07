import argparse,csv,math,random,time
from pathlib import Path
FIELDS='seq epoch temp_c hum_pct press_hpa lux p_rain line action flags rain_label rain_now uptime_s'.split()
def generate(path,days=35):
    rng=random.Random(42);n=days*48;start=int(time.time())-n*1800;Path(path).parent.mkdir(parents=True,exist_ok=True)
    with open(path,'w',newline='',encoding='utf-8') as f:
        writer=csv.writer(f);writer.writerow(FIELDS)
        for i in range(n):
            epoch=start+i*1800;hour=(epoch//3600+7)%24;wave=math.sin(i/18);wet=int(wave>.5 or rng.random()<.05)
            writer.writerow([i+1,epoch,round(29+3*math.sin(hour/24*2*math.pi),2),round(78+15*wave,2),
                round(1009-2*wave,1),round(max(0,math.sin((hour-6)/12*math.pi))*30000*(1-.7*wet),1),
                .8 if wet else .1,'RETRACTED' if wet else 'EXTENDED',1 if wet else 2,4,wet,wet,i*1800])
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--out',default='data/sample_device_log.csv');p.add_argument('--days',type=int,default=35)
    a=p.parse_args();generate(a.out,a.days)
