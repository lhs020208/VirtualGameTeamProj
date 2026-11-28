import assert from 'node:assert/strict';
import test from 'node:test';
import {decodeRecipe, emptyRecipe, grainKg} from '../public/recipes.mjs';
import {displayMetric} from '../public/protocol.mjs';

test('two individual sphere masses and sub-centigram display', ()=>{
 const d=grainKg(4), e=grainKg(2);
 assert.ok(Math.abs(d*1000-.04021239)<1e-8);
 assert.ok(Math.abs(e*1000-.005026549)<1e-9);
 assert.equal(d/e,8);
 assert.equal(displayMetric({unit:'kg', validity:'valid', value:e},{mass:true}).text,'<0.01');
});
test('count and mass basis survive serialization without rounding', ()=>{
 const r=emptyRecipe();
 r.name='입자 시험'; r.mode='a_target'; r.mixing={target_rpm:1,duration_s:1};
 r.version=5;r.particle_settings=Object.fromEntries([...'abcdef'].map(k=>['supply-'+k,{diameter_mm:k==='e'?2:4,target_basis:k==='a'?'count':'mass',target_count:k==='a'?1:0}]));
 r.materials[0].target_kg=grainKg(4);
 assert.deepEqual(decodeRecipe(JSON.stringify(r)),r);
 const raw=.000005030123456789;
 r.particle_settings['supply-a']={diameter_mm:2,target_basis:'mass',target_count:0};
 r.materials[0].target_kg=raw;assert.equal(decodeRecipe(JSON.stringify(r)).materials[0].target_kg,raw);
 r.particle_settings['supply-a'].target_count=.5;
 assert.throws(()=>decodeRecipe(JSON.stringify(r)));
});
test('v1–v4 keep file-defined diameters and original mass targets', ()=>{
 for(const version of [1,2,3,4]){
  const r=emptyRecipe();r.version=version;r.name='이전 설정';
  r.mixing={target_rpm:7,duration_s:.1};
  r.mode=version===3?'hopper_sequence':'a_target';
  r.materials[0].target_kg=.00004423363086549788;
  if(version===1){delete r.units.pressure;delete r.hopper_pressure_targets;}
  if(version===3){Object.assign(r.materials[0],{enabled:true,role:'material'});r.additive.hopper_id='';}
  if(version===4){r.hopper_pressure_targets.supplier_feed_pa=Object.fromEntries([...'abcdef'].map(k=>['supply-'+k,500]));}
  const read=decodeRecipe(JSON.stringify(r));
  assert.equal(read.particle_settings,undefined);
  assert.equal(read.materials[0].target_kg,r.materials[0].target_kg);
 }
});
