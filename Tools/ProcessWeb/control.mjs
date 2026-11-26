import {isBatch,readKinds} from './public/recipes.mjs';
import {randomUUID,randomBytes} from 'node:crypto';
import {validateCommand,fingerprint,pending,mixScopeReason,transferScopeReason} from './public/commands.mjs';
// UUID spelling differs between browser crypto.randomUUID and Unreal FGuid.
// Compare identities case-insensitively, but keep the caller's receipt ID unchanged.
const requestKey=id=>typeof id==='string'?id.toLowerCase():'';
export class ControlRelay {
 constructor(store,{now=()=>performance.now()}={}){this.store=store;this.now=now;this.sessions=new Map();this.records=new Map();this.lease=null;this.identity=null;this.nextOrder=0;this.retired=new Map();}
 session(key){
  if(typeof key!=='string'||!/^[0-9a-f-]{36}$/i.test(key))throw Error('브라우저 복원 키 형식 오류');
  for(const [id,s] of this.sessions)if(s.key===key||this.now()-s.seen>1800000){if(this.lease?.session===id)this.release(id);this.sessions.delete(id);}
  if(this.sessions.size>=32)throw Error('브라우저 세션 한도입니다. 사용하지 않는 탭을 닫고 기다리세요.');
  const id=randomBytes(24).toString('hex');this.sessions.set(id,{key,seen:this.now()});return id;
 }
 auth(id){const s=this.sessions.get(id);if(!s||this.now()-s.seen>1800000)throw Error('세션 만료 · 새로고침 후 제어권을 다시 가져오세요.');return s;}
 refresh(){
  const p=this.store.view(),e=p.envelope,id=e?.snapshot?`${e.publisher_id}|${e.run_id}|${e.snapshot.epoch}`:null;
  if(this.identity!==id||p.connection.status!=='connected'||p.data_stale||this.lease&&this.now()>=this.lease.until){this.release();}
  if(this.identity!==id){for(const r of this.records.values())if(pending(r.status)){r.status='unknown';r.message='실행·초기화 세대 변경 · 이전 요청 재실행 금지';}this.identity=id;this.observedTransfer=null;}
  return p;
 }
 release(id){
  if(id&&this.lease?.session!==id)return;
  const lease=this.lease?.id;this.lease=null;
  for(const r of this.records.values())if(r.command.kind!=='stop_all'&&!readKinds.includes(r.command.kind)&&r.command.lease_id===lease&&pending(r.status)){r.status=r.status==='queued'?'rejected':'unknown';r.message='제어권 종료 · 대기 요청 취소, 자동 재전송 없음';}
 }
 observed(id,o){this.auth(id);const p=this.refresh(),e=p.envelope,w=e?.control?.batch_workflow;if(!o||p.data_stale||p.connection.status!=='connected'||!w?.transfer?.complete||o.run_id!==e.run_id||o.epoch!==e.snapshot.epoch||o.batch_id!==w.batch_id||o.transfer_id!==w.transfer.id||o.sequence!==e.snapshot.sequence)return;this.observedTransfer=structuredClone(o);}
 heartbeat(id){this.auth(id).seen=this.now();this.refresh();if(this.lease?.session===id)this.lease.until=this.now()+3000;return this.view(id);}
 acquire(id){
  this.auth(id);const p=this.refresh(),e=p.envelope;
  if(p.connection.status!=='connected'||p.data_stale||e?.snapshot?.validity!=='valid')throw Error('최신 언리얼 연결·공정 데이터가 필요합니다.');
  if(e.control.web_read_only)throw Error('언리얼 웹 제어 옵션이 꺼져 있습니다.');
  if(this.lease&&this.lease.session!==id)throw Error('다른 웹 화면이 제어권을 사용 중입니다.');
  if(e.control.acquire_reason)throw Error(e.control.acquire_reason);
  if(!this.lease)this.lease={id:randomUUID(),session:id,until:this.now()+3000};return this.view(id);
 }
 view(id){
  const s=this.auth(id),p=this.refresh(),c=p.envelope?.control,desired=this.lease?.session===id;
  return {owns:!!(desired&&c?.owner==='web'&&c.lease_id===this.lease.id),pending:!!(desired&&c?.lease_id!==this.lease.id),lease_id:desired?this.lease.id:'',owner:c?.owner??'unreal',acquire_reason:c?.acquire_reason??'언리얼 연결 대기',records:[...this.records.values()].filter(r=>r.client===s.key).map(r=>this.public(r))};
 }
 public(r){const {client,fingerprint:hash,order,delivered,...v}=r;return structuredClone(v);}
 submit(id,c){
  const session=this.auth(id);validateCommand(c);const existing=this.records.get(requestKey(c.request_id))??this.retired.get(requestKey(c.request_id)),fp=fingerprint(c);
  if(existing){if(existing.client!==session.key||existing.fingerprint!==fp)throw Error('같은 요청 ID를 다른 내용·세션에서 사용할 수 없습니다.');return this.public(existing);}
  // Reserve the normal history limit for manual commands. Stops coalesce with an in-flight
  // stop and remain reachable at capacity; retired IDs are retained as non-executable receipts.
  if(this.records.size>=240&&c.kind!=='stop_all')throw Error('일반 요청 기록 한도 · 전체 정지 후 서버를 재시작하세요.');


  const p=this.refresh(),e=p.envelope,r={command:structuredClone(c),request_id:c.request_id,client:session.key,fingerprint:fp,status:'queued',message:'로컬 서버 접수 · 작업자 적용 전',order:++this.nextOrder,revision:null};
  let why='';
  if(p.connection.status!=='connected'||p.data_stale||e?.snapshot?.validity!=='valid')why='연결 없음 또는 최신 데이터 대기';
  else if(c.run_id!==e.run_id||c.epoch!==e.snapshot.epoch)why='이전 실행·초기화 세대 명령 거부';
  else if(e.control.web_read_only)why='조회 전용 연결';
  else if(c.kind!=='stop_all'&&!readKinds.includes(c.kind)&&(!this.lease||this.lease.session!==id||this.lease.id!==c.lease_id||e.control.lease_id!==c.lease_id))why='웹 제어권 없음 또는 적용 미확인';
  else if(c.kind!=='stop_all'&&!isBatch(c.kind)&&!['transfer_pressure','mix_rpm'].includes(c.kind)&&e.snapshot.batch.active)why='자동 배치 실행 중 · 일반 웹 수동 명령 차단';
  else if(c.kind!=='stop_all'&&[...this.records.values()].some(x=>x.command.kind==='stop_all'&&pending(x.status)))why='전체 정지 확인 중 · 완료 후 새 요청 필요';
  if(!why&&c.kind==='mix_rpm'){
   why=mixScopeReason(e,c);
   if(!why&&[...this.records.values()].some(x=>pending(x.status)&&x.command.kind==='mix_rpm'&&x.command.run_id===c.run_id&&
      x.command.epoch===c.epoch&&x.command.mix_run===c.mix_run&&x.command.batch_id===c.batch_id))why='혼합 속도 적용 확인 중';
  }
  if(!why&&c.kind==='transfer_pressure')why=transferScopeReason(e,c);
  if(!why&&c.kind==='transfer_pressure'&&[...this.records.values()].some(x=>x.command.kind==='transfer_pressure'&&x.command.run_id===c.run_id&&x.command.epoch===c.epoch&&x.command.batch_id===c.batch_id&&x.command.transfer_id===c.transfer_id&&x.command.stage_token===c.stage_token&&pending(x.status)))why='적용 확인 중 · 현재 압력 요청의 결과를 기다리세요.';
  if(!why&&isBatch(c.kind)){const w=e.control.batch_workflow;if(['batch_check','batch_resume','batch_abandon'].includes(c.kind)&&c.batch_id!==w?.batch_id)why='대상 배치 ID 변경 · 현재 상태를 확인하세요.';if(['recipe_apply','batch_start'].includes(c.kind)&&(c.recipe_revision!==w?.recipe_revision||c.recipe_fingerprint!==w?.recipe_fingerprint))why='적용 설정 버전 충돌 · 다시 확인하세요.';if(c.kind==='batch_resume'&&c.check_token!==w?.recovery?.check_token)why='상태 확인 이후 변경 · 배치 상태 확인을 다시 실행하세요.';}
  if(!why){const capability=e.control.actions?.find(a=>a.equipment===c.equipment&&a.kind===c.kind&&(!['valve','mixer_enabled','purge_enabled'].includes(c.kind)||a.value===c.value));if(!capability)why='서비스에서 지원하지 않는 명령';else if(capability.reason)why=capability.reason;}
  if(why){r.status='rejected';r.message=why;}
  if(!why&&c.kind==='stop_all')for(const q of this.records.values())if(q.status==='queued'){q.status='rejected';q.message='전체 정지 우선 · 대기 명령 취소';}
  if(c.kind==='stop_all'&&this.records.size>=256){const old=[...this.records.values()].find(x=>!pending(x.status));if(old){this.records.delete(requestKey(old.request_id));this.retired.set(requestKey(old.request_id),{request_id:old.request_id,client:old.client,fingerprint:old.fingerprint,status:'unknown',message:'압축된 이전 요청 · 재실행 금지. 현재 스냅샷을 확인하세요.'});}}
  this.records.set(requestKey(c.request_id),r);return this.public(r);
 }
 ingest(e){
  this.refresh();for(const update of e.control?.results??[]){const r=this.records.get(requestKey(update.request_id));if(!r||r.command.run_id!==e.run_id||r.command.epoch!==update.epoch||!r.delivered)continue;if(['accepted','processing','applied','rejected','failed','unknown'].includes(update.status))Object.assign(r,update,{request_id:r.request_id});}
 }
 frame(e){
  const p=this.refresh();if(p.connection.status!=='connected'||p.envelope?.publisher_id!==e.publisher_id)return {commands:[]};
  const commands=[];
  if(!p.data_stale&&!e.control.web_read_only){for(const r of [...this.records.values()].sort((a,b)=>(a.command.kind==='stop_all'?-1:0)-(b.command.kind==='stop_all'?-1:0)||a.order-b.order)){
   if(r.status!=='queued'||r.command.run_id!==e.run_id||r.command.epoch!==e.snapshot?.epoch)continue;
   if(r.command.kind!=='stop_all'&&!readKinds.includes(r.command.kind)&&r.command.lease_id!==this.lease?.id)continue;
   if(r.command.kind==='mix_rpm'){const why=mixScopeReason(e,r.command);if(why){r.status='rejected';r.message=why;continue;}}
   if(r.command.kind==='transfer_pressure'){const why=transferScopeReason(e,r.command);if(why){r.status='rejected';r.message=why;continue;}}
   r.delivered=true;r.status='processing';r.message='전달됨 · 서비스 접수·작업자 적용 확인 중';commands.push(r.command);break; // Bounded single delivery keeps recipe bodies + acknowledgements below native 32 KiB cap.
  }}
  return {observed_transfer:this.observedTransfer??null,result_acks:[...this.records.values()].filter(r=>r.delivered&&r.data).map(r=>r.request_id).slice(-256),server_id:this.store.serverId,run_id:e.run_id,epoch:e.snapshot?.epoch??'0',lease_id:this.lease?.id??'',lease_remaining_ms:this.lease?Math.max(0,this.lease.until-this.now()):0,commands};
 }
 result(id,request){const s=this.auth(id),r=this.records.get(requestKey(request))??this.retired.get(requestKey(request));return r&&r.client===s.key?this.public(r):{request_id:request,status:'unknown',message:'결과 기록 없음 · 이전 명령을 자동 재전송하지 않습니다.'};}
}
