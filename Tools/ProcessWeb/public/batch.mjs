import {operatorNumber,displayMetric} from './protocol.mjs';
import {webContext,sendCommand,reason,acknowledgeTransfer} from './manual.mjs';
import {emptyRecipe,decodeRecipe} from './recipes.mjs';
import {pending,statusLabels} from './commands.mjs';
const $=id=>document.getElementById(id),make=(tag,text)=>{const x=document.createElement(tag);if(text!==undefined)x.textContent=text;return x;},fmt=v=>v===null||v===undefined?'확인 중':operatorNumber(v),mass=v=>{const m=displayMetric({unit:'kg',value:v,validity:Number.isFinite(v)?'valid':'unknown'},{mass:true});return m.text+' '+m.unit;};
let original=emptyRecipe(),files=[],loaded=null,editing=false,feedback='',lastBatch='',identity='';const fields=new Map(),intents=new Map();
function summary(text){try{const r=JSON.parse(text);return `${r.name||'이름 미입력'} · ${r.mode==='a_full'?'A 전량':r.materials.filter(m=>m.enabled!==false).map(m=>`${m.hopper_id.slice(-1).toUpperCase()} ${mass(m.target_kg)} ±${mass(m.tolerance_kg)}`).join(' / ')} · ${fmt(r.mixing.target_rpm)} RPM · ${fmt(r.mixing.duration_s)} 해석초 · 첨가제 ${!r.additive.enabled?'미사용':!r.additive.scheduled?'초기 투입':`예약 ON · ${fmt(r.additive.delay_s)}초`} · 이송 수취 ${fmt((r.hopper_pressure_targets?.weighing_to_receiver.receiver??-1000)/1000)} kPa(g)`;}catch{return '확인 대기';}}
function field(id,label,{unit='',step=1,type='number',options=[]}={}){
 const box=make('div'),l=make('label',label),row=make('div'),input=make(options.length?'select':'input'),err=make('p');box.className='recipe-field';row.className='number-row';input.id='recipe-'+id;input.setAttribute('aria-label',label);l.htmlFor=input.id;
 if(options.length)for(const [v,t]of options){const o=make('option',t);o.value=v;input.append(o);}else{input.type=type==='check'?'checkbox':'text';if(type==='number')input.inputMode='decimal';}
 const f={input,box,err,initial:'',value:undefined,type};fields.set(id,f);
 if(type==='number'){for(const [sign,txt]of [[-1,'−'],[1,'+']]){const b=make('button',txt);b.type='button';b.setAttribute('aria-label',label+(sign<0?' 감소':' 증가'));b.onclick=()=>{const v=Number(input.value);if(input.value.trim()&&Number.isFinite(v)){input.value=String(Number((v+sign*step).toPrecision(15)));editing=true;render();}};if(sign<0)row.append(b);else{row.append(input,b,make('span',unit));}}}else row.append(input);
 input.addEventListener('keydown',e=>{if(e.key==='Enter')e.preventDefault();});input.addEventListener('wheel',e=>e.preventDefault(),{passive:false});input.addEventListener('input',()=>{editing=true;render();});input.addEventListener('change',()=>{editing=true;render();});err.className='field-error';box.append(l,row,err);$('recipe-fields').append(box);
}
field('name','레시피 이름',{type:'text'});field('mode','배치 모드',{type:'text',options:[['a_full','A 단독 전량'],['a_target','A 단독 목표량'],['sequential_ab','A → B 순차 계량'],['hopper_sequence','공급호퍼 선택 · 순차 계량']]});
for(const k of 'abcdef'){field('name-'+k,`${k.toUpperCase()} 원료 표시명`,{type:'text'});field('target-'+k,`${k.toUpperCase()} 목표량`,{unit:'g',step:1});field('tolerance-'+k,`${k.toUpperCase()} 계량 허용 오차`,{unit:'g',step:.1});}
for(const k of 'abcdef'){
 const label=k.toUpperCase();
 field('role-'+k,label+' 투입 역할',{type:'text',options:[['','미지정'],['material','일반 원료'],['additive','첨가제'],['off-material','일반 원료 · 미사용'],['off-additive','첨가제 · 미사용']]});
 field('order-'+k,label+' 투입 순서',{unit:'번째',step:1});
}
field('c-enabled','C 첨가제 사용',{type:'check'});field('c-scheduled','첨가제 예약 ON (OFF는 초기 투입)',{type:'check'});field('delay','혼합 시작 후 첨가제 투입 시각 N',{unit:'해석초',step:.1});field('rpm','혼합 목표 RPM',{unit:'RPM',step:1});field('duration','혼합 시간',{unit:'해석초',step:1});field('receiver','칭량→수취 단계 수취 목표 압력',{unit:'kPa(g)',step:.1});
const pressureFields=[
 ['feed-wh','공급 단계 칭량 압력','supply_to_weighing','weigher'],
 ['feed-rh','공급 단계 수취 압력','supply_to_weighing','receiver'],
 ['transfer-wh','수취 이송 단계 칭량 압력','weighing_to_receiver','weigher']
];
for(const [id,label] of pressureFields)field(id,label,{unit:'kPa(g)',step:.1});
for(const k of 'abcdef')field('pressure-'+k,k.toUpperCase()+' 공급 기준 압력',{unit:'kPa(g)',step:.1});
function fill(r){original=structuredClone(r);const vals={name:r.name,mode:r.mode,'c-enabled':r.additive.enabled,'c-scheduled':r.additive.scheduled,delay:r.additive.delay_s,rpm:r.mixing.target_rpm,duration:r.mixing.duration_s,receiver:r.hopper_pressure_targets.weighing_to_receiver.receiver/1000};for(const [id,,phase,axis] of pressureFields)vals[id]=r.hopper_pressure_targets[phase][axis]/1000;for(const k of 'abcdef'){vals['pressure-'+k]=(r.hopper_pressure_targets.supplier_feed_pa?.['supply-'+k]??r.hopper_pressure_targets.supply_to_weighing['supply-common'])/1000;const m=r.materials.find(m=>m.hopper_id==='supply-'+k);
 vals['role-'+k]=m?(m.enabled===false?'off-':'')+(m.role??(m.hopper_id===r.additive.hopper_id&&r.additive.enabled?'additive':'material')):'';
 vals['order-'+k]=m?r.materials.indexOf(m)+1:k.charCodeAt(0)-96;vals['name-'+k]=m?.display_name??(k==='c'?'첨가제 C':'원료 '+k.toUpperCase());vals['target-'+k]=(m?.target_kg??0)*1000;vals['tolerance-'+k]=(m?.tolerance_kg??0)*1000;}for(const [k,v]of Object.entries(vals)){const f=fields.get(k);if(f.type==='check')f.input.checked=v;else f.input.value=String(v);f.initial=f.input.value;f.value=v;}editing=false;}
