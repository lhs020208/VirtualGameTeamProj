// Isolated acknowledgement fixture: flow values are test inputs, not a fluid model.
import {MockBatchOperator,exampleRecipe} from './mock-batch.mjs';
import {metric} from './mock.mjs';
import {transferScopeReason} from './public/commands.mjs';
export class MockPressureOperator extends MockBatchOperator {
 constructor(){super({scenario:'pressure'});this.store.close(); // Recreate ONLY the owned temporary mock store.
  this.store=new this.store.constructor();const r=exampleRecipe();r.name='모의 실시간 수취 압력';r.hopper_pressure_targets.weighing_to_receiver.receiver=-500;this.store.save(JSON.stringify(r));
  this.pressureQueue=[];this.pressureDelayTicks=3;this.pressureExecutions=0;this.transferId='0';this.receiverPa=0;this.requested=null;this.lastPressure=null;this.lastEnvelope=null;
 }
 cancelPressure(why){for(const q of this.pressureQueue){const r=this.results.get(q.c.request_id);r.status='unknown';r.message=why;}this.pressureQueue=[];}
 receive(f){
  if(this.pressureQueue.length&&this.lease&&this.lease!==f.lease_id){this.cancelPressure('제어권 종료 · 기존 전체 정지 경로');super.receive({...f,commands:[{request_id:'mock-disconnect-stop',kind:'stop_all',epoch:'1'}]});}
  const other=[];for(const c of f.commands??[]){
   if(c.kind==='stop_all'||c.kind==='pause'){this.cancelPressure('전체 정지 우선 · 취소된 압력 자동 복원 안 함');this.receiverPa=0;}
   if(c.kind!=='transfer_pressure'){other.push(c);continue;}if(this.results.has(c.request_id))continue;
   const why=c.run_id!==this.lastEnvelope?.run_id||c.epoch!==this.lastEnvelope?.snapshot.epoch?'실행·세대 변경':!c.lease_id||c.lease_id!==f.lease_id?'제어권 없음':this.pressureQueue.length?'현재 요청 적용 확인 중':transferScopeReason(this.lastEnvelope,c);
   const r={request_id:c.request_id,service_request_id:c.request_id,epoch:c.epoch,batch_id:c.batch_id,transfer_id:c.transfer_id,status:why?'rejected':'processing',message:why||'모의 작업자 적용 확인 중',revision:String(++this.pressureExecutions)};this.results.set(c.request_id,r);
   if(!why){this.requested=c.value*1000;this.lastPressure={c,requested_s:this.seconds,applied_s:null};this.pressureQueue.push({c,remaining:this.pressureDelayTicks});}
  }super.receive({...f,commands:other});
 }
 apply(c){super.apply(c);if(c.kind==='batch_resume'&&this.results.get(c.request_id)?.status==='applied'){this.receiverPa=this.executed.hopper_pressure_targets.weighing_to_receiver.receiver;this.requested=null;this.lastPressure=null;}}
 tick(n){
  // More than one browser heartbeat in a transfer; all timing remains mock fixture input.
  if(this.auto&&this.progress>=8&&this.progress<21)this.progress-=.5;
  const e=super.tick(n),s=e.snapshot,transferring=s.batch.stage==='transferring',id=this.progress<8?'0':this.progress<31?'1':'2';
  s.sequence=String(n); // A new coherent mock snapshot, even though the base manual fixture is paused.
  const recipe=this.executed?.hopper_pressure_targets.weighing_to_receiver.receiver??null;
  if(id!==this.transferId){this.cancelPressure('다음 이송 전환 · 이전 요청 취소');this.transferId=id;this.receiverPa=recipe??0;this.requested=null;this.lastPressure=null;}
  if(!transferring)this.cancelPressure('이송 종료 · 이전 요청 취소');
  for(const q of [...this.pressureQueue])if(--q.remaining<=0){this.receiverPa=q.c.value*1000;this.lastPressure.applied_s=this.seconds;const r=this.results.get(q.c.request_id);r.status='applied';r.message='모의 적용 완료 · 물리 반응 검증 아님';this.pressureQueue=this.pressureQueue.filter(x=>x!==q);}
  if(this.stopStage==='closing'||this.stopStage==='complete')this.receiverPa=0;
  const why=this.stopStage==='closing'?'전체 정지 확인 중':this.pressureQueue.length?'적용 확인 중 · 현재 요청 응답 대기':!this.auto?'자동 이송 종료 또는 중단':!transferring?'칭량→수취 이송 준비 또는 종료':this.receiverOutletOpen?'수취 출구 실제 닫힘 확인 필요':'';
  const flow=transferring&&this.auto&&!this.receiverOutletOpen?(this.receiverPa===-1000?.07:.04):null; // Two canned values, NOT calculated physics.
  const t={batch_id:this.batchId,transfer_id:id,epoch:s.epoch,sequence:s.sequence,simulation_seconds:s.simulation_seconds,validity:s.validity,available:!why,reason:why,stage:s.batch.label,recipe_pa:recipe,requested_pa:this.requested,applied_pa:this.receiverPa,flow_m3_s:flow,flow_reason:flow===null?'미산출 · 현재 열린 이송 없음':'',service_request_id:this.lastPressure?.c.request_id??'',pending:!!this.pressureQueue.length,status:this.lastPressure?this.results.get(this.lastPressure.c.request_id)?.message:'이번 이송의 조절 요청 없음',requested_s:this.lastPressure?.requested_s??null,applied_s:this.lastPressure?.applied_s??null,pressure_applied_revision:String(this.pressureExecutions)};
  e.control.transfer_pressure=t;e.control.actions.push({equipment:'receiver',kind:'transfer_pressure',value:0,unit:'kPa(g)',reason:why});e.control.batch_workflow.transfer.id=id;
  const rh=s.equipment.find(x=>x.id==='receiver'),wh=s.equipment.find(x=>x.id==='weigher');rh.metrics.pressure_requested=metric(this.requested??this.receiverPa,'Pa(g)');rh.metrics.pressure_applied=metric(this.receiverPa,'Pa(g)');rh.metrics.flow=metric(flow,'m3/s','모의 유량 시험 입력',flow===null?'not_computed':'valid',t.flow_reason);wh.metrics.valve_requested=wh.metrics.valve_applied=metric(transferring&&this.auto?1:0,'bool');rh.metrics.valve_requested=rh.metrics.valve_applied=metric(this.receiverOutletOpen?1:0,'bool');
  this.lastEnvelope=structuredClone(e);return e;
 }
}
