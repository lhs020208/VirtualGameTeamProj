"""Tasks 15-21: reproducible correctness and instrumented-core performance suite.
Requires built matching Unreal module for optional --unreal-editor validation.
"""
import argparse
import csv
import datetime as dt
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CMAKE = Path(r"C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe")
CRITERIA = {
    "pair_analytic_v1": {"position_m_lt": 1e-5, "velocity_m_s_lt": .001, "relative_energy_lt": .005, "mass_kg_lt": 1e-12, "momentum_kg_m_s_lt": 1e-12, "contacts_required": True},
    "floor_analytic_v1": {"position_m_lt": 2e-8, "velocity_m_s_lt": 2e-4, "relative_energy_lt": .005, "mass_kg_lt": 1e-12, "contact_every_step": True},
    "dense_conservation_v1": {"mass_kg_lt": 1e-10, "momentum_kg_m_s_lt": 1e-10, "overlap_radius_ratio_lt": .01, "contacts_required": True},
    "rig_freefall_v1": {"position_m_lt": 3e-11, "velocity_m_s_lt": 1e-12, "mass_kg_lt": 1e-10, "momentum_kg_m_s_lt": 1e-10, "contacts_zero": True},
    "existing_rig_drop_v1": {"max_overlap_m_lt": .005, "mass_kg_lt": 1e-12, "passed_tube_and_contacted_collector": True, "final_z_gt": 1.04},
    "atomic_rejection_v1": {"invalid_dt_or_hertz_overlap_rejected_without_state_change": True},
    "convergence": {"metric": "pair velocity / floor position", "coarse_over_fine_min": 1.8, "coarse_over_fine_max": 2.2},
    "reproducibility": "Exact 17-digit final state text and deterministic metrics across repeats on this build/machine. Contact history counts included, full history vectors not serialized.",
    "performance": "Diagnostic only; no fixed timing pass threshold. Stepping + metric collection; excludes initialization/rendering/file I/O."
}

def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024*1024), b''):
            h.update(block)
    return h.hexdigest()

def sources():
    paths = []
    for folder in (ROOT/'Physics/PowderCore', ROOT/'Source', ROOT/'Scripts'):
        paths += [p for p in folder.rglob('*') if p.is_file() and p.suffix in ('.cpp', '.hpp', '.h', '.cs', '.py', '.txt', '.boxes', '.tracking') and '__pycache__' not in p.parts]
    return {p.relative_to(ROOT).as_posix(): sha(p) for p in sorted(paths)}

def digest_files(values):
    return hashlib.sha256(json.dumps(values, sort_keys=True, separators=(',', ':')).encode()).hexdigest()

def save_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False)+'\n', encoding='utf-8')

