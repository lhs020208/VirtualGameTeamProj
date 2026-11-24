import {operatorNumber} from './protocol.mjs';
import {webContext,reason,sendCommand} from './manual.mjs';
import {pending,statusLabels,transferScopeReason} from './commands.mjs';
const $=id=>document.getElementById(id),input=$('transfer-pressure-input'),button=$('transfer-pressure-apply');
let scope='',target=null,localMessage='',equipment='receiver';
const number=operatorNumber;
const value=()=>input.value.trim()===''?NaN:Number(input.value),valid=()=>Number.isFinite(value())&&value()>=-1&&value()<=(equipment==='receiver'?0:1);
const same=(c,e,t)=>c?.kind==='transfer_pressure'&&c.run_id===e?.run_id&&c.epoch===e?.snapshot?.epoch&&c.batch_id===t?.batch_id&&c.transfer_id===t?.transfer_id&&c.equipment===equipment&&c.stage_token===(t?.stage_token||undefined);
function render(){
 const {packet,selected,records,unavailable}=webContext(),e=packet?.envelope,rootState=e?.control?.transfer_pressure;
 equipment=selected;const t=rootState?.targets?{...rootState,...rootState.targets[selected]}:rootState;
 const visible=selected==='receiver'||rootState?.targets&&Object.hasOwn(rootState.targets,selected);
 $('transfer-pressure-panel').hidden=!visible;if(!visible){scope='';target=null;return;}
 $('pressure-section').textContent=(t?.section||'칭량 → 수취')+' · '+(selected==='receiver'?'수취호퍼':selected==='weigher'?'칭량호퍼':selected.slice(-1).toUpperCase()+' 공급')+' 압력';
 $('pressure-phase-note').textContent=t?.section?t.section+' · 이번 구간에만 적용하며 다음 구간은 레시피 기준값을 사용합니다.':'이번 수취 이송에만 적용합니다.';
 const key=[e?.run_id,e?.snapshot?.epoch,selected,t?.batch_id,t?.stage_token,t?.supplier_id,t?.transfer_id].join('|');
 if(scope!==key){scope=key;target=t?{batch_id:t.batch_id,transfer_id:t.transfer_id,...(t.targets?{stage_token:t.stage_token,supplier_id:t.supplier_id}:{})}:null;input.value=t?.applied_pa!==null&&t?.applied_pa!==undefined?String(t.applied_pa/1000):t?.recipe_pa!==null&&t?.recipe_pa!==undefined?String(t.recipe_pa/1000):'';localMessage='대상 변경 시 편집값을 새로 확인하세요.';}
 const fresh=!unavailable&&packet?.connection.status==='connected'&&!packet?.data_stale&&e?.snapshot?.validity==='valid'&&t?.validity==='valid'&&t.epoch===e.snapshot.epoch&&t.sequence===e.snapshot.sequence;
 const latest=[...records].reverse().find(r=>same(r.command,e,t)),outstanding=latest&&pending(latest.status);
 let why=reason(selected,'transfer_pressure',value());if(!why)why=transferScopeReason(e,{...target,equipment:selected});if(!why&&outstanding)why='적용 확인 중 · 현재 요청 결과를 기다리세요.';
 input.disabled=!!why;button.disabled=!!why||!valid();
 for(const id of ['transfer-pressure-minus','transfer-pressure-plus'])$(id).disabled=!!why;$('transfer-pressure-reason').textContent=why||'조절 가능 · 자동 이송을 유지하며 현재 구간의 선택 장비 압력만 바꿉니다.';
 $('transfer-pressure-input-error').textContent=valid()?'':(selected==='receiver'?'−1~0':'−1~+1')+' kPa(g)의 유한한 숫자를 입력하세요.';
 const metrics=[['현재 적용 압력','applied_pa','kPa(g)',1000,2],['레시피 기준 압력','recipe_pa','kPa(g)',1000,2],['요청 압력','requested_pa','kPa(g)',1000,2],['계산 이송 유량','flow_m3_s','m³/s',1,4]];
 const root=$('transfer-pressure-values');for(const [label,key,unit,scale,digits]of metrics){let row=root.querySelector(`[data-transfer-metric="${key}"]`);if(!row){row=document.createElement('div');row.dataset.transferMetric=key;row.append(document.createElement('dt'),document.createElement('dd'));root.append(row);}row.firstChild.textContent=label;const v=t?.[key];row.lastChild.textContent=(fresh?number(v===null||v===undefined?null:v/scale,digits):'갱신 중단 · 값 확인 필요')+(fresh&&v!==null&&v!==undefined?' '+unit:'');}
 let status=latest?`${statusLabels[latest.status]??latest.status} · 요청 ${number(latest.command.value)} kPa(g) · ${latest.message??''}`:(t?.status||'현재 구간 조절 요청 없음');
 if(latest?.status==='applied'&&fresh&&t.applied_pa!==latest.command.value*1000)status='작업자 응답 수신 · 최신 적용값 확인 중';
 if(!fresh)status='연결/최신 데이터 확인 필요 · 적용 결과를 추정하지 않습니다. '+(latest?'마지막 요청: '+status:'');
 const dirty=fresh&&valid()&&t?.applied_pa!=null&&value()*1000!==t.applied_pa;
 $('transfer-pressure-status').textContent=dirty&&!outstanding?'편집값 미적용 · '+status:status;
 $('transfer-pressure-sample').textContent=t?`확정 스냅샷 ${t.sequence} · 해석 ${number(t.simulation_seconds,6)}초 · 단계: ${t.stage}${t.flow_m3_s===null?' · '+(t.targets?'현재 구간 유량 미산출':t.flow_reason):''}`:'확정 압력·유량 스냅샷 확인 대기';
 $('transfer-pressure-scope').textContent=t?`배치 ${t.batch_id||'미확인'} / 이송 #${t.transfer_id} · ${localMessage}`:'지정된 자동 배치 연결 필요';
 $('transfer-pressure-mock-note').textContent=packet?.mock?'모의 데이터·모의 조작 · 유량 변화는 화면 시험 입력값이며 실제 물리 반응이 아닙니다.':'';
}
input.addEventListener('input',()=>{localMessage='편집값 · 적용 버튼을 눌러야 요청됩니다.';render();});input.addEventListener('keydown',e=>{if(e.key==='Enter')e.preventDefault();});input.addEventListener('wheel',e=>e.preventDefault(),{passive:false});
for(const [id,sign]of [['transfer-pressure-minus',-1],['transfer-pressure-plus',1]])$(id).onclick=()=>{if(Number.isFinite(value())){input.value=String(Number((value()+sign*.1).toPrecision(15)));input.dispatchEvent(new Event('input'));}};
button.onclick=async()=>{if(button.disabled||!target||!valid())return;await sendCommand('transfer_pressure',equipment,value(),'kPa(g)',{...target});render();};
document.addEventListener('process-control',render);render();
