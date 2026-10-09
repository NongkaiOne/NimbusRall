# ภาพรวม: เตรียมข้อมูลและสร้าง feature แบบเดียวกับ ESP32 โดยใช้ข้อมูลปัจจุบัน/อดีตเป็นอินพุต
"""Causal feature definitions shared with include/features.h."""
import math
import numpy as np
import pandas as pd

FEATURES = ['temp', 'hum', 'press', 'lux_log', 'dew_point', 'temp_minus_dew',
            'd_press_1h', 'd_press_3h', 'd_hum_1h', 'd_temp_1h', 'lux_ratio',
            'hour_sin', 'hour_cos', 'doy_sin', 'doy_cos', 'press_std_3h']

# สูตรหนึ่งแถว ต้องตรงกับ featureVector ใน include/features.h
def vector(temp, hum, press, lux, hour, doy, dp1=0, dp3=0, dh1=0, dt1=0, ratio=.5, press_std=0):
    g = math.log(max(.01, hum)/100) + 17.62*temp/(243.12+temp)
    dew = 243.12*g/(17.62-g)
    return [temp, hum, press, math.log10(1+max(0,lux)), dew, temp-dew,
            dp1, dp3, dh1, dt1, ratio, math.sin(2*math.pi*hour/24),
            math.cos(2*math.pi*hour/24), math.sin(2*math.pi*doy/365.25),
            math.cos(2*math.pi*doy/365.25), press_std]

def load_history(path, light=None):
    with open(path, encoding='utf-8-sig') as f:
        skip = 3 if f.readline().startswith('latitude,') else 0
    df = pd.read_csv(path, skiprows=skip)
    df.columns = [c.split(' (')[0] for c in df.columns]
    if light:
        other = load_history(light)
        df = df.drop(columns=['shortwave_radiation'], errors='ignore').merge(
            other[['time','shortwave_radiation']], on='time', validate='one_to_one')
    return df

# ตรวจข้อมูลรายชั่วโมงต่อเนื่อง และแปลงรังสีแสงเป็น lux โดยประมาณสำหรับฝึก
def prepare(df):
    df = df.copy(); df['time'] = pd.to_datetime(df['time'])
    df = df.sort_values('time').set_index('time')
    if df.index.has_duplicates: raise ValueError('Duplicate timestamps')
    if not (df.index.to_series().diff().dropna() == pd.Timedelta(hours=1)).all():
        raise ValueError('History must be continuous hourly data')
    df = df.rename(columns={'temperature_2m':'temp','relative_humidity_2m':'hum','surface_pressure':'press'})
    df['lux'] = df.shortwave_radiation.clip(lower=0)*120
    return feature_frame(df), df

# สร้าง feature จากปัจจุบัน/อดีต ไม่ใช้ฝนในอนาคตเป็นอินพุต
def feature_frame(df):
    x = pd.DataFrame(index=df.index)
    for k in ['temp','hum','press']: x[k]=df[k]
    x['lux_log'] = np.log10(1+df.lux.clip(lower=0))
    g = np.log(df.hum.clip(lower=.01)/100)+17.62*df.temp/(243.12+df.temp)
    x['dew_point']=243.12*g/(17.62-g); x['temp_minus_dew']=df.temp-x.dew_point
    step = (df.index[1]-df.index[0]).total_seconds(); n = int(3600/step)
    for name,col,lag in [('d_press_1h','press',n),('d_press_3h','press',3*n),('d_hum_1h','hum',n),('d_temp_1h','temp',n)]:
        x[name]=(df[col]-df[col].shift(lag)).fillna(0)
    peaks=df.lux.groupby(df.index.hour,group_keys=False).transform(lambda s:s.rolling('7D',closed='left',min_periods=1).max())
    x['lux_ratio']=(df.lux/peaks.replace(0,np.nan)).clip(upper=2).fillna(.5)
    hours=df.index.hour+df.index.minute/60
    x['hour_sin']=np.sin(2*np.pi*hours/24); x['hour_cos']=np.cos(2*np.pi*hours/24)
    x['doy_sin']=np.sin(2*np.pi*df.index.dayofyear/365.25);x['doy_cos']=np.cos(2*np.pi*df.index.dayofyear/365.25)
    x['press_std_3h']=df.press.rolling('3h',closed='both',min_periods=1).std(ddof=0)
    return x[FEATURES]

# ใช้ฝนในอนาคตเป็นคำตอบสำหรับฝึก แยกออกจาก feature
def future_labels(rain,horizon,step_hours=1):
    n=int(horizon/step_hours)
    if n<1 or n*step_hours!=horizon: raise ValueError('Invalid horizon')
    future=pd.concat([rain.shift(-i) for i in range(1,n+1)],axis=1)
    return future.max(axis=1).where(future.notna().all(axis=1))
