import csv,tempfile,unittest
from pathlib import Path
from audit_optimization24 import audit
ROOT=Path(__file__).resolve().parents[1]
class AuditTests(unittest.TestCase):
 def test_evidence_and_corruption(self):
  b=ROOT/'Docs/DEM/optimization19_controlled/windows.csv'; f=ROOT/'Docs/DEM/optimization24_final/windows.csv'
  self.assertTrue(audit(b,f)['numerical_comparison_passed'])
  with f.open(encoding='utf-8') as stream: rows=list(csv.DictReader(stream)); fields=list(rows[0])
  for field,value in [('inclusive_rate','nan'),('contacts','1'),('batch_steps','32'),('inclusive_rate','0.1'),('truncate','')]:
   bad=[dict(r) for r in rows]
   if field=='truncate':bad.pop()
   else:bad[0][field]=value
   with tempfile.TemporaryDirectory() as d:
    path=Path(d)/'bad.csv'
    with path.open('w',newline='') as s:
     writer=csv.DictWriter(s,fieldnames=fields);writer.writeheader();writer.writerows(bad)
    with self.assertRaises(AssertionError):audit(b,path)
if __name__=='__main__': unittest.main()
