export const VERSION = 1;
export const LEVEL = '/Game/ProcessPlant_20261111_LowPoly/L_ProcessPlant_LowPoly_1111';
export const ACTOR = `${LEVEL}.L_ProcessPlant_LowPoly_1111:PersistentLevel.PowderPlantTest_0`;
export const VALIDITIES = ['valid','unknown','initializing','stale','source_missing','unsupported','error','not_computed'];
const units = new Set(['kg','particles','Pa(g)','Pa','m3/s','rpm','s','bool']);
const decimal = s => typeof s === 'string' && /^(0|[1-9]\d{0,18})$/.test(s);
const string = (s,n=2048) => typeof s === 'string' && s.length <= n;
const finite = n => typeof n === 'number' && Number.isFinite(n);
export function validateEnvelope(e) {
  if (!e || e.schema_version !== VERSION || !string(e.publisher_id,128) || !e.publisher_id || !string(e.run_id,128) || !e.run_id || !decimal(e.publication_sequence)) throw Error('지원하지 않는 규격·실행 식별자·발행 순서');
  if (!e.control||!['unreal','web'].includes(e.control.owner)||typeof e.control.web_read_only!=='boolean'||typeof e.control.transfer_supported!=='boolean')throw Error('제어권 형식 오류');
  if(!e.control.web_read_only&&(e.control.command_version!==1||!Array.isArray(e.control.actions)||!Array.isArray(e.control.results)||e.control.actions.length>128||e.control.results.length>256||typeof e.control.lease_id!=='string'))throw Error('수동 제어 규격 오류');
  if (e.level !== LEVEL || e.actor !== ACTOR) throw Error('지정된 레벨·Actor와 일치하지 않음');
  if (!['connected','waiting','disconnected'].includes(e.binding?.status) || !string(e.binding.reason) || !string(e.binding.next_action)) throw Error('연결 상태 형식 오류');
  if (e.binding.status !== 'connected') { if (e.snapshot !== null) throw Error('연결 대기 상태에는 이전 스냅샷을 전달할 수 없음'); return e; }
  const s=e.snapshot;
  if (!s || !decimal(s.epoch) || !decimal(s.sequence) || !(s.simulation_seconds===null&&s.validity!=='valid'||finite(s.simulation_seconds)&&s.simulation_seconds>=0) || !finite(s.age_ms) || s.age_ms<0 || !VALIDITIES.includes(s.validity) || !['unavailable','initializing','running','paused','pauserequested','resumerequested','fault'].includes(s.execution) || !string(s.error) || !Array.isArray(s.equipment) || s.equipment.length>64 || !Array.isArray(s.regions) || !s.batch || !string(s.batch.stage) || !string(s.batch.reason) || !Array.isArray(s.batch.materials)) throw Error('확정 스냅샷 형식 오류');
  const mix=e.control.mix_rpm;
  if(mix!==undefined){
    if(!mix||mix.epoch!==s.epoch||mix.sequence!==s.sequence||!decimal(mix.mix_run)||!string(mix.batch_id,36))
      throw Error('혼합 RPM의 작업자 스냅샷 또는 운전 식별자가 잘못되었습니다.');
    for(const k of ['recipe_rpm','applied_rpm','applied_seconds','start_angle_rad'])
      if(mix[k]!==undefined&&!finite(mix[k]))throw Error('혼합 RPM 또는 적용 시각·각도가 잘못되었습니다.');
  }
  const t=e.control.transfer_pressure;
  if(t){
    if(!string(t.batch_id,36)||!decimal(t.transfer_id)||t.epoch!==s.epoch||t.sequence!==s.sequence||t.simulation_seconds!==s.simulation_seconds||!VALIDITIES.includes(t.validity)||typeof t.available!=='boolean'||typeof t.pending!=='boolean'||!string(t.reason)||!string(t.flow_reason)||!string(t.status)||!string(t.stage)||!string(t.service_request_id,36))throw Error('이송 압력의 스냅샷·대상 형식 불일치');
    for(const k of ['recipe_pa','requested_pa','applied_pa','flow_m3_s','requested_s','applied_s'])if(t[k]!==null&&!finite(t[k]))throw Error('이송 압력·유량 수치 오류');
    if(t.targets!==undefined){
      const ids=[...'abcdef'].map(k=>'supply-'+k).concat('weigher','receiver');
      if(!t.targets||Object.keys(t.targets).length!==ids.length||!string(t.stage_token,36)||!string(t.supplier_id,12)||!string(t.section))throw Error('자동 압력 구간 형식 오류');
      for(const id of ids){
        const target=t.targets[id];
        if(!target||typeof target.available!=='boolean'||!string(target.reason)||!string(target.status))throw Error('장비별 압력 상태 오류');
        for(const key of ['recipe_pa','requested_pa','applied_pa','flow_m3_s'])if(target[key]!==null&&!finite(target[key]))throw Error('장비별 압력·유량 형식 오류');
        if(target.available&&(!t.stage_token||!t.batch_id||t.validity!=='valid'||!/^supply-[a-f]$/.test(t.supplier_id)||
          (t.transfer_id==='0'?id!==t.supplier_id&&id!=='weigher':id!=='receiver'&&id!=='weigher')))throw Error('압력 조절 구간과 장비 불일치');
      }
    }
    if(t.available&&(t.validity!=='valid'||!t.batch_id||t.transfer_id==='0'))throw Error('미확인 이송 압력 조작 허용 거부');
  }
  function metrics(ms) {
    if(!ms || typeof ms!=='object' || Array.isArray(ms) || Object.keys(ms).length>32) throw Error('상태 항목 형식 오류');
    for(const m of Object.values(ms)) {
      if(!m || !units.has(m.unit) || !VALIDITIES.includes(m.validity) || !string(m.reason) || !string(m.source,128) || !(m.value===null || finite(m.value))) throw Error('상태 수치·단위·유효성 오류');
      if(m.validity!=='valid' && m.value!==null) throw Error('미확인 값은 null이어야 함');
      if(m.validity==='valid' && m.value===null) throw Error('유효값 누락');
      if(m.unit==='kg'&&m.value!==null&&m.value<0)throw Error('음수 질량 오류');
      if(m.value!==null && ['particles','bool'].includes(m.unit) && (!Number.isSafeInteger(m.value) || m.value<0 || (m.unit==='bool'&&m.value>1))) throw Error('입자 수·밸브 형식 오류');
    }
  }
  const ids=new Set();
  for(const q of s.equipment) {if(!string(q.id,64)||ids.has(q.id)||!string(q.label,128)||!['supplier','weigher','receiver','mixer'].includes(q.kind)) throw Error('장비 ID·종류 오류');ids.add(q.id);metrics(q.metrics);}
  for(const q of [...s.regions,...s.batch.materials]) {if(!string(q.id,64)||!string(q.label,128))throw Error('구역·원료 ID 오류');metrics(q.metrics);}
  return e;
}
// Integer cursors stay decimal strings: no loss above JavaScript's 2^53 boundary.
export class EnvelopeCursor {
  constructor(){this.run=null;this.retired=new Set();this.publication=-1n;this.epoch=-1n;this.sequence=-1n;}
  accept(e){
    validateEnvelope(e);const p=BigInt(e.publication_sequence);
    if(p<=this.publication || this.retired.has(e.run_id))return false;
    const s=e.snapshot;
    if(this.run===e.run_id && s && (BigInt(s.epoch)<this.epoch || (BigInt(s.epoch)===this.epoch && BigInt(s.sequence)<this.sequence)))return false;
    if(this.run!==e.run_id){if(this.run)this.retired.add(this.run);this.run=e.run_id;this.epoch=-1n;this.sequence=-1n;}
    if(this.retired.size>1024)throw Error('실행 수명 한도: 로컬 서버를 재시작하세요');
    this.publication=p;if(s){this.epoch=BigInt(s.epoch);this.sequence=BigInt(s.sequence);}return true;
  }
}
export class BrowserCursor {
  constructor(){this.server=null;this.retired=new Set();this.response=-1;this.source=null;}
  accept(packet){
    if(packet.schema_version!==VERSION || typeof packet.server_id!=='string' || !Number.isSafeInteger(packet.response_sequence))return false;
    if(this.retired.has(packet.server_id))return false;
    if(this.server!==packet.server_id){if(this.server)this.retired.add(this.server);this.server=packet.server_id;this.response=-1;this.source=null;}
    if(packet.response_sequence<=this.response)return false;
    if(packet.envelope){validateEnvelope(packet.envelope);const e=packet.envelope;
      if(!this.source||this.source.publisher!==e.publisher_id)this.source={publisher:e.publisher_id,cursor:new EnvelopeCursor()};
      const c=this.source.cursor;
      if(BigInt(e.publication_sequence)===c.publication){if(e.run_id!==c.run)return false;}else if(!c.accept(e))return false;
    } else this.source=null;
    this.response=packet.response_sequence;return true;
  }
}
export function operatorNumber(n,digits=2){
 if(!Number.isFinite(n))return '미확인';
 const limit=10**-digits;
 if(n!==0&&Math.abs(n)<limit)return n<0?`>-${limit}`:`<${limit}`;
 return new Intl.NumberFormat('ko-KR',{maximumFractionDigits:digits,useGrouping:false}).format(n||0);
}
export function displayMetric(m,{mass=false}={}) {
  const kg=mass&&m?.value>=1;const unit=mass?(kg?'kg':'g'):m?.unit==='Pa(g)'?'kPa(g)':m?.unit==='Pa'?'kPa':m?.unit==='m3/s'?'m³/s':m?.unit==='particles'?'개':m?.unit==='rpm'?'RPM':m?.unit==='s'?'초':m?.unit??'';
  if(!m || m.validity!=='valid' || m.value===null)return {text:({unknown:'미확인',initializing:'준비 중',stale:'오래된 값',source_missing:'미연결',unsupported:'미지원',error:'오류',not_computed:'미산출'})[m?.validity]??'미확인',unit};
  if(m.unit==='bool')return {text:m.value?'열림':'닫힘',unit:''};
  const n=m.value*(mass?(kg?1:1000):(m.unit==='Pa(g)'||m.unit==='Pa')?.001:1);const digits=m.unit==='m3/s'?4:2;
  return {text:operatorNumber(n,digits),unit};
}
