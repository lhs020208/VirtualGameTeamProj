import unittest,math
from calibrate_bulk_exchange import fit
class CalibrationTest(unittest.TestCase):
 def test_known_closure_and_holdout(self):
  data=[{'time_s':str(t),'mix_index':str(1-math.exp(-4*.08*t)),'split':'train' if t in [1,2,4] else 'holdout'} for t in [1,2,3,4,6,8]]
  r=fit(data);self.assertAlmostEqual(r['exchange_s_inverse'],.08);self.assertTrue(r['procedure_passed']);self.assertFalse(r['physical_accuracy_validated'])
  data[-1]['mix_index']='0';self.assertFalse(fit(data)['procedure_passed'])
  data[-1]['mix_index']='nan'
  with self.assertRaises(ValueError):fit(data)
 def test_insufficient_data(self):
  with self.assertRaises(ValueError):fit([])
if __name__=='__main__':unittest.main()
