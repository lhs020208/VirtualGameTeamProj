"""Fit a restricted two-compartment exchange coefficient. Never certifies real-process fidelity."""
import argparse,csv,json,math
from pathlib import Path

def fit(rows):
 train=[];holdout=[]
 for r in rows:
  t=float(r['time_s']);index=float(r['mix_index']);split=r['split']
  if not math.isfinite(t) or not math.isfinite(index) or t<=0 or not 0<=index<1 or split not in ('train','holdout'):raise ValueError('invalid calibration observation')
  (train if split=='train' else holdout).append((t,index))
 if len(train)<3 or len(holdout)<2:raise ValueError('independent train/holdout observations required')
 if {t for t,_ in train}&{t for t,_ in holdout}:raise ValueError('train/holdout time overlap')
 k=-sum(t*math.log1p(-i) for t,i in train)/(4*sum(t*t for t,_ in train))
 if not math.isfinite(k) or k<0:raise ValueError('invalid fit')
 errors=[abs(1-math.exp(-4*k*t)-i) for t,i in holdout]
 return {'exchange_s_inverse':k,'holdout_max_absolute_error':max(errors),'procedure_passed':max(errors)<=.01,'physical_accuracy_validated':False,'scope':'two equal-volume well-mixed compartments only; applying to a spatial process requires separate validation'}

def main():
 p=argparse.ArgumentParser();p.add_argument('input',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
 with a.input.open(encoding='utf-8-sig') as f:rows=list(csv.DictReader(f))
 result=fit(rows);result['data_source']=str(a.input);a.output.write_text(json.dumps(result,indent=2),encoding='utf-8')
if __name__=='__main__':main()