def run(command, log, timeout=1800):
    command = [str(v) for v in command]
    start = time.perf_counter()
    print('RUN', log.name, flush=True)
    with log.open('w', encoding='utf-8') as f:
        result = subprocess.run(command, cwd=ROOT, stdout=f, stderr=subprocess.STDOUT,
                                timeout=timeout, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    return {'command': command, 'log': log.name, 'exit_code': result.returncode, 'wall_s': time.perf_counter()-start}

def read_matrix(path):
    with path.open(encoding='utf-8', newline='') as f:
        rows = list(csv.DictReader(f))
    if len(rows) != 42 or len({r['case'] for r in rows}) != 42:
        raise ValueError('benchmark incomplete: expected exactly 42 unique cases')
    for row in rows:
        for key, value in row.items():
            if key not in ('case', 'kind', 'criterion', 'error', 'final_state') and value and not math.isfinite(float(value)):
                raise ValueError(f'nonfinite metric {key}')
    return {r['case']: r for r in rows}

def analyze(matrices):
    cases = []
    for name in matrices[0]:
        rows = [m[name] for m in matrices]
        first = rows[0]
        stable_fields = [k for k in first if k not in ('wall_s', 'steps_per_s', 'particle_steps_per_s', 'wall_s_per_physical_s')]
        exact = all(all(row[k] == first[k] for k in stable_fields) for row in rows)
        seconds = [float(row['wall_s']) for row in rows]
        record = {k: v for k, v in first.items() if k != 'final_state'}
        record['state_sha256'] = hashlib.sha256(first['final_state'].encode()).hexdigest()
        record['exact_repeat'] = exact
        record['passed_all_repeats'] = all(row['passed'] == '1' for row in rows)
        record['wall_s_min'] = min(seconds);record['wall_s_median'] = statistics.median(seconds);record['wall_s_max'] = max(seconds)
        record['instrumented_steps_per_s_median'] = statistics.median(float(row['steps_per_s']) for row in rows)
        record['wall_s_per_physical_s_median'] = statistics.median(float(row['wall_s_per_physical_s']) for row in rows)
        cases.append(record)
    convergence = []
    for repeat, matrix in enumerate(matrices, 1):
        for kind, stiffness, metric in [('pair', k, 'velocity_error_m_s') for k in (1000,10000,100000)] + [('rig_floor', k, 'position_error_m') for k in (400000,4000000)]:
            values = [float(matrix[f'{kind}_k{stiffness}_s{n}'][metric]) for n in (200,400,800)]
            ratios = [values[0]/values[1], values[1]/values[2]]
            convergence.append({'repeat': repeat, 'case': f'{kind}_k{stiffness}', 'metric': metric, 'errors': values, 'halving_ratios': ratios, 'passed': all(1.8 <= q <= 2.2 for q in ratios)})
    return cases, convergence

def markdown(report):
    lines = ['# DEM baseline validation report', '', f"Status: **{'PASS' if report['passed'] else 'FAIL'}**", '',
             f"UTC: {report['metadata']['started_utc']}; repeats: {report['metadata']['repeats']}; core version: {report['metadata']['core_version']}.",
             f"Source SHA-256: `{report['metadata']['source_sha256']}`", '',
             '## Verification', '', f"- Unit suites: {len(report['unit_runs'])}; all passed: {all(x['passed'] for x in report['unit_runs'])}.",
             f"- Unreal suites: {len(report['engine_runs'])}; all passed: {bool(report['engine_runs']) and all(x['passed'] for x in report['engine_runs'])}.",
             f"- Matrix: {len(report['cases'])} cases per repeat; deterministic metrics and final state identical: {all(x['exact_repeat'] for x in report['cases'])}.",
             f"- All analytic convergence groups passed: {all(x['passed'] for x in report['convergence'])}.", '',
             '## Time-step convergence', '', '| Case | Metric | Error, 200 steps | Error, 400 steps | Error, 800 steps | Halving ratios |', '|---|---|---:|---:|---:|---|']
    for row in report['convergence']:
        if row['repeat'] == 1:
            e=row['errors'];q=row['halving_ratios'];lines.append(f"| {row['case']} | {row['metric']} | {e[0]:.6g} | {e[1]:.6g} | {e[2]:.6g} | {q[0]:.5g}, {q[1]:.5g} |")
    lines += ['', '## Instrumented core cost', '', 'The short matrices cover 2e-5 physical seconds, NOT complete deposition. Wall time includes stepping and metric collection. No rendering, initialization or file I/O is timed. Min/max describe repeat variation, not confidence intervals.', '', '| Case | Steps | Wall seconds median [min, max] | Steps/s | Wall s / physical s | Peak overlap/radius |', '|---|---:|---:|---:|---:|---:|']
    for row in report['cases']:
        if row['kind'] in ('dense_contact','rig_initial_freefall','rig_full_drop'):
            lines.append(f"| {row['case']} | {row['steps']} | {row['wall_s_median']:.6g} [{row['wall_s_min']:.6g}, {row['wall_s_max']:.6g}] | {row['instrumented_steps_per_s_median']:.6g} | {row['wall_s_per_physical_s_median']:.6g} | {float(row['max_overlap_radius_ratio']):.6g} |")
    lines += ['', '## Conservation / energy', '', '| Case | Max mass error kg | Max momentum error kg m/s | Max relative energy error |', '|---|---:|---:|---:|']
    for row in report['cases']:
        if row['kind']!='expected_rejection':
            lines.append(f"| {row['case']} | {float(row['max_mass_error_kg']):.6g} | {row['max_momentum_error_kg_m_s'] or 'N/A'} | {row['max_relative_energy_error'] or 'N/A'} |")
    lines += ['', '## Deliberately rejected conditions', '']
    for row in report['cases']:
        if row['kind']=='expected_rejection':lines.append(f"- {row['case']}: {row['error']}; unchanged state: {row['passed_all_repeats']}.")
    lines += ['', '## Scope and limitations', '',
              '- Pair tests: elastic frictionless 1 kg spheres, k=1e3..1e5 N/m, 200/400/800 steps over a fixed quarter-contact interval. Pair velocity is the first-order convergence metric; pair position has a special second-order cancellation in this initial condition.',
              '- Collector-face tests: actual 53-box fixture, d=0.04 m, rho=2500 kg/m3, initial compression 1e-5 m, k=4e5/4e6 N/m; floor position is the first-order metric.',
              '- Dense-contact matrix: 30/100/300 spheres with small initial overlaps, no gravity/damping/friction, k=4e5/4e6 N/m, dt=2e-7/1e-7 s. This isolates conservative pair-contact cost, not calibrated powder.',
              '- Rig initial matrix: normal seeded default placements in the 53-box fixture; NO contact during this very short window. Stiffness dependence here is not a contact-response validation.',
              '- Complete rig drop: one enlarged 0.2 m, 1 kg synthetic sphere for 1.5 physical seconds, including collector impact. This is not a 300-particle discharge experiment.',
              '- Mass and momentum are independent conservation checks only where applicable. Wall impulses/gravity make isolated momentum conservation inapplicable; wall rows leave it blank. Energy is checked only in undamped analytic contact cases.',
              '- Overlap is the maximum reported at solver force evaluations, not a continuous-time upper bound. Remaining untested dt/material/count combinations are not certified.',
              '- Timing depends on machine/background load and includes benchmark instrumentation. Short-window wall/physical ratios are diagnostics, not guaranteed full-experiment completion times.',
              '- Existing unit/Unreal tests cover additional Hertz, sliding/rolling friction, tracking, supply and output cases; the new stiffness/performance matrix is linear-contact only.',
              '- No calibration to physical powder measurements, no CFD, and no universal realtime guarantee.', '']
    return '\n'.join(lines)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--repeats', type=int, default=2)
    p.add_argument('--cmake', type=Path, default=DEFAULT_CMAKE)
    p.add_argument('--build', type=Path, default=ROOT/'Intermediate/PowderCore')
    p.add_argument('--unreal-editor', type=Path)
    args=p.parse_args()
    if args.repeats<2 or args.repeats>10:p.error('repeats must be 2..10')
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    try:
        provenance=sources()
        version=(ROOT/'Physics/PowderCore/include/powder/core.hpp').read_text().split('Version = "')[1].split('"')[0]
        meta={'schema':1,'suite':'DEM_validation_v7','started_utc':dt.datetime.now(dt.timezone.utc).isoformat(),'repeats':args.repeats,
              'core_version':version,'source_sha256':digest_files(provenance),'source_files':provenance,
              'platform':platform.platform(),'machine':platform.machine(),'processor':os.environ.get('PROCESSOR_IDENTIFIER',platform.processor()),
              'logical_cpus':os.cpu_count(),'python':sys.version,'configuration':'Release', 'timing_scope':'instrumented core, no engine rendering',
              'unreal_requested':bool(args.unreal_editor)}
        save_json(out/'metadata.json',meta);save_json(out/'criteria.json',CRITERIA)
        build=run([args.cmake,'--build',args.build,'--config','Release'],out/'build.log')
        if build['exit_code']:raise RuntimeError('build failed; see build.log')
        exe=args.build/'Release/powder_bench.exe';ctest=args.cmake.with_name('ctest.exe')
        meta['benchmark_sha256']=sha(exe);meta['cmake_cache_sha256']=sha(args.build/'CMakeCache.txt')
        if args.unreal_editor:
            module=ROOT/'Binaries/Win64/UnrealEditor-VirtualGameTeamProj.dll'
            runtime=list((ROOT/'Source').rglob('*.cpp'))+list((ROOT/'Source').rglob('*.h'))+list((ROOT/'Physics/PowderCore/src').glob('*.cpp'))+list((ROOT/'Physics/PowderCore/include').rglob('*.hpp'))
            if module.stat().st_mtime < max(f.stat().st_mtime for f in runtime):raise RuntimeError('Unreal module is stale; build Development Editor before this suite')
            meta['unreal_module_sha256']=sha(module);meta['unreal_editor']=str(args.unreal_editor)
        save_json(out/'metadata.json',meta)
        unit=[];engine=[];extra=[];matrices=[];bench=[]
        for i in range(1,args.repeats+1):
            u=run([ctest,'--test-dir',args.build,'-C','Release','--output-on-failure','--output-junit',out/f'unit_{i}.xml'],out/f'unit_{i}.log')
            tree=ET.parse(out/f'unit_{i}.xml').getroot();u['tests']=int(tree.attrib['tests']);u['passed']=u['exit_code']==0 and u['tests']>=19;unit.append(u)
            for script in ('cli_tests.py','motion_tests.py','contact_cli_tests.py'):
                x=run([sys.executable,'-B',ROOT/'Physics/PowderCore/tests'/script,args.build/'Release/powder_run.exe'],out/f'{script}_{i}.log');extra.append(x)
            extra.append(run([sys.executable,'-B',ROOT/'Scripts/test_dem_results.py',args.build/'result_fixture'],out/f'results_parser_{i}.log'))
            extra.append(run([sys.executable,'-B',ROOT/'Scripts/test_dem_mixing.py',args.build/'mixing_fixture'],out/f'mixing_parser_{i}.log'))
            if args.unreal_editor:
                folder=out/f'engine_{i}'
                e=run([args.unreal_editor,ROOT/'VirtualGameTeamProj.uproject','-unattended','-nullrhi','-nosound','-ini:Engine:[ConsoleVariables]:HomeScreen.EnableHomeScreen=0','-ExecCmds=Automation RunTests Powder.DEM.','-TestExit=Automation Test Queue Empty',f'-ReportExportPath={folder}',f'-abslog={out / f"engine_{i}_full.log"}'],out/f'engine_{i}.log')
                report=json.loads((folder/'index.json').read_text(encoding='utf-8-sig'));e['succeeded']=report['succeeded'];e['failed']=report['failed'];e['warnings']=report['succeededWithWarnings'];e['passed']=e['exit_code']==0 and e['succeeded']>=11 and e['failed']==0 and e['warnings']==0;engine.append(e)
            run_result=run([exe,ROOT/'Physics/PowderCore/fixtures/L_Main.boxes',out/f'matrix_{i}.csv'],out/f'matrix_{i}.log');bench.append(run_result)
            matrices.append(read_matrix(out/f'matrix_{i}.csv'))
            print(f'REPEAT {i} COMPLETE',flush=True)
        cases,convergence=analyze(matrices)
        unchanged=sources()==provenance
        passed=all(x['passed'] for x in unit+engine) and all(x['exit_code']==0 for x in extra+bench) and all(x['passed_all_repeats'] and x['exact_repeat'] for x in cases) and all(x['passed'] for x in convergence) and unchanged
        report={'schema':1,'passed':passed,'metadata':meta,'sources_unchanged':unchanged,'build':build,'unit_runs':unit,'engine_runs':engine,'additional_runs':extra,'benchmark_runs':bench,'cases':cases,'convergence':convergence,'engine_scope':'validated' if engine else 'not_run'}
        save_json(out/'report.json',report);(out/'report.md').write_text(markdown(report),encoding='utf-8')
        save_json(out/'complete.json',{'complete':True,'passed':passed})
        print(f'{"PASS" if passed else "FAIL"}: {out}',flush=True)
        return 0 if passed else 2
    except Exception as e:
        save_json(out/'failure.json',{'error':str(e),'complete':False})
        print(f'Validation incomplete: {e}',file=sys.stderr)
        return 1

if __name__=='__main__':sys.exit(main())
