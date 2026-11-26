import {operatorNumber} from './protocol.mjs';
import {validateCommand,commandReason,statusLabels,commandLabels,pending} from './commands.mjs';
const $=id=>document.getElementById(id),make=(tag,text)=>{const e=document.createElement(tag);if(text!==undefined)e.textContent=text;return e;};
let packet=null,selected='',unavailable='',session='',state=null,built='',error='',server='',busy=false,stopped=false;
let key=sessionStorage.getItem('process-client');if(!key){key=crypto.randomUUID();sessionStorage.setItem('process-client',key);}
let remembered;try{remembered=JSON.parse(sessionStorage.getItem('process-requests')??'[]');}catch{remembered=[];}
const records=new Map(remembered.map(r=>[r.request_id,{...r,status:pending(r.status)?'unknown':r.status,message:pending(r.status)?'화면 재접속 · 결과 조회 대기':r.message}]));
const remember=()=>sessionStorage.setItem('process-requests',JSON.stringify([...records.values()].slice(-32)));
async function api(url,body={}){const abort=new AbortController(),t=setTimeout(()=>abort.abort(),1800);try{const r=await fetch('/api/'+url,{method:'POST',headers:{'Content-Type':'application/json',...(session?{'X-Process-Session':session}:{})},body:JSON.stringify(body),signal:abort.signal,credentials:'omit'});const j=await r.json();if(!r.ok){const error=Error(j.error??'요청 실패');error.httpStatus=r.status;throw error;}return j;}finally{clearTimeout(t);}}
async function connect(){const j=await api('session',{client_key:key});session=j.session;server=j.server_id;state=null;for(const r of remembered){if(r.request_id){const result=await api('result',{request_id:r.request_id});records.set(r.request_id,{...r,...result});}}remember();}
let observedTransfer=null;export const acknowledgeTransfer=o=>{observedTransfer=o;};
async function heartbeat(){if(stopped)return;try{if(!session)await connect();state=await api('heartbeat',{observed_transfer:observedTransfer});for(const r of state.records)records.set(r.request_id,r);error='';remember();}catch(e){session='';state=null;error=e.message;for(const r of records.values())if(pending(r.status)){r.status='unknown';r.message='연결 끊김 · 적용 결과 확인 필요. 자동 재전송하지 않습니다.';}remember();}render();if(!stopped)setTimeout(heartbeat,750);}
export function reason(e,k,v){if(unavailable)return '연결 끊김 · 현재 상태와 요청 결과를 다시 확인하세요.';if(!session)return '브라우저 세션 연결 대기';return commandReason(packet,state,e,k,v);}
export async function sendCommand(kind,equipment,value,unit,extra={}){
 const previous=[...records.values()].find(r=>pending(r.status)&&r.command?.kind===kind&&r.command?.equipment===equipment&&r.command?.value===value&&r.command?.run_id===packet?.envelope?.run_id&&r.command?.epoch===packet?.envelope?.snapshot?.epoch&&(kind!=='mix_rpm'||r.command.mix_run===extra.mix_run&&r.command.batch_id===extra.batch_id)&&(kind!=='transfer_pressure'||r.command?.batch_id===extra.batch_id&&r.command?.transfer_id===extra.transfer_id&&r.command?.stage_token===extra.stage_token));if(previous){error='같은 요청 적용 확인 중 · 중복 발행하지 않습니다.';render();return;}
 const why=reason(equipment,kind,value);if(why){error=why;render();return;}
 const c={command_version:1,request_id:crypto.randomUUID(),run_id:packet.envelope.run_id,epoch:packet.envelope.snapshot.epoch,lease_id:state?.lease_id??'',equipment,kind,value,unit,...extra};
 try{validateCommand(c);}catch(e){error=e.message;render();return;}
 const r={request_id:c.request_id,command:c,status:'queued',message:'요청 중 · 서버 접수 확인 전'};records.set(c.request_id,r);remember();render();
 try{Object.assign(r,await api('command',c));error='';}catch(e){r.status=e.httpStatus?'rejected':'unknown';r.message=e.message+(e.httpStatus?' · 서버 거부 확인':' · 결과 조회 필요, 자동 재전송 없음');}remember();render();return r;
}
function button(text,e,k,v,u){const wrap=make('div'),b=make('button',text),why=make('p');b.type='button';b.dataset.command=k;b.dataset.value=String(v);wrap.className='manual-action';why.className='action-reason';b.onclick=()=>sendCommand(k,e,v,u);wrap.append(b,why);return {wrap,b,why,update(){const r=reason(e,k,v);b.disabled=!!r;why.textContent=r;}};}
const controls=[];
function number(e,kind,label,unit,step,min,max){
 const box=make('div'),l=make('label',label),row=make('div'),input=make('input'),minus=make('button','−'),plus=make('button','+'),u=make('span',unit),local=make('p'),applied=make('button','적용');box.className='manual-field';row.className='number-row';input.type='text';input.inputMode='decimal';input.id=`manual-${kind}`;input.setAttribute('aria-label',label);l.htmlFor=input.id;
 const metric=packet?.envelope?.snapshot?.equipment.find(d=>d.id===e)?.metrics[kind==='rpm'?'rpm_target':kind==='pressure'?'pressure_requested':'purge_pressure_requested'];input.value=metric?.validity==='valid'?String(metric.value/(unit==='kPa(g)'?1000:1)):'';
 const parsed=()=>input.value.trim()===''?NaN:Number(input.value),valid=()=>Number.isFinite(parsed())&&parsed()>=min&&parsed()<=max;
 minus.type=plus.type=applied.type='button';minus.setAttribute('aria-label',label+' 감소');plus.setAttribute('aria-label',label+' 증가');
 for(const [b,sign] of [[minus,-1],[plus,1]])b.onclick=()=>{if(Number.isFinite(parsed())){input.value=String(Number((parsed()+sign*step).toPrecision(15)));input.dispatchEvent(new Event('input'));}};
 input.addEventListener('keydown',e=>{if(e.key==='Enter')e.preventDefault();});input.addEventListener('wheel',e=>e.preventDefault(),{passive:false});
 const update=()=>{const r=reason(e,kind,0);applied.disabled=!!r||!valid();local.textContent=r||(!valid()?`${min}~${max} ${unit} 범위의 숫자를 입력하세요. 자동 보정하지 않습니다.`:'편집값 · 적용 버튼을 눌러야 요청됩니다.');};
 input.oninput=update;applied.dataset.command=kind;applied.onclick=()=>sendCommand(kind,e,parsed(),unit);local.className='action-reason';row.append(minus,input,plus,u);box.append(l,row,applied,local);controls.push({update});return box;
}
function mixSpeed(scope){
 const box=make('div'),label=make('label','이번 혼합 속도'),row=make('div'),input=make('input'),apply=make('button','RPM 적용');
 const summary=make('p'),feedback=make('p');
 box.className='manual-field';row.className='number-row';input.type='text';input.inputMode='decimal';input.id='live-mix-rpm';label.htmlFor=input.id;
 input.value=Number.isFinite(scope.applied_rpm)?String(scope.applied_rpm):'';
 const target={batch_id:scope.batch_id,mix_run:scope.mix_run};
 const value=()=>input.value.trim()?Number(input.value):NaN;
 function update(){
  const mix=packet?.envelope?.control?.mix_rpm;
  const actual=!unavailable&&!packet?.data_stale&&Number.isFinite(mix?.applied_rpm)?operatorNumber(mix.applied_rpm)+' RPM':'미확인';
  const remaining=packet?.envelope?.control?.batch_workflow?.mixing_remaining_s;
  summary.textContent=`현재 적용 ${actual} · 레시피 ${operatorNumber(mix?.recipe_rpm)} RPM · 남은 혼합 ${operatorNumber(unavailable||packet?.data_stale?null:remaining)}초`;
  label.textContent='이번 혼합에 적용할 RPM';
  const queued=[...records.values()].some(r=>pending(r.status)&&r.command?.kind==='mix_rpm'&&r.command.batch_id===target.batch_id&&r.command.mix_run===target.mix_run);
  const why=reason('mixer','mix_rpm',value())||(queued?'RPM 적용 확인 중':'')||
   (value()===0?'정지하려면 전체 정지를 사용하세요.':!Number.isFinite(value())||value()<0||value()>30?'0 초과 30 이하 RPM을 입력하세요.':'');
  feedback.textContent=[why||(value()!==mix?.applied_rpm?'편집값 미적용':''),mix?.message].filter(Boolean).join(' · ');apply.disabled=!!why;
 }
 for(const step of [-1,1]){
  const b=make('button',step<0?'−':'+');b.type='button';b.onclick=()=>{if(Number.isFinite(value())){input.value=String(value()+step);update();}};
  if(step<0)row.append(b,input);else row.append(b);
 }
 row.append(make('span','RPM'));apply.type='button';apply.dataset.command='mix_rpm';
 apply.onclick=()=>{if(!apply.disabled)sendCommand('mix_rpm','mixer',value(),'rpm',target);};
 input.oninput=update;input.onkeydown=e=>{if(e.key==='Enter')e.preventDefault();};
 box.append(label,summary,row,apply,feedback);controls.push({update});return box;
}
function buildDetail(){const root=$('manual-detail');const env=packet?.envelope,mix=selected==='mixer'&&env?.snapshot?.batch?.active?env.control?.mix_rpm:null;
 const identity=JSON.stringify([selected,env?.run_id,env?.snapshot?.epoch,mix?.batch_id,mix?.mix_run]);
 if(!root||built===identity)return;built=identity;root.replaceChildren();controls.length=0;const e=packet?.envelope?.snapshot?.equipment.find(d=>d.id===selected);if(!e){root.append(make('p','장비 연결 후 지원 조작이 표시됩니다.'));built='';return;}
 root.append(make('h3','선택 장비 수동 조작'));
 function add(text,k,v,u){const c=button(text,e.id,k,v,u);controls.push(c);root.append(c.wrap);}
 if(e.kind==='mixer'&&mix){root.append(mixSpeed(mix));}
 else if(e.kind==='mixer'){root.append(number(e.id,'rpm','목표 RPM 설정','rpm',1,0,30));add('혼합 시작','mixer_enabled',1,'bool');add('혼합 정지','mixer_enabled',0,'bool');root.append(make('p','목표 RPM 적용과 혼합 시작은 별도 조작입니다. 적용 RPM은 아래 확정 상태에서 확인하세요.'));}
 else{add('밸브 열기','valve',1,'bool');add('밸브 닫기','valve',0,'bool');root.append(number(e.id,'pressure',e.kind==='supplier'?'공급 A~F 공통 호퍼 압력':'호퍼 압력','kPa(g)',.1,-1,e.id==='receiver'?0:1));
 if(e.kind==='supplier'){root.append(make('h4','선택 공급호퍼 하류 연결부 공기'));root.append(number(e.id,'purge_pressure','연결부 공기 압력','kPa(g)',.1,0,1));add('연결부 공기 시작','purge_enabled',1,'bool');add('연결부 공기 정지','purge_enabled',0,'bool');root.append(make('p','호퍼 압력과 별도 설정 · 선택 호퍼 A~F, 공급 밸브·칭량 출구 닫힘 등 기존 서비스 조건을 확인합니다.'));}}
 root.append(make('p','요청값과 실제 적용값은 아래 확정 상태에 별도로 표시됩니다. 게이트 점유 중에는 실제 닫힘 확인을 기다립니다.'));
}
const globals=[];
export const webContext=()=>({packet,state,selected,records:[...records.values()],unavailable,session:!!session});
function render(){
 document.dispatchEvent(new Event('process-control'));
 if(!$('manual-detail'))return;buildDetail();for(const c of [...globals,...controls])c.update();
 $('owner').textContent=state?.owns?'웹 · 이 화면':state?.pending?'웹 제어권 적용 확인 중':packet?.envelope?.control?.owner==='web'?'웹 · 다른 화면 또는 반환 확인 중':'언리얼';
 $('acquire').disabled=!session||!!unavailable||state?.owns||state?.pending||packet?.connection.status!=='connected'||packet?.data_stale||packet?.envelope?.control?.web_read_only||!!state?.acquire_reason;
 $('release').disabled=!(state?.owns||state?.pending);$('authority-reason').textContent=error||(!session?'브라우저 연결 확인 중':state?.owns?'이 웹 화면이 조작합니다. 전체 정지는 언리얼에서도 가능합니다.':state?.pending?'언리얼 서비스의 제어권 적용 응답을 기다립니다.':state?.acquire_reason||'제어권 가져오기를 선택하세요. 장비는 움직이지 않습니다.');
 $('pause-explanation').textContent=packet?.envelope?.control?.pause_policy==='CloseThenPause'?'계산 일시정지는 공기 차단·압력 0·밸브 닫힘·RPM 0 확인 후 계산을 멈추는 기존 전체 정지 절차입니다.':'일시정지 정책: '+(packet?.envelope?.control?.pause_policy??'확인 대기');
 const batch=packet?.envelope?.snapshot?.batch;
 const autoOperating=batch?.active&&['none','complete'].includes(batch.stop_stage);
 $('stop-detail').textContent=autoOperating?'자동 배치 운전 중 · 전체 정지 가능':[batch?.stop_reason,packet?.envelope?.control?.unconfirmed].filter(Boolean).join(' · ');
 $('requests').replaceChildren(...[...records.values()].slice(-8).reverse().map(r=>{const d=make('div');d.className='request '+r.status;const c=r.command;d.append(make('strong',`${statusLabels[r.status]??r.status} · ${c?commandLabels[c.kind]+' / '+c.equipment:'이전 요청'}`),make('p',r.message??''),make('small',`ID ${r.request_id} · 서비스 번호 ${r.revision??'미확인'}${c?` · 요청값 ${c.value} ${c.unit}`:''}`));return d;}));
}
export function updateManual(p,id,offline){packet=p;selected=id;unavailable=offline;if(session&&server&&p?.server_id&&server!==p.server_id){session='';state=null;}render();}
for(const [id,label,k] of [['stop-action','전체 정지','stop_all'],['resume-action','계산만 재개','resume'],['pause-action','계산 일시정지','pause']]){const c=button(label,'plant',k,0,'none');c.b.id=k==='stop_all'?'global-stop':k;$(id).append(c.wrap);globals.push(c);}
$('acquire').onclick=async()=>{try{state=await api('acquire');error='';}catch(e){error=e.message;}render();};$('release').onclick=async()=>{try{state=await api('release');error='';}catch(e){error=e.message;}render();};
$('refresh-results').onclick=async()=>{for(const r of records.values())if(r.status==='unknown'||pending(r.status)){try{Object.assign(r,await api('result',{request_id:r.request_id}));}catch(e){error=e.message;}}remember();render();};
window.addEventListener('pagehide',()=>{stopped=true;if(session)fetch('/api/release',{method:'POST',headers:{'Content-Type':'application/json','X-Process-Session':session},body:'{}',keepalive:true}).catch(()=>{});});
window.addEventListener('pageshow',()=>{if(stopped){stopped=false;session='';state=null;heartbeat();}});heartbeat();