const value=k=>{const f=fields.get(k);return f.type==='check'?f.input.checked:f.type==='number'?f.input.value.trim()===''?NaN:Number(f.input.value):f.input.value;};
// Unedited values use the exact decoded number, never a formatted display converted back to kg/Pa.
const preserved=(k,raw,scale=1)=>fields.get(k).input.value===fields.get(k).initial&&raw!==undefined?raw:value(k)/scale;
function collect(){const r=emptyRecipe();r.name=value('name');r.mode=value('mode');const ab=r.mode==='sequential_ab',c=ab&&value('c-enabled');r.feed_order=['supply-a',...(ab?['supply-b']:[]),...(c?['supply-c']:[])];r.materials=r.feed_order.map(id=>{const k=id.slice(-1),old=original.materials.find(m=>m.hopper_id===id),full=r.mode==='a_full';return {hopper_id:id,display_name:value('name-'+k),target_kg:full?0:preserved('target-'+k,old?.target_kg,1000),tolerance_kg:full?0:preserved('tolerance-'+k,old?.tolerance_kg,1000)};});r.additive.enabled=c;r.additive.scheduled=c&&value('c-scheduled');r.additive.delay_s=r.additive.scheduled?preserved('delay',original.additive.delay_s):0;if(r.mode==='hopper_sequence'){
  r.version=3;
  const keys=[...'abcdef'].filter(k=>value('role-'+k)).sort((a,b)=>value('order-'+a)-value('order-'+b));
  r.materials=keys.map(k=>{
   const id='supply-'+k,role=value('role-'+k),old=original.materials.find(m=>m.hopper_id===id);
   return {hopper_id:id,display_name:value('name-'+k),enabled:!role.startsWith('off-'),role:role.replace('off-',''),target_kg:preserved('target-'+k,old?.target_kg,1000),tolerance_kg:preserved('tolerance-'+k,old?.tolerance_kg,1000)};
  });
  const active=r.materials.filter(m=>m.enabled),add=active.find(m=>m.role==='additive');
  r.feed_order=active.map(m=>m.hopper_id);
  r.additive={enabled:!!add,hopper_id:add?.hopper_id??'',scheduled:value('c-scheduled'),delay_s:value('c-scheduled')?preserved('delay',original.additive.delay_s):0};
 }r.mixing={target_rpm:preserved('rpm',original.mixing.target_rpm),duration_s:preserved('duration',original.mixing.duration_s)};r.hopper_pressure_targets.weighing_to_receiver.receiver=preserved('receiver',original.hopper_pressure_targets.weighing_to_receiver.receiver,.001);
 const old=original.hopper_pressure_targets,p=r.hopper_pressure_targets,sources={};
 let individual=!!old.supplier_feed_pa;
 for(const [id,,phase,axis] of pressureFields){
  p[phase][axis]=preserved(id,old[phase][axis],.001);
  if(p[phase][axis]!==old[phase][axis])individual=true;
 }
 p.supply_to_weighing['supply-common']=old.supply_to_weighing['supply-common'];
 for(const k of 'abcdef'){
  const id='supply-'+k;
  sources[id]=preserved('pressure-'+k,old.supplier_feed_pa?.[id]??p.supply_to_weighing['supply-common'],.001);
  if(sources[id]!==p.supply_to_weighing['supply-common'])individual=true;
 }
 if(individual){r.version=4;p.supplier_feed_pa=sources;}
 return r;}
