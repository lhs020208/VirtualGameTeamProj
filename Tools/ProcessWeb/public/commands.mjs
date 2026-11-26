import {isBatch,readKinds,validateBatchCommand} from './recipes.mjs';
export const commandLabels={recipe_list:'레시피 목록',recipe_load:'레시피 불러오기',recipe_save:'레시피 저장',recipe_apply:'레시피 적용',batch_start:'자동 배치 시작',batch_check:'배치 상태 확인',batch_resume:'남은 양 이어서 투입',batch_abandon:'배치 포기',mix_rpm:'이번 혼합 RPM',transfer_pressure:'현재 구간 호퍼 압력',stop_all:'전체 정지',resume:'계산만 재개',pause:'계산 일시정지',valve:'밸브',pressure:'호퍼 압력',purge_pressure:'하류 연결부 공기 압력',purge_enabled:'하류 연결부 공기',rpm:'목표 RPM',mixer_enabled:'혼합기'};
export const statusLabels={queued:'요청 접수',accepted:'요청 접수',processing:'적용 확인 중',applied:'적용 완료',rejected:'거부',failed:'실패',unknown:'결과 확인 필요'};
export const pending=s=>['queued','accepted','processing'].includes(s);
const uuid=s=>typeof s==='string'&&/^[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}$/i.test(s)&&!/^0{8}-0{4}-0{4}-0{4}-0{12}$/.test(s);
const supply=s=>/^supply-[a-f]$/.test(s),purge=supply;
export function validateCommand(c){
 if(!c||c.command_version!==1||!uuid(c.request_id)||typeof c.run_id!=='string'||!c.run_id||c.run_id.length>128||typeof c.epoch!=='string'||!/^(0|[1-9]\d{0,18})$/.test(c.epoch)||BigInt(c.epoch)>9223372036854775807n||typeof c.value!=='number'||!Number.isFinite(c.value)||typeof c.lease_id!=='string'||c.lease_id.length>128)throw Error('요청 ID·실행·세대·수치 형식 오류');
 if(isBatch(c.kind)){validateBatchCommand(c);return c;}
 const {kind:k,equipment:e,value:v,unit:u}=c;let valid=false;
 if(['stop_all','resume','pause'].includes(k))valid=e==='plant'&&u==='none'&&v===0;
 else if(k==='valve')valid=(supply(e)||['weigher','receiver'].includes(e))&&u==='bool'&&[0,1].includes(v);
 else if(k==='mixer_enabled')valid=e==='mixer'&&u==='bool'&&[0,1].includes(v);
 else if(k==='purge_enabled')valid=purge(e)&&u==='bool'&&[0,1].includes(v);
 else if(k==='mix_rpm'){
  if(v===0)throw Error('정지하려면 전체 정지를 사용하세요.');
  valid=e==='mixer'&&u==='rpm'&&v>0&&v<=30&&uuid(c.batch_id)&&typeof c.mix_run==='string'&&/^[1-9]\d{0,18}$/.test(c.mix_run)&&BigInt(c.mix_run)<=9223372036854775807n;
 }
 else if(k==='rpm')valid=e==='mixer'&&u==='rpm'&&v>=0&&v<=30;
 else if(k==='transfer_pressure'){
  const scoped=c.stage_token!==undefined||c.supplier_id!==undefined;
  const target=scoped?uuid(c.stage_token)&&supply(c.supplier_id)&&(supply(e)||['weigher','receiver'].includes(e)):e==='receiver'&&c.transfer_id!=='0';
  valid=target&&u==='kPa(g)'&&v>=-1&&v<=(e==='receiver'?0:1)&&uuid(c.batch_id)&&typeof c.transfer_id==='string'&&/^(0|[1-9]\d{0,18})$/.test(c.transfer_id)&&BigInt(c.transfer_id)<=9223372036854775807n;
 }
 else if(k==='pressure')valid=(supply(e)||['weigher','receiver'].includes(e))&&u==='kPa(g)'&&v>=-1&&v<=(e==='receiver'?0:1);
 else if(k==='purge_pressure')valid=purge(e)&&u==='kPa(g)'&&v>=0&&v<=1;
 if(!valid)throw Error('지원하지 않는 대상·명령·단위 또는 범위 밖 입력입니다. 값은 보정하지 않습니다.');return c;
}
export const fingerprint=c=>JSON.stringify([c.run_id,c.epoch,c.equipment,c.kind,c.value,c.unit,c.recipe_json,c.file_id,c.file_version,c.recipe_revision,c.recipe_fingerprint,c.batch_id,c.check_token,c.confirm,c.transfer_id,c.stage_token,c.supplier_id,c.mix_run]);
export function commandReason(packet,control,equipment,kind,value=0){
 if(!packet||packet.connection?.status!=='connected')return '언리얼 연결 없음 · 연결 설정을 확인하세요.';
 if(packet.data_stale||packet.envelope?.snapshot?.validity!=='valid')return '최신 공정 데이터 대기 · 갱신 후 다시 확인하세요.';
 const state=packet.envelope?.control;
 if(!state||state.web_read_only)return '조회 전용 연결 · 언리얼을 -ProcessWebControl 옵션으로 연결하세요.';
 if(kind!=='stop_all'&&!readKinds.includes(kind)&&!control?.owns)return control?.pending?'제어권 적용 확인 중 · 응답을 기다리세요.':'제어권 없음 · 제어권 가져오기를 선택하세요.';
 const a=state.actions?.find(x=>x.equipment===equipment&&x.kind===kind&&(!['valve','mixer_enabled','purge_enabled'].includes(kind)||x.value===value));
 return a?a.reason:'시뮬레이션 미지원 · 지원되는 장비를 선택하세요.';
}

// Scope admission is repeated at server delivery and by the native service after observing the worker.
export function mixScopeReason(e,c){
 const mix=e?.control?.mix_rpm;
 if(!mix||!mix.mix_run||mix.mix_run==='0'||mix.mix_run!==c.mix_run||mix.batch_id!==c.batch_id||
    c.batch_id!==e.control.batch_workflow?.batch_id)return '배치 또는 혼합 운전 변경 · 현재 혼합기에서 다시 입력하세요.';
 return e.control.actions?.find(a=>a.kind==='mix_rpm'&&a.equipment==='mixer')?.reason??'현재 혼합 운전 확인 중';
}
export function transferScopeReason(e,c){
 const t=e?.control?.transfer_pressure;
 if(!t)return '이송 중 압력 조절 미지원 · 언리얼 연결 버전을 확인하세요.';
 if(c.batch_id!==t.batch_id||c.batch_id!==e.control.batch_workflow?.batch_id)return '배치 ID 변경 · 이전 배치 요청 거부';
 if(c.transfer_id!==t.transfer_id)return '이송 ID 변경 · 이전 이송 요청 거부';
 if(t.epoch!==e.snapshot?.epoch||t.sequence!==e.snapshot?.sequence)return '같은 확정 스냅샷의 압력 데이터 확인 대기';
 if(t.validity!=='valid')return '최신 이송 압력 데이터 대기';
 if(t.targets){
  if(!t.stage_token||c.stage_token!==t.stage_token||c.supplier_id!==t.supplier_id)return '자동 단계 또는 공급원이 변경되었습니다. 현재 구간을 확인하세요.';
  const target=t.targets[c.equipment];
  return target?.available?'':target?.reason||'선택한 장비의 자동 압력 조절 잠김';
 }
 if(c.stage_token!==undefined)return '대상 압력 단계 확인 필요';
 if(!t.available)return t.reason||'현재 이송 단계에서 압력 조절 불가';return '';
}
