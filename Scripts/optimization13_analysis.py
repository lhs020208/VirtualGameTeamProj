"""Offline acceptance helpers. Missing/invalid evidence never authorizes a ratio."""
import math

def finite(value):
    result=float(value)
    if not math.isfinite(result):raise ValueError('nonfinite evidence')
    return result

def distance(reference,candidate):
    if len(reference)!=21 or len(candidate)!=21:raise ValueError('expected common 21 samples')
    result={'mixing2':0.,'mixing4':0.,'com_relative':0.,'invalid2':0,'invalid4':0}
    for a,b in zip(reference,candidate):
        if int(a['sample'])!=int(b['sample']) or abs(finite(a['time_s'])-finite(b['time_s']))>1e-12:raise ValueError('sample time mismatch')
        result['com_relative']=max(result['com_relative'],abs(finite(a['com_z'])-finite(b['com_z']))/max(.01,abs(finite(a['com_z']))))
        for grid in (2,4):
            if a[f'valid{grid}']!='1' or b[f'valid{grid}']!='1':result[f'invalid{grid}']+=1;continue
            error=abs(finite(a[f'uniformity{grid}'])-finite(b[f'uniformity{grid}']))
            result[f'mixing{grid}']=max(result[f'mixing{grid}'],error)
    return result

def accepted(errors,limits):
    return (not errors['invalid2'] and not errors['invalid4'] and
            all(math.isfinite(errors[k]) and 0<=errors[k]<=limits[k] for k in ('mixing2','mixing4','com_relative')))

def maximum_prefix(results,ratios=(2,4,8)):
    maximum=1
    for q in ratios:
        if results.get(str(q),{}).get('eligible_in_test_domain') is not True:break
        maximum=q
    return maximum