function validation(){let first='';for(const f of fields.values())f.err.textContent='';const fail=(k,s)=>{fields.get(k).err.textContent=s;first ||=s;};const r=collect();if(r.mode==='hopper_sequence'){
  const used=new Set();
  for(const m of r.materials){const k=m.hopper_id.slice(-1),n=value('order-'+k);
   if(!Number.isInteger(n)||n<1||n>6||used.has(n))fail('order-'+k,'1~6 중 겹치지 않는 순서를 선택하세요.');
   used.add(n);
  }
 }if(!r.name.trim()||r.name.trim()!==r.name||r.name.length>80)fail('name','이름은 앞뒤 공백 없이 1~80자');for(const m of r.materials){const k=m.hopper_id.slice(-1);if(!m.display_name.trim())fail('name-'+k,'표시명 입력 필요');if(r.mode!=='a_full'&&m.enabled!==false){if(!Number.isFinite(m.target_kg)||m.target_kg<=0)fail('target-'+k,'양수 목표량 필요');if(!Number.isFinite(m.tolerance_kg)||m.tolerance_kg<0||m.tolerance_kg>=m.target_kg)fail('tolerance-'+k,'공차는 0 이상, 목표량 미만');}}if(!Number.isFinite(r.mixing.target_rpm)||r.mixing.target_rpm<=0||r.mixing.target_rpm>30)fail('rpm','0 초과 30 이하 RPM');if(!Number.isFinite(r.mixing.duration_s)||r.mixing.duration_s<=0)fail('duration','양수 해석 시간 필요');if(r.additive.scheduled&&(!Number.isFinite(r.additive.delay_s)||r.additive.delay_s<0||r.additive.delay_s>=r.mixing.duration_s))fail('delay','0 이상, 혼합 시간 미만');for(const [id,,phase,axis] of pressureFields){
 const pa=r.hopper_pressure_targets[phase][axis];
 if(!Number.isFinite(pa)||pa < -1000||pa>(axis==='receiver'?0:1000))fail(id,axis==='receiver'?'−1~0 kPa(g) 범위':'−1~+1 kPa(g) 범위');
 }
 for(const [id,pa] of Object.entries(r.hopper_pressure_targets.supplier_feed_pa??{})){
  if(!Number.isFinite(pa)||Math.abs(pa)>1000)fail('pressure-'+id.slice(-1),'−1~+1 kPa(g) 범위');
 }
 const p=r.hopper_pressure_targets.weighing_to_receiver.receiver;if(!Number.isFinite(p)||p<-1000||p>0)fail('receiver','−1~0 kPa(g) 범위');if(!first)try{decodeRecipe(JSON.stringify(r));}catch(e){first=e.message;}return {r,error:first};}
