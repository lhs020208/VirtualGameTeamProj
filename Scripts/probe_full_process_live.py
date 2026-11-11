import os,gc,json,types
from pathlib import Path
assert os.getpid()==17164,'Not the dedicated full-process test; refusing access'
matches=[f for f in gc.get_objects() if isinstance(f,types.FunctionType) and f.__name__=='tick' and f.__code__.co_filename.replace('\\','/').endswith('/run_full_process_20261110.py')]
assert len(matches)==1,len(matches)
g=matches[0].__globals__
assert str(g['OUT']).replace('\\','/').endswith('/FullProcess/20261110_182948')
p=g['p']
r={'pid':os.getpid(),'stage':g['stage'],'sim_s':p.get_editor_property('physical_seconds'),'count':p.get_editor_property('particle_count'),'data_directory':str(p.get_editor_property('data_directory')),'source_callback':matches[0].__code__.co_filename,'read_only':True}
(g['OUT']/'live_control_probe.json').write_text(json.dumps(r,indent=2))
