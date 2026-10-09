# ภาพรวม: ทดสอบ feature ความตรงกันของโมเดล Python/C++ และการเก็บ log ฝั่งคอม
import csv,importlib.util,json,random,subprocess,sys,tempfile,unittest
from pathlib import Path
import numpy as np
import pandas as pd
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'training'));sys.path.insert(0,str(ROOT/'logger'))
from features import vector,prepare,load_history,future_labels
from logger import Store

class FeaturesTest(unittest.TestCase):
    def test_snapshot_ignores_time_and_history(self):
        model=json.loads((ROOT/'training/model.json').read_text())
        self.assertEqual(model['input_mode'],'current')
        np.testing.assert_array_equal(model['coef'][6:],np.zeros(10))
        a=np.array(vector(30,80,1008,200,0,1,0,0,0,0,.5,0))
        b=np.array(vector(30,80,1008,200,23,365,-9,9,30,-20,2,10))
        def score(x):
            return model['intercept']+np.sum(np.array(model['coef'])*(x-model['mean'])/model['scale'])
        self.assertEqual(score(a),score(b))
    def test_future_label_excludes_present_and_unknown_tail(self):
        y=future_labels(pd.Series([1.,0.,0.,1.,0.]),2)
        self.assertEqual(y.iloc[0],0);self.assertEqual(y.iloc[1],1);self.assertTrue(y.iloc[-2:].isna().all())
    def test_no_future_feature_leakage(self):
        df=load_history(ROOT/'data/history_hourly.csv').iloc[:300].copy()
        a,_=prepare(df);df.loc[200:,'temperature_2m']=60;df.loc[200:,'shortwave_radiation']=99999
        b,_=prepare(df);np.testing.assert_allclose(a.iloc[:200],b.iloc[:200])
    def test_native_cpp_parity(self):
        exe=ROOT/'.test-build/parity.exe'
        self.assertTrue(exe.exists(),'Compile tests/parity.cpp first; see docs/testing.md')
        model=json.loads((ROOT/'training/model.json').read_text())
        rng=random.Random(7);cases=[[rng.uniform(15,40),rng.uniform(30,100),rng.uniform(990,1020),rng.uniform(0,100000),rng.uniform(0,24),rng.randint(1,366),rng.uniform(-4,4),rng.uniform(-5,5),rng.uniform(-20,20),rng.uniform(-5,5),rng.uniform(0,2),rng.uniform(0,3)]for _ in range(200)]
        cases.extend([[30,95,1005,500,12,100,0,0,0,0,.1,1],[30,85,1005,500,12,100,0,-3,0,0,.5,1],[30,50,1010,50000,12,100,0,0,0,0,1,1]])
        text='\n'.join(' '.join(map(str,model[k]))for k in ['mean','scale','coef'])+'\n'+str(model['intercept'])+'\n'
        text+='\n'.join(' '.join(map(str,c))for c in cases)+'\n'
        result=subprocess.run([str(exe)],input=text,text=True,capture_output=True,check=True)
        maximum=0
        for inp,line in zip(cases,result.stdout.splitlines(),strict=True):
            out=np.array(list(map(float,line.split())));x=np.array(vector(*inp));np.testing.assert_allclose(out[:16],x,atol=.0001,rtol=.0001)
            z=model['intercept']+np.sum(np.array(model['coef'])*(x-model['mean'])/model['scale'])
            p=1/(1+np.exp(-np.clip(z,-40,40)));maximum=max(maximum,abs(p-out[16]))
            fallback=.8 if (x[7]<=-2 and x[1]>=80)or x[6]<=-1.5 or(x[1]>=92 and x[10]<.25)else .1
            self.assertAlmostEqual(out[17],fallback,places=6)
        self.assertLess(maximum,.01);print('Maximum native C++ / Python probability error:',maximum)

class LoggerTest(unittest.TestCase):
    def test_dedup_restart_and_label(self):
        row='1,1760000000,30,80,1008,12000,0.2,RETRACTED,0,0,-1,-1,100\n'
        with tempfile.TemporaryDirectory() as tmp:
            s=Store(tmp);self.assertTrue(s.record('sl-one',row));self.assertFalse(s.record('sl-one',row));s.export();s.db.close()
            s=Store(tmp);self.assertFalse(s.record('sl-one',row));s.record('sl-two',row)
            s.record('sl-one',row.replace('0,0,-1,-1,100','0,0,1,-1,100'));s.record('sl-one',row);s.export()
            with open(Path(tmp)/'device_log.csv',newline='')as f:rows=list(csv.DictReader(f))
            self.assertEqual(len(rows),2);self.assertEqual(rows[0]['rain_label'],'1');s.db.close()
    def test_invalid_record(self):
        with tempfile.TemporaryDirectory()as tmp:
            s=Store(tmp)
            with self.assertRaises(ValueError):s.record('sl-test','garbage')
            s.db.close()

if __name__=='__main__':unittest.main()
