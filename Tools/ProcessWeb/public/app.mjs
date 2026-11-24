import './transfer-pressure.mjs';
import './batch.mjs';
import {updateManual} from './manual.mjs';
import {BrowserCursor,displayMetric} from './protocol.mjs';
const $=id=>document.getElementById(id),cursor=new BrowserCursor();let packet=null,selected='receiver',unavailable='',controller=null,stopped=false,timer;
const labels={target:'목표량',final:'확정 계량량',connector_mass:"선택 공급 연결부 잔량",connector_particles:"선택 공급 연결부 입자 수",valve_requested:'밸브 요청 상태',valve_applied:'밸브 실제 적용 상태',mass:'현재 모델 질량',particles:'현재 입자 수',initial_particles:'초기 장입 입자 수',pressure_requested:'호퍼 요청 압력',pressure_applied:'호퍼 적용 압력 설정',weigher_pressure_applied:'칭량호퍼 적용 압력 설정',valve_before:'밸브 직전 압력 · 모델 추정',valve_after:'밸브 직후 압력 · 모델 추정',valve_difference:'밸브 앞뒤 구간 차압 · 직전 − 직후',flow:'계산 이송 유량',purge_requested:'연결부 공기 요청',purge_applied:'연결부 공기 실제 적용',purge_pressure_requested:'연결부 공기 요청 압력',purge_pressure_applied:'연결부 공기 적용 압력',purge_flow:'하류 독립 공기 유량',rpm_target:'목표 RPM',rpm_applied:'계산 적용 RPM'};
const unknown={value:null,unit:'kg',validity:'unknown',source:'모델 계산값',reason:'확정 상태 수신 대기'};
const blank=()=>[...Array.from({length:6},(_,i)=>({id:`supply-${String.fromCharCode(97+i)}`,kind:'supplier',label:`공급 ${String.fromCharCode(65+i)}`,metrics:{mass:unknown}})),...['weigher','receiver','mixer'].map((kind,i)=>({id:kind,kind,label:['칭량호퍼 WH101','수취호퍼 RH-201','혼합기 MX-301'][i],metrics:{mass:unknown}}))];
function el(tag,cls,text){const e=document.createElement(tag);if(cls)e.className=cls;if(text!==undefined)e.textContent=text;return e;}
function show(m){return displayMetric(m,{mass:m?.unit==='kg'});}
function textValue(m){const v=show(m);return `${v.text}${v.unit?' '+v.unit:''}`;}
function render(){
 const s=packet?.envelope?.snapshot,old=!!unavailable||packet?.data_stale||s?.validity==='stale';document.body.classList.toggle('data-old',!!s&&old);
 $('mock-badge').hidden=!packet?.mock;
 const cs=unavailable?'disconnected':packet?.connection.status??'waiting';const connected=cs==='connected';
 $('connection').className='badge '+(connected?'connected':['disconnected','ambiguous'].includes(cs)?'disconnected':'waiting');
 $('connection').textContent=connected?'통신 연결':cs==='disconnected'?'연결 끊김':cs==='ambiguous'?'대상 확인 필요':'연결 대기';
 $('stage').textContent=s?s.batch.label??s.batch.stage:'언리얼 연결 대기';$('stage-reason').textContent=s?s.batch.reason:'지정된 해석 Actor의 확정 상태가 도착하면 표시합니다.';
 $('simulation-time').textContent=s&&s.simulation_seconds!==null?new Intl.NumberFormat('ko-KR',{maximumFractionDigits:3}).format(s.simulation_seconds):'—';
 const execution={running:'계산 실행 중',paused:'계산 일시정지',initializing:'계산 초기화 중',pauserequested:'일시정지 확인 중',resumerequested:'재개 확인 중',fault:'계산 오류',unavailable:'계산 상태 미확인'};
 $('execution').textContent=s?(execution[s.execution]??s.execution)+(packet?.simulation_progress==='not_advancing'?' · 해석 시간 진행 미확인':''):'계산 상태 미확인';
 $('owner').textContent=s?'언리얼 · 웹은 조회 전용':'언리얼 · 조회 연결 대기';
 const error=s?.error||((s?.validity==='error'||s?.execution==='fault')?'작업자 데이터 오류 · 상세 사유 미제공':'')||s?.batch.block_reason;let title=unavailable||packet?.connection.reason||'언리얼 연결 대기',action=packet?.connection.next_action||'로컬 서버와 대상 레벨 연결 설정을 확인하세요.';
 if(connected&&!error&&!old){title=s?.execution==='paused'?'통신 정상 · 계산 일시정지':'확정 스냅샷 조회 중';action='장비를 선택하고 제어권을 가져온 뒤 명시적인 조작 버튼을 사용하세요. 조회·선택은 장비를 움직이지 않습니다.';}
 if(connected&&s&&s.validity!=='valid'&&!old&&!error){title='통신 정상 · 공정 데이터 확인 중';action='데이터 유효성: '+s.validity+' · 언리얼의 작업자 상태와 초기화를 확인하세요.';}
 if(old){title=unavailable||'오래된 데이터 · 현재 상태 아님';action='마지막 수신값입니다. 언리얼 연결과 스냅샷 갱신을 확인하세요.';}
 if(error&&connected){title=s?.error||s?.validity==='error'||s?.execution==='fault'?'계산 오류':'현재 차단 사유';action=error;}
 $('notice').className='notice '+(error?'fault':old||cs==='ambiguous'?'warn':connected?'live':'');$('notice-title').textContent=title;$('notice-action').textContent=action;
 const eq=s?.equipment??blank();if(!eq.some(e=>e.id===selected))selected=eq[0]?.id;renderDiagram(eq);renderDetail(eq.find(e=>e.id===selected),s);updateManual(packet,selected,unavailable);
 $('regions').replaceChildren(...(s?.regions??[{id:'transfer-pipe',label:'칭량–수취 배관',metrics:{mass:unknown}}]).map(r=>{const d=el('div','region');d.append(el('span','',r.label),el('strong','',textValue(r.metrics.mass)));return d;}));
 $('material-results').replaceChildren(...(s?.batch.materials??[]).map(m=>{const tr=el('tr');tr.append(el('td','',m.label),...['target','final','receiver','mixer'].map(k=>el('td','',textValue(m.metrics[k]))));return tr;}));
 if(!s)$('material-results').append(el('tr','', '확정 배치 데이터 수신 대기'));
 $('block-reason').textContent=s?.batch.block_reason?`현재 차단 사유 · ${s.batch.block_reason}`:s?.batch.stop_reason&&s.batch.stop_stage!=='none'?s.batch.stop_reason:'';
 const hb=packet?.connection.heartbeat_age_ms;$('transport-note').textContent=unavailable?'로컬 연결 재시도 중':packet?`로컬 조회 정상 · ${hb===null?'언리얼 하트비트 대기':`마지막 하트비트 ${(hb/1000).toFixed(1)}초 전`}`:'로컬 서버 조회 준비 중';
}
function renderDiagram(eq){
 const container=$('diagram'),sup=eq.filter(e=>e.kind==='supplier'),width=Math.max(container.clientWidth,640,sup.length*106);container.style.minWidth=Math.max(640,sup.length*106)+'px';
 const compact=innerWidth<=1350,H=compact?340:420,sy=compact?106:124,bar=compact?132:154,hy=compact?158:190,hh=compact?84:108,my=compact?264:330,mh=compact?64:78;container.style.height=H+'px';
 const svg=$('pipes');svg.style.height=H+'px';svg.setAttribute('viewBox',`0 0 ${width} ${H}`);svg.setAttribute('preserveAspectRatio','none');
 const wx=width*.29,rx=width*.76,cardWidth=Math.min(174,width*.24),positions=new Map();let lines=[];
 sup.forEach((e,i)=>{const x=(i+.5)*width/sup.length;positions.set(e.id,{x,y:16,w:Math.min(145,width/sup.length-13),h:sy-16});lines.push(`M ${x} ${sy} V ${bar}`);});
 if(sup.length){lines.push(`M ${width/(2*sup.length)} ${bar} H ${width-width/(2*sup.length)}`,`M ${wx} ${bar} V ${hy}`);}
 for(const [kind,x,y,h] of [['weigher',wx,hy,hh],['receiver',rx,hy,hh],['mixer',rx,my,mh]]){const e=eq.find(e=>e.kind===kind);if(e)positions.set(e.id,{x,y,w:cardWidth,h});}
 lines.push(`M ${wx+cardWidth/2} ${hy+hh/2} H ${rx-cardWidth/2}`,`M ${rx} ${hy+hh} V ${my}`);
 svg.replaceChildren(...lines.map(d=>{const p=document.createElementNS('http://www.w3.org/2000/svg','path');p.setAttribute('d',d);return p;}));
 const focused=document.activeElement?.dataset.equipment,existing=new Map([...$('equipment').children].map(n=>[n.dataset.equipment,n]));const nodes=eq.filter(e=>positions.has(e.id)).map(e=>{
 const p=positions.get(e.id),b=existing.get(e.id)??el('button','device');b.type='button';b.dataset.equipment=e.id;b.setAttribute('aria-pressed',String(e.id===selected));b.setAttribute('aria-label',`${e.label} 상태 상세`);Object.assign(b.style,{left:p.x+'px',top:p.y+'px',width:p.w+'px',height:p.h+'px'});
 const v=show(e.kind==='mixer'?e.metrics.rpm_applied:e.metrics.mass),value=el('div','device-data',v.text);value.append(el('small','',v.unit));const name=b.querySelector('.device-name')??el('div','device-name');name.textContent=e.label;const oldValue=b.querySelector('.device-data');if(oldValue)oldValue.replaceChildren(...value.childNodes);else b.append(value);if(!name.parentElement)b.prepend(name);
 if(e.kind!=='mixer'){const m=e.metrics.valve_applied,open=m?.validity==='valid'&&m.value===1;const state=b.querySelector('.device-state')??el('div','device-state');state.className='device-state'+(open?' is-open':'');state.replaceChildren();state.append(el('i','dot '+(open?'live':'closed')),document.createTextNode(m?textValue(m):'상태 미확인'));if(e.metrics.particles?.validity==='valid'&&e.metrics.particles.value>0&&e.metrics.mass?.value>0&&e.metrics.mass.value<.00001)state.append(document.createTextNode(` · ${e.metrics.particles.value}개 남음`));b.append(state);}
 b.onclick=()=>{selected=e.id;render();};return b;});
 if(nodes.length!==$('equipment').children.length||nodes.some((n,i)=>n!==$('equipment').children[i]))$('equipment').replaceChildren(...nodes);if(focused)nodes.find(n=>n.dataset.equipment===focused)?.focus({preventScroll:true});
}
function renderDetail(e,s){
 $('detail-title').textContent=e?.label??'장비 상세';
 const stale=unavailable||packet?.data_stale||s?.validity!=='valid';
 const headline=$('equipment-summary');
 headline.hidden=e?.kind!=='supplier';
 if(!headline.hidden){
  const stage=s?.batch?.label||s?.batch?.stage||'시작 전';
  const val=k=>stale?'미확인':textValue(e.metrics[k]);
  headline.textContent=`${e.role||'역할 미확인'} · ${stage} · 목표 ${val('target')} · 확정 ${val('final')}`;
 }$('detail-note').textContent=e?.kind==='supplier'?'호퍼 압력은 적용 경계 설정입니다. 밸브 앞뒤 압력과 구간 차압은 모델 추정값이며 센서 측정값이 아닙니다.':'질량은 현재 구역의 모델 계산량입니다. 적용 압력은 경계 설정이며 계산 RPM은 작업자가 확인한 값입니다.';
 const previous=new Map([...$('detail-values').children].map(n=>[n.dataset.metric,n]));
 const rows=Object.entries(e?.metrics??{}).map(([k,m])=>{const d=previous.get(k)??el('div','metric');d.dataset.metric=k;const v=show(m),dt=d.querySelector('dt')??el('dt'),dd=d.querySelector('dd')??el('dd');dt.textContent=labels[k]??k;dd.className=m.validity==='valid'?'':'uncertain-text';dd.replaceChildren(document.createTextNode(v.text),el('small','',v.unit));if(!dt.parentElement)d.append(dt,dd);let reason=d.querySelector('p');if(m.reason){if(!reason){reason=el('p');d.append(reason);}reason.textContent=m.reason;}else reason?.remove();return d;});
 if(rows.length!==$('detail-values').children.length||rows.some((n,i)=>n!==$('detail-values').children[i]))$('detail-values').replaceChildren(...rows);
 const env=packet?.envelope,identity={레벨:env?.level??'미연결',Actor:env?.actor??'미연결','실제 Actor 경로':s?.actual_actor_path??'미확인','실행 ID':env?.run_id??'미확인','초기화 세대':s?.epoch??'미확인','스냅샷 순서':s?.sequence??'미확인','데이터 유효성':s?.validity??'미확인'};
 $('identity').replaceChildren(...Object.entries(identity).map(([k,v])=>{const d=el('div');d.append(el('dt','',k),el('dd','',v));return d;}));
}
async function poll(){
 if(stopped)return;controller=new AbortController();const timeout=setTimeout(()=>controller?.abort(),1800);
 try{const r=await fetch('/api/state',{signal:controller.signal,cache:'no-store',credentials:'omit'});if(!r.ok)throw Error(`로컬 조회 오류 (${r.status})`);const p=await r.json();if(!cursor.accept(p))throw Error('이전 실행 또는 역전된 상태 메시지 거부');packet=p;unavailable='';}
 catch(e){unavailable=e.name==='AbortError'?'로컬 서버 응답 시간 초과':e.message==='Failed to fetch'?'로컬 서버 연결 끊김':e.message;}
 finally{clearTimeout(timeout);controller=null;if(!stopped){render();timer=setTimeout(poll,250);}}
}
window.addEventListener('pagehide',()=>{stopped=true;clearTimeout(timer);controller?.abort();});window.addEventListener('pageshow',()=>{if(stopped){stopped=false;poll();}});
new ResizeObserver(()=>{if(packet)renderDiagram(packet.envelope?.snapshot?.equipment??blank());}).observe($('diagram-scroll'));
render();poll();
