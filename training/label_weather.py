# ภาพรวม: เติมป้ายฝนที่ขาดจาก Open-Meteo เป็นค่าประมาณ โดยรักษาป้ายของมนุษย์/เซ็นเซอร์ไว้ก่อน
"""Fill missing current-rain proxy labels, preserving all human labels. No future features."""
import argparse,json,os,urllib.parse,urllib.request
from datetime import datetime,timezone,timedelta
from pathlib import Path
import pandas as pd
from dotenv import load_dotenv

def main():
    p=argparse.ArgumentParser();p.add_argument('--input',required=True);p.add_argument('--out',required=True);p.add_argument('--rain-mm',type=float,default=.1);a=p.parse_args()
    load_dotenv(Path(__file__).with_name('.env'))
    lat=float(os.environ['LAT']);lon=float(os.environ['LON'])
    if not(-90<=lat<=90 and -180<=lon<=180):raise ValueError('Invalid coordinates')
    df=pd.read_csv(a.input);valid=df.epoch>0
    stamps=pd.to_datetime(df.loc[valid,'epoch'],unit='s',utc=True)
    if stamps.empty:raise ValueError('No synchronized timestamps; cannot label unsynced boots')
    start=stamps.min().date();end=stamps.max().date();cut=(datetime.now(timezone.utc)-timedelta(days=7)).date();weather={}
    for endpoint,first,last in [('https://archive-api.open-meteo.com/v1/archive',start,min(end,cut)),
                                ('https://api.open-meteo.com/v1/forecast',max(start,cut+timedelta(days=1)),end)]:
        if first>last:continue
        params=dict(latitude=lat,longitude=lon,start_date=first.isoformat(),end_date=last.isoformat(),hourly='precipitation',timezone='UTC')
        with urllib.request.urlopen(endpoint+'?'+urllib.parse.urlencode(params),timeout=60) as response:body=json.load(response)
        for t,rain in zip(body['hourly']['time'],body['hourly']['precipitation']):
            if rain is not None:weather[int(pd.Timestamp(t,tz='UTC').timestamp())]=int(rain>=a.rain_mm)
    df['label_source']='unknown'
    for i,r in df.iterrows():
        if r.rain_label in [0,1]:df.at[i,'label_source']='human'
        elif r.rain_now in [0,1]:df.at[i,'label_source']='sensor'
        elif r.epoch>0 and int(r.epoch)//3600*3600 in weather:
            df.at[i,'rain_now']=weather[int(r.epoch)//3600*3600];df.at[i,'label_source']='open-meteo-proxy'
    df.to_csv(a.out,index=False);print('Saved labels; weather API is a proxy, not observed backyard rain.')
if __name__=='__main__':main()
