import {LEVEL,ACTOR} from './public/protocol.mjs';
export const metric=(value,unit,source='모델 계산값',validity='valid',reason='')=>({value:validity==='valid'?value:null,unit,source,validity,reason});
export function mockEnvelope(tick=1,scenario='flowing') {
 const m=metric, paused=scenario==='paused', complete=scenario==='complete', bad=scenario==='error', unknown=scenario==='unknown';
 const equipment=Array.from({length:scenario==='expanded'?10:6},(_,i)=>({id:`supply-${String.fromCharCode(97+i)}`,label:`공급 ${String.fromCharCode(65+i)}`,kind:'supplier',metrics:{valve_requested:m(i===1&&!complete?1:0,'bool'),valve_applied:m(i===1&&!complete?1:0,'bool'),particles:m(1750-i*84,'particles'),mass:m((1750-i*84)*.0000402123917,'kg'),pressure_requested:m(500,'Pa(g)','호퍼 목표 압력'),pressure_applied:m(500,'Pa(g)','적용 압력 설정'),valve_before:m(440,'Pa(g)','모델 추정값',i===1&&!complete?'valid':'not_computed',i===1&&!complete?'':'닫힘 · 차압 미산출'),valve_after:m(395,'Pa(g)','모델 추정값',i===1&&!complete?'valid':'not_computed',i===1&&!complete?'':'닫힘 · 차압 미산출'),valve_difference:m(45,'Pa','모델 추정값',i===1&&!complete?'valid':'not_computed',i===1&&!complete?'':'닫힘 · 차압 미산출'),flow:m(.043,'m3/s','계산 유량',i===1&&!complete?'valid':'not_computed',i===1&&!complete?'':'공급 밸브 닫힘'),weigher_pressure_applied:m(-500,'Pa(g)','칭량호퍼 적용 압력 설정')}}));
 equipment.push({id:'weigher',label:'칭량호퍼 WH101',kind:'weigher',metrics:{mass:m(complete?0:.039971117,'kg'),particles:m(complete?0:994,'particles'),valve_requested:m(0,'bool'),valve_applied:m(0,'bool'),pressure_requested:m(-500,'Pa(g)'),pressure_applied:m(-500,'Pa(g)','적용 압력 설정'),flow:m(.0680458769,'m3/s','계산 이송 유량')}});
 equipment.push({id:'receiver',label:'수취호퍼 RH-201',kind:'receiver',metrics:{mass:m(complete?0:.100370130,'kg'),particles:m(complete?0:2496,'particles'),valve_requested:m(0,'bool'),valve_applied:m(0,'bool'),pressure_requested:m(-500,'Pa(g)'),pressure_applied:m(-500,'Pa(g)','적용 압력 설정'),flow:m(.0680458769,'m3/s','계산 이송 유량')}});
 equipment.push({id:'mixer',label:'혼합기 MX-301',kind:'mixer',metrics:{mass:m(complete?.241193:0,'kg'),particles:m(complete?5998:0,'particles'),rpm_target:m(15,'rpm'),rpm_applied:m(complete?0:15,'rpm','계산 적용 RPM')}});
 if(unknown)equipment[6].metrics.mass=m(0,'kg','모델 계산값','unknown','질량 스냅샷 확인 대기');
 if(scenario==='tiny')equipment[6].metrics.mass=m(.0000001,'kg'),equipment[6].metrics.particles=m(1,'particles');
 if(scenario==='unsupported')equipment[5].metrics=Object.fromEntries(Object.entries(equipment[5].metrics).map(([k,v])=>[k,{...v,value:null,validity:'unsupported',reason:'이 시험 모델에서 지원하지 않는 장비'}]));
 return {schema_version:1,publisher_id:'mock-publisher',run_id:'mock-run',publication_sequence:String(tick),level:LEVEL,actor:ACTOR,binding:{status:'connected',reason:'모의 데이터 · 실제 시뮬레이션에 연결하지 않음',next_action:'화면·프로토콜 시험에만 사용'},control:{owner:'unreal',web_read_only:true,transfer_supported:false},snapshot:{epoch:'1',sequence:String(paused?1:tick),simulation_seconds:paused?2.5:tick*.025,age_ms:scenario==='stale'?5000:0,validity:bad?'error':scenario==='stale'?'stale':'valid',execution:paused?'paused':bad?'fault':'running',error:bad?'모의 오류: 작업자 계산 결과를 확인할 수 없습니다.':'',equipment,regions:[{id:'transfer-pipe',label:'칭량–수취 배관',metrics:{mass:m(complete?0:.025,'kg'),particles:m(complete?0:622,'particles')}},{id:'mixer-connector',label:'수취–혼합 연결부',metrics:{mass:m(0,'kg'),particles:m(0,'particles')}}],batch:{stage:complete?'complete':'feeding',label:complete?'배치 완료':'B 계량 중',active:!complete,reason:scenario==='long'?'B 공급 연결부의 잔류 입자를 확인 중입니다. 현재 공급 밸브의 실제 닫힘과 공기 차단을 확인한 뒤 칭량 안정 조건을 확인합니다. '.repeat(8):complete?'확정 원료 전량 도착 · 혼합 시간 충족 · 계산 적용 RPM 0 확인':'목표량 도달을 기다리고 있습니다. A 원료는 수취호퍼에 보관 중입니다.',stop_stage:'none',stop_reason:'',block_reason:bad?'작업자 오류로 진행할 수 없습니다.':'',materials:['A','B','C'].map((n,i)=>({id:`supply-${n.toLowerCase()}`,label:`원료 ${n}`,metrics:{target:m(i===2?.04:.1,'kg'),final:m(i===0?.10037013:i===1?.099887: .039971,'kg'),receiver:m(complete?0:i===0?.10037013:0,'kg'),mixer:m(complete?(i===0?.10037013:i===1?.099887:.039971):0,'kg')}}))}}};
}

