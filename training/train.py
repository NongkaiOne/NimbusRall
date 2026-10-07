import argparse,json,warnings
from pathlib import Path
from datetime import datetime,timezone
import numpy as np
import pandas as pd
from sklearn.preprocessing import StandardScaler
from sklearn.linear_model import LogisticRegression
from sklearn.metrics import roc_auc_score,average_precision_score,confusion_matrix,precision_recall_curve
from sklearn.calibration import calibration_curve
from features import FEATURES,load_history,prepare,feature_frame,future_labels

def metrics(y,p,t):
    return dict(roc_auc=float(roc_auc_score(y,p)),pr_auc=float(average_precision_score(y,p)),
                confusion_matrix=confusion_matrix(y,p>=t).tolist(),
                reliability=[a.tolist() for a in calibration_curve(y,p,n_bins=10)])

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--history',required=True);ap.add_argument('--light')
    ap.add_argument('--device-log');ap.add_argument('--horizon',type=int,default=2)
    ap.add_argument('--rain-mm',type=float,default=.1);ap.add_argument('--out',default='training/model.json')
    ap.add_argument('--snapshot',action='store_true',help='Use only current sensor values; no time or history required')
    a=ap.parse_args()
    if a.horizon<1 or a.rain_mm<=0: ap.error('horizon and rain-mm must be positive')
    x,df=prepare(load_history(a.history,a.light))
    selected=FEATURES[:6] if a.snapshot else FEATURES
    x=x[selected]
    y=future_labels((df.precipitation>=a.rain_mm).astype(float),a.horizon)
    valid=y.notna() & np.isfinite(x).all(axis=1);x=x[valid];y=y[valid].astype(int)
    train=x.index<pd.Timestamp('2024-01-01')-pd.Timedelta(hours=a.horizon)
    val=(x.index>=pd.Timestamp('2024-01-01')) & (x.index<pd.Timestamp('2025-01-01')-pd.Timedelta(hours=a.horizon))
    test=x.index>=pd.Timestamp('2025-01-01')
    if not all([train.any(),val.any(),test.any()]): raise ValueError('Need train before 2024, validation 2024, test 2025+')
    scaler=StandardScaler().fit(x[train]);z=scaler.transform(x)
    model=LogisticRegression(class_weight='balanced',max_iter=2000,C=.5).fit(z[train],y[train])
    report={'split':'train < 2024; validation 2024; test >= 2025; horizon purged at boundaries',
            'rows':{'train':int(train.sum()),'validation':int(val.sum()),'test':int(test.sum())},
            'features':selected,'input_mode':'current' if a.snapshot else 'history','proxy_lux':'shortwave_radiation * 120; calibrate with BH1750',
            'device_warning':'No device calibration; reanalysis is not backyard sensor data.'}
    if a.device_log:
        d=pd.read_csv(a.device_log)
        if 'deviceId' in d and d.deviceId.nunique()>1: raise ValueError('Use logs from one device')
        d=d[d.epoch>0].copy();d.index=pd.to_datetime(d.epoch,unit='s',utc=True).dt.tz_convert('Asia/Bangkok')
        d=d.sort_index().rename(columns={'temp_c':'temp','hum_pct':'hum','press_hpa':'press'})
        d=d[~d.index.duplicated(keep='last')].resample('30min').last()
        rain=d.rain_label.where(d.rain_label.isin([0,1]),d.rain_now.where(d.rain_now.isin([0,1])))
        dy=future_labels(rain,a.horizon,.5);dx=feature_frame(d)[selected];ok=dy.notna() & np.isfinite(dx).all(axis=1)
        dx=dx[ok];dy=dy[ok]
        if len(dx)>100 and (dx.index[-1]-dx.index[0]).days>=28:
            split=dx.index[int(len(dx)*.8)];tr=dx.index<split-pd.Timedelta(hours=a.horizon);te=dx.index>=split
            dz=scaler.transform(dx)
            if dy[tr].nunique()==2 and dy[te].nunique()==2:
                report['device_before']=metrics(dy[te],model.predict_proba(dz[te])[:,1],.5)
                model.fit(np.concatenate([z[train],dz[tr]]),np.concatenate([y[train],dy[tr]]),
                          sample_weight=np.concatenate([np.ones(train.sum()),np.full(tr.sum(),3)]))
                report['device_after']=metrics(dy[te],model.predict_proba(dz[te])[:,1],.5)
                report['device_warning']='Weighted x3; final 20% device data held out with horizon purge.'
            else: warnings.warn('Single class device labels; no adaptation')
        else: warnings.warn('Need >=4 weeks of valid labeled device data; no adaptation')
    p=model.predict_proba(z[val])[:,1];precision,recall,thresholds=precision_recall_curve(y[val],p)
    candidates=np.where(recall[:-1]>=.85)[0];idx=candidates[np.argmax(thresholds[candidates])]
    threshold=float(thresholds[idx])
    report['validation_threshold']={'threshold':threshold,'recall':float(recall[idx]),'precision':float(precision[idx])}
    report['test']=metrics(y[test],model.predict_proba(z[test])[:,1],threshold)
    report['baseline_auc']=.75;report['meets_reference']=report['test']['roc_auc']>=.75
    out=Path(a.out);out.parent.mkdir(parents=True,exist_ok=True)
    data=dict(version='lr-'+datetime.now(timezone.utc).strftime('%Y%m%d%H%M%S'),horizon_h=a.horizon,
              rain_mm=a.rain_mm,features=FEATURES,mean=scaler.mean_.tolist(),scale=scaler.scale_.tolist(),
              coef=model.coef_[0].tolist(),intercept=float(model.intercept_[0]),threshold_suggest=threshold,
              trained_at=datetime.now(timezone.utc).isoformat(),uses_rain_now=False)
    if a.snapshot:
        # Keep the firmware vector format, with unused inputs provably inactive.
        data['input_mode']='current'
        data['version']='snapshot-'+datetime.now(timezone.utc).strftime('%Y%m%d%H%M%S')
        data['mean'] += [0.0]*(len(FEATURES)-len(selected))
        data['scale'] += [1.0]*(len(FEATURES)-len(selected))
        data['coef'] += [0.0]*(len(FEATURES)-len(selected))
    content=json.dumps(data,separators=(',',':'),allow_nan=False)
    if len(content.encode())>4096: raise ValueError('Model exceeds 4 KB')
    out.write_text(content,encoding='utf-8');dest=out.parent/'reports';dest.mkdir(exist_ok=True)
    (dest/'metrics.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    actual,pred=report['test']['reliability'];plt.plot(pred,actual,'o-');plt.plot([0,1],[0,1],'--')
    plt.xlabel('Predicted probability');plt.ylabel('Observed rain frequency');plt.savefig(dest/'reliability.png');plt.close()
    print(json.dumps(report,indent=2));print('Model bytes:',len(content))

if __name__=='__main__': main()
