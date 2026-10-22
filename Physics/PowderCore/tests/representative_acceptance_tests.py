import copy,importlib.util,math,subprocess,sys,unittest
from pathlib import Path
root=Path(sys.argv[1]);exe=Path(sys.argv[2]);sys.argv=sys.argv[:1]
spec=importlib.util.spec_from_file_location('analysis13',root/'Scripts/optimization13_analysis.py');analysis=importlib.util.module_from_spec(spec);spec.loader.exec_module(analysis)
def sample():return [dict(sample=str(i),time_s=str(i*.006),com_z='.01',valid2='1',valid4='1',uniformity2='.5',uniformity4='.5') for i in range(21)]
class AcceptanceTests(unittest.TestCase):
 def test_missing_and_discontinuous_ratio_never_promotes(self):
  self.assertEqual(analysis.maximum_prefix({'8':{'eligible_in_test_domain':True}}),1)
  self.assertEqual(analysis.maximum_prefix({'2':{'eligible_in_test_domain':True},'4':{'eligible_in_test_domain':False},'8':{'eligible_in_test_domain':True}}),2)
 def test_invalid_samples_not_zero_error_pass(self):
  ref=sample();bad=copy.deepcopy(ref);bad[4]['valid4']='0';bad[4]['uniformity4']='';e=analysis.distance(ref,bad)
  self.assertEqual(e['invalid4'],1);self.assertFalse(analysis.accepted(e,dict(mixing2=.1,mixing4=.1,com_relative=.1)))
 def test_time_and_nonfinite_rejected(self):
  ref=sample();bad=copy.deepcopy(ref);bad[5]['time_s']='.031'
  with self.assertRaises(ValueError):analysis.distance(ref,bad)
  bad=copy.deepcopy(ref);bad[5]['uniformity2']='nan'
  with self.assertRaises(ValueError):analysis.distance(ref,bad)
 def test_tolerance_boundary_and_error(self):
  e=dict(mixing2=.1,mixing4=.1,com_relative=.1,invalid2=0,invalid4=0);limits=dict(mixing2=.1,mixing4=.1,com_relative=.1)
  self.assertTrue(analysis.accepted(e,limits));e['mixing2']=.10001;self.assertFalse(analysis.accepted(e,limits));e['mixing2']=math.nan;self.assertFalse(analysis.accepted(e,limits))
 def test_forbidden_scope_cli_rejected_before_output(self):
  import tempfile
  with tempfile.TemporaryDirectory() as temp:
   for args in [('aperture_small','8','2','11','1'),('mixing','3','2','11','1'),('mixing','1','0','11','1'),('mixing','1','2','11','0')]:
    target=Path(temp)/'output';r=subprocess.run([str(exe),*args,str(target)],capture_output=True,text=True)
    self.assertNotEqual(r.returncode,0);self.assertFalse(target.exists())
if __name__=='__main__':unittest.main()