// Deterministic CPU-only command peer. It models acknowledgements, never GPU dynamics.
export class MockOperator {
 constructor(scenario='manual'){this.scenario=scenario;this.lease='';this.results=new Map();this.queue=[];this.values=new Map();this.running=scenario!=='paused';this.stopped=false;this.rpm=15;this.mixing=false;this.stopStage='none';this.executions=0;this.seconds=0;}
 receive(f){this.lease=f.lease_id??'';for(const c of f.commands??[]){if(this.results.has(c.request_id))continue;const r={request_id:c.request_id,epoch:c.epoch,status:'processing',message:'모의 작업자 적용 확인 중',revision:String(++this.executions)};this.results.set(c.request_id,r);if(c.kind==='stop_all'||c.kind==='pause'){for(const q of this.queue){this.results.get(q.c.request_id).status='unknown';this.results.get(q.c.request_id).message='전체 정지로 취소';}this.queue=[];this.stopStage='closing';}this.queue.push({c,remaining:3});}}
 tick(n){
  if(this.running)this.seconds+=.025;
  for(const q of [...this.queue]){if(--q.remaining>0)continue;const c=q.c;this.queue=this.queue.filter(x=>x!==q);const r=this.results.get(c.request_id);r.status='applied';r.message='모의 적용 완료 · GPU 동작 증거 아님';
   if(c.kind==='stop_all'||c.kind==='pause'){this.values.clear();this.running=false;this.stopped=true;this.mixing=false;this.stopStage='complete';}
   else if(c.kind==='resume'){this.running=true;this.stopStage='none';}
   else if(c.kind==='rpm')this.rpm=c.value;
   else if(c.kind==='mixer_enabled')this.mixing=!!c.value;
   else {
    this.values.set(`${c.equipment}:${c.kind}`,c.value);
    if(c.kind==='pressure'&&/^supply-[a-f]$/.test(c.equipment))for(const letter of 'abcdef')this.values.set(`supply-${letter}:pressure`,c.value);
    if(c.kind==='purge_enabled'&&c.value===0)this.values.set(`${c.equipment}:purge_pressure`,0);
   }
  }
  const e=mockEnvelope(n,this.scenario==='manual'?'paused':this.scenario),s=e.snapshot;
  if(this.scenario==='manual'||this.stopped){s.execution=this.running?'running':'paused';s.batch.active=false;s.batch.stage='idle';s.batch.label='수동 조작';s.batch.reason='모의 데이터·모의 조작';s.simulation_seconds=this.seconds;}
  s.batch.stop_stage=this.stopStage;s.batch.stop_reason=this.stopStage==='closing'?'공기 차단·압력 0·밸브 닫힘·RPM 0·작업자 정지 확인 중':this.stopStage==='complete'?'전체 정지 확인 · 모의 결과':'';
  for(const d of s.equipment){
   const v=(k,def=0)=>this.values.get(`${d.id}:${k}`)??def;
   if(d.id==='mixer'){d.metrics.rpm_target=metric(this.rpm,'rpm');d.metrics.rpm_applied=metric(this.mixing?this.rpm:0,'rpm');continue;}
   if(this.scenario==='manual'||this.stopped){d.metrics.valve_requested=metric(v('valve'),'bool');d.metrics.valve_applied=metric(v('valve'),'bool');}
   if(this.values.has(`${d.id}:pressure`)||this.stopped){d.metrics.pressure_requested=metric(v('pressure')*1000,'Pa(g)');d.metrics.pressure_applied=metric(v('pressure')*1000,'Pa(g)');}
   if(/^supply-[a-c]$/.test(d.id)){for(const k of ['requested','applied']){d.metrics[`purge_${k}`]=metric(v('purge_enabled'),'bool');d.metrics[`purge_pressure_${k}`]=metric(v('purge_pressure')*1000,'Pa(g)');}d.metrics.purge_flow=metric(v('purge_enabled')?.01:0,'m3/s');}
  }
  const actions=[],add=(equipment,kind,value=0,unit='none')=>{let reason='';if(s.batch.active&&kind!=='stop_all')reason='자동 배치 실행 중 · 전체 정지 후 수동 조작';else if(this.stopStage==='closing'&&kind!=='stop_all')reason='전체 정지 확인 중';else if(kind!=='stop_all'&&!this.stopped)reason='먼저 전체 정지 완료를 확인하세요.';else if(!this.running&&!['resume','stop_all','pause','rpm'].includes(kind)&&!(['valve','mixer_enabled','purge_enabled'].includes(kind)&&value===0))reason='계산 일시정지 · 계산만 재개를 선택하세요.';if(kind.startsWith('purge')&&!/^supply-[a-c]$/.test(equipment))reason='이 연결부 공기 제어는 미지원입니다.';actions.push({equipment,kind,value,unit,reason});};
  for(const k of ['stop_all','resume','pause'])add('plant',k);
  for(const d of s.equipment){if(d.id==='mixer'){add(d.id,'rpm',this.rpm,'rpm');for(const v of [0,1])add(d.id,'mixer_enabled',v,'bool');}else{for(const v of [0,1])add(d.id,'valve',v,'bool');add(d.id,'pressure',0,'kPa(g)');if(d.kind==='supplier'){add(d.id,'purge_pressure',0,'kPa(g)');for(const v of [0,1])add(d.id,'purge_enabled',v,'bool');}}}
  e.control={owner:this.lease?'web':'unreal',lease_id:this.lease,web_read_only:false,transfer_supported:true,command_version:1,pause_policy:'CloseThenPause',unconfirmed:this.stopStage==='closing'?'모의 작업자 응답 대기':'',acquire_reason:s.batch.active?'자동 배치 실행 중':this.stopStage==='closing'?'전체 정지 확인 중':'',actions,results:[...this.results.values()]};return e;
 }
}