function operationReason(k){const ctx=webContext();const ownPending=ctx.records.some(r=>r.command?.kind===k&&pending(r.status));return ownPending?'현재 요청 처리 확인 중':reason('plant',k,0);}
async function send(k,extra={},intent=k){feedback='요청 접수 · 처리 확인 중';render();const r=await sendCommand(k,'plant',0,'none',extra);if(r){intents.set(r.request_id,intent);consume();}render();}
function consume(){const ctx=webContext();for(const r of ctx.records){if(!intents.has(r.request_id)||pending(r.status))continue;const intent=intents.get(r.request_id);intents.delete(r.request_id);feedback=`${statusLabels[r.status]??r.status} · ${r.message??''}`;
 if(r.status==='applied'&&r.data){if(intent==='recipe_list'){files=r.data.files??[];const selected=$('recipe-files').value;$('recipe-files').replaceChildren(make('option','저장 레시피 선택'));$('recipe-files').firstChild.value='';for(const f of files){const o=make('option',f.name+(f.error?' · 파일 오류':''));o.value=f.file_id;$('recipe-files').append(o);}if(files.some(f=>f.file_id===selected))$('recipe-files').value=selected;if(r.data.truncated)feedback+=' · 목록 128개 한도';}
 if(intent==='recipe_load'){try{fill(decodeRecipe(r.data.recipe_json));loaded={file_id:r.data.file_id,file_version:r.data.file_version};feedback='불러오기 완료 · 편집값만 변경. 전체 적용 후 공정 화면에서 시작하세요.';}catch(e){feedback=e.message;}}
 if(intent==='recipe_save'){loaded={file_id:r.data.file_id,file_version:r.data.file_version};$('overwrite-confirm').checked=false;feedback='저장 완료 · 적용값·장비 상태는 바뀌지 않았습니다. 목록 새로고침으로 확인하세요.';}
 if(intent==='recipe_apply'){editing=false;feedback='레시피 적용됨 · 공정 화면에서 남은 시작 조건을 확인하세요.';}
 }
}}
function render(){if(!$('recipe-panel'))return;consume();const ctx=webContext(),w=ctx.packet?.envelope?.control?.batch_workflow,b=ctx.packet?.envelope?.snapshot?.batch;const id=ctx.packet?.envelope?ctx.packet.envelope.run_id+'|'+ctx.packet.envelope.snapshot?.epoch:'';
 if(identity&&identity!==id){files=[];loaded=null;intents.clear();$('recipe-files').replaceChildren(make('option','실행 변경 · 목록 새로고침 필요'));$('recipe-files').firstChild.value='';}identity=id;
 if(lastBatch!==w?.batch_id){$('abandon-confirm').checked=false;lastBatch=w?.batch_id;}
 const active=!!b?.active,locked=active||['requested','closing','pausing'].includes(b?.stop_stage),v=validation(),ab=value('mode')==='sequential_ab',c=ab&&value('c-enabled');
 for(const [k,f]of fields){const relevant=!((k.endsWith('-b')&&!ab)||(k.endsWith('-c')&&!c)||(k==='c-enabled'&&!ab)||(k==='c-scheduled'&&!c)||(k==='delay'&&(!c||!value('c-scheduled')))||k.startsWith('target-')&&value('mode')==='a_full'||k.startsWith('tolerance-')&&value('mode')==='a_full');const custom=value('mode')==='hopper_sequence';
 const customOnly=/^(role|order)-/.test(k)||/-[def]$/.test(k);
 f.box.hidden=custom?k==='c-enabled':customOnly;
 const customAllowed=k!=='delay'||value('c-scheduled');
 f.input.disabled=locked||f.box.hidden||!(custom?customAllowed:relevant);for(const button of f.box.querySelectorAll('button'))button.disabled=f.input.disabled;}
 $('recipe-order').textContent=value('mode')==='hopper_sequence'?'선택 순서: '+v.r.feed_order.join(' → '):'지원 투입 순서: '+(ab?'A → B'+(c?' → C':''):'A')+' · C 예약 ON 0초는 혼합 적용 직후 투입, OFF는 초기 투입';
 $('applied-summary').textContent=w?`현재 적용 r${w.recipe_revision} · ${summary(w.applied_json)}`:'적용 레시피 확인 대기';$('recipe-applied-summary').textContent=$('applied-summary').textContent;
 $('execution-summary').textContent=w?.execution_json?summary(w.execution_json)+' · 배치 '+w.batch_id:'배치 시작 전 · 시작 시 실행값 확정';$('recipe-dirty').textContent=editing?'편집값 변경됨 · 적용값과 별도. 전체 적용을 눌러야 다음 실행 설정이 바뀝니다.':'편집값과 적용값은 별도입니다. 불러오기만으로 적용되지 않습니다.';
 const completed=b?.stage==='complete'&&!b.active;
 // A transfer's transient complete flag no longer describes the process after mixer delivery.
 // Keep its record for diagnostics; current-state text follows the confirmed batch stage.
 const t=w?.transfer;$('transfer-progress').textContent=completed?'이송 종료 · 원료별 최종 혼합기 도착량은 아래 결과에서 확인하세요.':t?`이송 #${t.id} · ${t.complete?'완료':'진행/확인 중'} · 기준 ${mass(t.baseline_kg)} / 남은 ${mass(t.remaining_kg)} / 배관 ${mass(t.transit_kg)} / 수취 ${mass(t.arrived_kg)}`:'';acknowledgeTransfer(t?.complete&&!ctx.packet.data_stale&&ctx.packet.connection.status==='connected'?{run_id:ctx.packet.envelope.run_id,epoch:ctx.packet.envelope.snapshot.epoch,sequence:ctx.packet.envelope.snapshot.sequence,batch_id:w.batch_id,transfer_id:t.id}:null);
 $('mix-progress').textContent=w?.batch_id?`혼합 ${fmt(w.mixing_elapsed_s)} / ${fmt(w.mixing_duration_s)} 해석초 · 남은 ${fmt(w.mixing_remaining_s)}초`:'';$('additive-progress').textContent=w?.additive_status??'';
 const set=(id,k,msg,extra='')=>{const why=operationReason(k)||extra;$(id).disabled=!!why;$(msg).textContent=why||'사용 가능 · 명시적으로 눌러 실행하세요.';};set('batch-start','batch_start','start-reason');set('batch-check','batch_check','check-reason');set('batch-resume','batch_resume','batch-resume-reason');set('batch-abandon','batch_abandon','abandon-reason',!$('abandon-confirm').checked?'대상 배치 포기 확인란을 선택하세요.':'');
 const fresh=ctx.packet?.connection.status==='connected'&&!ctx.packet?.data_stale&&!ctx.unavailable&&ctx.packet?.envelope?.snapshot?.validity==='valid';const r=fresh?w?.recovery:w?.recovery?{...w.recovery,checked:false,reason:'최신 상태 확인 필요 · 연결과 데이터 갱신을 확인하세요.',movement_summary:'이전 확인값은 오래된 상태입니다. 최신 연결 후 상태 확인하세요.',materials:(w.recovery.materials??[]).map(m=>({...m,already_passed_kg:null,transit_kg:null,additional_supply_kg:null,remaining_transfer_kg:null}))}:null;let title=r?.checked?'재개 가능 · 현재 배치 확인 완료':r?.pending?'수동 조치 / 배치 상태 확인':r?.abandoned?'배치 포기 · 이력 보존':'정지 후 남은 양 재개';if(['requested','closing','pausing'].includes(b?.stop_stage))title='정지 확인 중';if(b?.stop_stage==='failed')title='전체 정지 실패 · 재시도 필요';$('recovery-panel').hidden=!r?.pending&&!r?.abandoned&&!['requested','closing','pausing','failed'].includes(b?.stop_stage);$('recovery-title').textContent=title;$('recovery-reason').textContent=b?.stop_stage==='failed'||['requested','closing','pausing'].includes(b?.stop_stage)?b.stop_reason:r?.pending?r.reason:r?.abandoned?'이전 배치를 재개하지 않습니다. 새 배치는 초기화와 시작 조건 확인이 필요합니다.':'전체 정지 후 보존된 배치가 있으면 상태를 확인합니다.';
 $('movement-summary').textContent=r?.movement_summary??'배치 상태 확인 후 남은 양이 표시됩니다.';$('recovery-materials').replaceChildren(...(r?.materials??[]).map(m=>{const tr=make('tr');for(const x of [m.label,mass(m.already_passed_kg),mass(m.transit_kg),mass(m.additional_supply_kg),mass(m.remaining_transfer_kg)])tr.append(make('td',x));return tr;}));$('abandon-description').textContent=`대상 배치: ${w?.batch_id||'없음'}. 기록과 이미 투입된 원료는 보존하고 이 배치의 재개를 종료합니다.`;
 const select=files.find(f=>f.file_id===$('recipe-files').value);$('overwrite-target').textContent='덮어쓸 대상: '+(select?.name??'목록에서 선택하세요.');
 $('recipe-list').disabled=!!operationReason('recipe_list');$('recipe-load').disabled=!!operationReason('recipe_load')||!select;$('recipe-copy').disabled=locked||!w;$('recipe-library-reason').textContent=operationReason('recipe_load')||select?.error||'불러오기는 편집값만 채웁니다.';
 const err=operationReason('recipe_apply')||v.error;$('recipe-apply').disabled=!!err;$('recipe-save-new').disabled=!!operationReason('recipe_save')||!!v.error;$('recipe-action-reason').textContent=err||'전체 검증 후 적용 · 재고와 입자 분해능은 시작 시 다시 확인';
 const overwriteWhy=operationReason('recipe_save')||v.error||(!loaded||loaded.file_id!==select?.file_id?'대상 레시피를 먼저 불러와 버전을 확인하세요.':!$('overwrite-confirm').checked?'대상 이름을 확인하고 덮어쓰기 확인란을 선택하세요.':'');$('recipe-overwrite').disabled=!!overwriteWhy;$('overwrite-reason').textContent=overwriteWhy;
 $('recipe-feedback').textContent=feedback;$('batch-feedback').textContent=completed&&!['requested','closing','pausing','failed'].includes(b.stop_stage)?'배치 완료 · 최종 도착량과 계산 적용 RPM을 확인하세요. 이전 요청 결과는 명령 이력에 보존됩니다.':feedback;
}
function view(settings){$('recipe-panel').hidden=!settings;for(const e of document.querySelectorAll('.workspace,.request-panel,.results,.manual-toolbar,#batch-operation'))e.hidden=settings;if(settings)$('recipe-panel').scrollIntoView({block:'start'});else $('batch-operation').scrollIntoView({block:'start'});}
for(const id of ['show-recipes','start-settings'])$(id).onclick=()=>view(true);for(const id of ['show-process','return-process'])$(id).onclick=()=>view(false);
$('recipe-list').onclick=()=>send('recipe_list');$('recipe-load').onclick=()=>send('recipe_load',{file_id:$('recipe-files').value});$('recipe-files').onchange=()=>{$('overwrite-confirm').checked=false;render();};$('overwrite-confirm').onchange=$('abandon-confirm').onchange=render;
$('recipe-copy').onclick=()=>{try{const r=JSON.parse(webContext().packet.envelope.control.batch_workflow.applied_json);fill(r);feedback='현재 적용값 복사 · 편집값만 변경';render();}catch(e){feedback=e.message;}};
const config=()=>{const w=webContext().packet?.envelope?.control?.batch_workflow;return {recipe_revision:w?.recipe_revision,recipe_fingerprint:w?.recipe_fingerprint};};
$('recipe-apply').onclick=()=>{const v=validation();if(!v.error)send('recipe_apply',{...config(),recipe_json:JSON.stringify(v.r)});};$('recipe-save-new').onclick=()=>{const v=validation();if(!v.error)send('recipe_save',{recipe_json:JSON.stringify(v.r)});};$('recipe-overwrite').onclick=()=>{const v=validation();if(!v.error&&loaded&&$('overwrite-confirm').checked)send('recipe_save',{recipe_json:JSON.stringify(v.r),...loaded,confirm:true});};
$('batch-start').onclick=()=>send('batch_start',config());for(const [id,k]of [['batch-check','batch_check'],['batch-resume','batch_resume'],['batch-abandon','batch_abandon']])$(id).onclick=()=>{const w=webContext().packet?.envelope?.control?.batch_workflow;send(k,{batch_id:w?.batch_id,check_token:w?.recovery?.check_token,confirm:k==='batch_abandon'&&$('abandon-confirm').checked});};
fill(emptyRecipe());document.addEventListener('process-control',render);render();
