// Recipe configuration only. The native service revalidates the complete JSON authoritatively.
export const batchKinds=['recipe_list','recipe_load','recipe_save','recipe_apply','recipe_charge','batch_start','batch_check','batch_resume','batch_abandon'];
export const readKinds=['recipe_list','recipe_load'];
export const isBatch=k=>batchKinds.includes(k);
export const emptyRecipe=()=>({schema:'virtual-game-team.batch-recipe',version:2,name:'',mode:'a_full',units:{mass:'kg',time:'simulation_s',speed:'rpm',pressure:'Pa_g'},feed_order:['supply-a'],materials:[{hopper_id:'supply-a',display_name:'원료 A',target_kg:0,tolerance_kg:0}],mixing:{target_rpm:0,duration_s:0},additive:{enabled:false,hopper_id:'supply-c',scheduled:false,delay_s:0},hopper_pressure_targets:{supply_to_weighing:{'supply-common':500,weigher:-500,receiver:0},weighing_to_receiver:{'supply-common':0,weigher:500,receiver:-1000}}});
const exact=(o,keys)=>o&&typeof o==='object'&&!Array.isArray(o)&&Object.keys(o).length===keys.length&&keys.every(k=>Object.hasOwn(o,k));
const num=v=>typeof v==='number'&&Number.isFinite(v),name=v=>typeof v==='string'&&v.trim()===v&&v.length>0&&v.length<=80&&!/[\x00-\x1f\x7f]/.test(v);
export function grainKg(mm){
 const radius=Math.fround(mm*.0005);
 return (4/3)*Math.PI*1200*radius*radius*radius;
}
export const validDiameter=mm=>Number.isFinite(mm)&&mm>=2&&mm<20&&Math.fround(mm*.0005)<Math.fround(.01);
export function decodeRecipe(text){
 if(typeof text!=='string'||text.length>8192)throw Error('레시피 크기·형식 오류');let r;try{r=JSON.parse(text);}catch{throw Error('손상된 레시피 JSON');}
 let particles;
 if(r?.version===5){
  particles=r.particle_settings;
  const ids=[...'abcdef'].map(k=>'supply-'+k);
  if(!exact(particles,ids))throw Error('A~F 입자 설정 누락');
  for(const id of ids){
   const p=particles[id];
   if(!exact(p,['diameter_mm','target_basis','target_count'])||!validDiameter(p.diameter_mm)||!Number.isSafeInteger(p.target_count)||p.target_count<0||p.target_count>1000000||p.target_basis!==(p.target_count?'count':'mass'))throw Error(id+' 지름 또는 목표 개수 오류');
   if(p.target_count){
    const m=Array.isArray(r.materials)&&r.materials.find(m=>m?.hopper_id===id);
    if(!m||m.enabled===false||r.mode==='a_full'||m.tolerance_kg!==0||Math.abs(m.target_kg-p.target_count*grainKg(p.diameter_mm))>1e-13*Math.max(1,m.target_kg))throw Error(id+' 개수 기준과 목표 질량 불일치');
   }
  }
  delete r.particle_settings;
  r.version=r.hopper_pressure_targets?.supplier_feed_pa?4:r.mode==='hopper_sequence'?3:2;
 }
 const fields=['schema','version','name','mode','units','feed_order','materials','mixing','additive'];
 if(![1,2,3,4].includes(r?.version))throw Error('지원하지 않는 레시피 버전');if(r.version>=2)fields.push('hopper_pressure_targets');
 if(!exact(r,fields)||r.schema!=='virtual-game-team.batch-recipe')throw Error('레시피 규격·필드 오류');
 if(!exact(r.units,r.version>=2?['mass','time','speed','pressure']:['mass','time','speed'])||r.units.mass!=='kg'||r.units.time!=='simulation_s'||r.units.speed!=='rpm'||r.version>=2&&r.units.pressure!=='Pa_g')throw Error('레시피 단위 오류');
 if(!name(r.name))throw Error('이름: 공백·제어문자 없이 1~80자');
 if(r.mode==='hopper_sequence'?r.version<3:r.version===3||!['a_full','a_target','sequential_ab'].includes(r.mode))throw Error('배치 모드: 지원하지 않는 호퍼 조합');
 if(!exact(r.additive,['enabled','hopper_id','scheduled','delay_s'])||typeof r.additive.enabled!=='boolean'||typeof r.additive.scheduled!=='boolean'||r.mode!=='hopper_sequence'&&r.additive.hopper_id!=='supply-c'||!num(r.additive.delay_s))throw Error('C 설정 형식 오류');
 const c=r.additive,ab=r.mode==='sequential_ab',order=['supply-a',...(ab?['supply-b']:[]),...(c.enabled?['supply-c']:[])];
 if(r.mode==='hopper_sequence'){
  if(!Array.isArray(r.materials)||r.materials.length>6)throw Error('공급호퍼 목록 오류');
  const seen=new Set(),active=[];
  for(const m of r.materials){
   if(!exact(m,['hopper_id','enabled','role','display_name','target_kg','tolerance_kg'])||!/^supply-[a-f]$/.test(m.hopper_id)||seen.has(m.hopper_id))throw Error('공급호퍼 ID 중복 또는 형식 오류');
   seen.add(m.hopper_id);
   if(typeof m.enabled!=='boolean'||!['material','additive'].includes(m.role)||!name(m.display_name)||!num(m.target_kg)||!num(m.tolerance_kg)||m.target_kg<0||m.tolerance_kg<0||m.enabled&&m.tolerance_kg>=m.target_kg)throw Error('원료 이름·역할·목표량·공차 오류');
   if(m.enabled)active.push(m);
  }
  const additives=active.filter(m=>m.role==='additive');
  if(!active.some(m=>m.role==='material')||additives.length>1||additives.length&&active.at(-1)!==additives[0])throw Error('일반 원료를 먼저 지정하고 첨가제는 마지막에 한 종류만 지정하세요.');
  if(c.enabled!==!!additives.length||c.hopper_id!==(additives[0]?.hopper_id??'')||!c.enabled&&c.scheduled||!c.scheduled&&c.delay_s!==0||JSON.stringify(active.map(m=>m.hopper_id))!==JSON.stringify(r.feed_order))throw Error('투입 순서 또는 첨가제 예약 설정 불일치');
 }else{
 if(c.enabled&&!ab||!c.enabled&&(c.scheduled||c.delay_s!==0)||!c.scheduled&&c.delay_s!==0)throw Error('C 사용·예약 설정 오류');
 if(JSON.stringify(r.feed_order)!==JSON.stringify(order)||!Array.isArray(r.materials)||r.materials.length!==order.length)throw Error('지원 순서: A → B → C. 임의 재정렬 불가');
 for(let i=0;i<order.length;i++){const m=r.materials[i],full=r.mode==='a_full';if(!exact(m,['hopper_id','display_name','target_kg','tolerance_kg'])||m.hopper_id!==order[i]||!name(m.display_name))throw Error(`${order[i]} 원료 표시명·호퍼 ID 오류`);if(!num(m.target_kg)||!num(m.tolerance_kg)||(full?m.target_kg!==0||m.tolerance_kg!==0:m.target_kg<=0||m.tolerance_kg<0||m.tolerance_kg>=m.target_kg))throw Error(`${order[i]} 목표량·계량 공차 오류`);}
 }
 if(!exact(r.mixing,['target_rpm','duration_s'])||!num(r.mixing.target_rpm)||r.mixing.target_rpm<=0||r.mixing.target_rpm>30||!num(r.mixing.duration_s)||r.mixing.duration_s<=0)throw Error('혼합: RPM은 0 초과 30 이하, 시간은 양수');
 if(c.scheduled&&(c.delay_s<0||c.delay_s>=r.mixing.duration_s))throw Error('C 예약 시각: 0 이상, 혼합 시간 미만');
 if(r.version===1){r.version=2;r.units.pressure='Pa_g';r.hopper_pressure_targets=emptyRecipe().hopper_pressure_targets;}
 const p=r.hopper_pressure_targets,f=p?.supply_to_weighing,t=p?.weighing_to_receiver,k=['supply-common','weigher','receiver'];
 if(r.version===4){
  const ids=[...'abcdef'].map(k=>'supply-'+k),within=(v,max)=>num(v)&&v>=-1000&&v<=max;
  if(!exact(p,['supply_to_weighing','weighing_to_receiver','supplier_feed_pa'])||!exact(f,k)||!exact(t,k)||!exact(p.supplier_feed_pa,ids)||
     !ids.every(id=>within(p.supplier_feed_pa[id],1000))||!within(f['supply-common'],1000)||!within(f.weigher,1000)||!within(f.receiver,0)||
     t['supply-common']!==0||!within(t.weigher,1000)||!within(t.receiver,0))throw Error('공급원 또는 단계 압력 범위·단위 오류');
 }else if(!exact(p,['supply_to_weighing','weighing_to_receiver'])||!exact(f,k)||!exact(t,k)||f['supply-common']!==500||f.weigher!==-500||f.receiver!==0||t['supply-common']!==0||t.weigher!==500||!num(t.receiver)||t.receiver<-1000||t.receiver>0)throw Error('단계 압력: 공급 +0.5/−0.5/0 고정, 이송 0/+0.5/수취 −1~0 kPa(g)');
 if(particles){r.version=5;r.particle_settings=particles;}
 return r;
}
export function validateBatchCommand(c){
 if(!isBatch(c.kind))return false;
 if(c.equipment!=='plant'||c.unit!=='none'||c.value!==0)throw Error('배치 명령 대상·단위 오류');
 for(const k of ['file_id','file_version','recipe_revision','recipe_fingerprint','batch_id','check_token'])if(c[k]!==undefined&&(typeof c[k]!=='string'||c[k].length>64))throw Error('배치 식별자 형식 오류');
 if(c.confirm!==undefined&&typeof c.confirm!=='boolean')throw Error('명시적 확인 형식 오류');
 if(['recipe_save','recipe_apply'].includes(c.kind))decodeRecipe(c.recipe_json);
 if(c.kind==='recipe_load'&&!c.file_id)throw Error('목록에서 불러올 레시피 선택 필요');
 if(c.kind==='recipe_save'&&c.file_id&&(!c.confirm||!c.file_version))throw Error('덮어쓰기 대상·버전·명시적 확인 필요');
 if(['recipe_apply','recipe_charge','batch_start'].includes(c.kind)&&(!/^(0|[1-9]\d{0,18})$/.test(c.recipe_revision??'')||!c.recipe_fingerprint))throw Error('현재 적용 설정 버전 확인 필요');
 if(['batch_check','batch_resume','batch_abandon'].includes(c.kind)&&!c.batch_id)throw Error('대상 배치 확인 필요');
 if(c.kind==='batch_resume'&&!c.check_token)throw Error('명시적 배치 상태 확인 필요');
 if(c.kind==='batch_abandon'&&!c.confirm)throw Error('대상 배치 포기 확인 필요');return true;
}
