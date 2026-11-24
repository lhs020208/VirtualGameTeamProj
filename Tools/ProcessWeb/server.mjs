import http from 'node:http';
import {randomUUID,randomBytes,timingSafeEqual} from 'node:crypto';
import {readFile,writeFile,mkdir,rename,unlink} from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import path from 'node:path';
import {EnvelopeCursor,VERSION} from './public/protocol.mjs';
import {MockOperator} from './mock.mjs';
import {MockBatchOperator} from './mock-batch.mjs';
import {MockPressureOperator} from './mock-pressure.mjs';
import {ControlRelay} from './control.mjs';
const ROOT=path.dirname(fileURLToPath(import.meta.url));
export class StateStore {
 constructor({now=()=>performance.now(),mock=false}={}){this.now=now;this.mock=mock;this.sources=new Map();this.serverId=randomUUID();this.response=0;}
 accept(e){
  let v=this.sources.get(e.publisher_id);
  if(!v){if(this.sources.size>=8)for(const [id,old] of this.sources)if(this.now()-old.seen>3000)this.sources.delete(id);if(this.sources.size>=8)throw Error('동시 발행자 한도 초과');v={cursor:new EnvelopeCursor(),seen:0,changed:0,simulation:null};}
  if(!v.cursor.accept(e))throw Error('이전 실행·세대·역전/중복 발행 메시지 거부');
  const now=this.now(),time=e.snapshot?.simulation_seconds;
  if(v.envelope?.run_id!==e.run_id||v.envelope?.snapshot?.epoch!==e.snapshot?.epoch||time!==v.simulation)v.changed=now;
  v.envelope=structuredClone(e);v.simulation=time;v.seen=now;this.sources.set(e.publisher_id,v);
 }
 view(){
  const now=this.now(),all=[...this.sources.values()],live=all.filter(x=>now-x.seen<=3000);
  let v=live[0],status='waiting',reason='언리얼 연결 대기',next='대상 레벨의 명시적 조회 연결 설정을 확인하세요. 실제 GPU 연결 검증은 6단계에서 수행합니다.';
  if(live.length>1){status='ambiguous';v=null;reason='여러 언리얼 발행자의 하트비트가 최근 수신됐습니다.';next='대상 에디터·PIE 인스턴스를 하나로 지정하세요. 임의 대상을 선택하지 않습니다.';}
  else if(v){status=v.envelope.binding.status;reason=v.envelope.binding.reason;next=v.envelope.binding.next_action;}
  else if(all.length){v=all.sort((a,b)=>b.seen-a.seen)[0];status='disconnected';reason='언리얼 통신 갱신이 중단됐습니다.';next='에디터와 로컬 조회 연결을 확인하세요. 아래 이전 값은 오래된 기록입니다.';}
  const e=v?.envelope??null, stale=status==='disconnected'||!!(e?.snapshot&&(e.snapshot.age_ms+Math.max(0,now-v.seen)>2000||e.snapshot.validity==='stale'));
  return {schema_version:VERSION,server_id:this.serverId,response_sequence:++this.response,mock:this.mock,connection:{status,reason,next_action:next,heartbeat_age_ms:v?Math.max(0,now-v.seen):null},data_stale:stale,simulation_progress:!e?.snapshot||e.snapshot.simulation_seconds===null?'unknown':e.snapshot.execution==='paused'?'paused':v&&now-v.changed>3000?'not_advancing':'advancing',envelope:e};
 }
}
export async function startServer({port=8765,ingestPort=8766,mock=false,scenario='manual',linkFile=path.resolve(ROOT,'../../Saved/ProcessWeb/link.json'),writeLink=true}={}) {
 const store=new StateStore({mock}),token=randomBytes(32).toString('hex');let timer,webPort,stopping=false,mockPeer;const control=new ControlRelay(store);
 const assets=new Map(await Promise.all([['/','index.html'],['/app.mjs','app.mjs'],['/styles.css','styles.css'],['/protocol.mjs','protocol.mjs'],['/commands.mjs','commands.mjs'],['/manual.mjs','manual.mjs'],['/recipes.mjs','recipes.mjs'],['/batch.mjs','batch.mjs'],['/transfer-pressure.mjs','transfer-pressure.mjs']].map(async([url,file])=>[url,{body:await readFile(path.join(ROOT,'public',file)),type:file.endsWith('html')?'text/html; charset=utf-8':file.endsWith('css')?'text/css; charset=utf-8':'text/javascript; charset=utf-8'}])));
 function respond(res,code,data,type='application/json; charset=utf-8'){res.writeHead(code,{'content-type':type,'cache-control':'no-store','x-content-type-options':'nosniff','content-security-policy':"default-src 'self'; connect-src 'self'; style-src 'self'; img-src 'self'; frame-ancestors 'none'; base-uri 'none'",'connection':'close'});res.end(typeof data==='string'||Buffer.isBuffer(data)?data:JSON.stringify(data));}
 function hostOK(req,p){return [`127.0.0.1:${p}`,`localhost:${p}`].includes(req.headers.host);}
 async function body(req,max=16384){let n=0,parts=[];for await(const p of req){n+=p.length;if(n>max)throw Error('요청 크기 한도');parts.push(p);}return JSON.parse(Buffer.concat(parts).toString('utf8'));}
 const web=http.createServer(async(req,res)=>{
  if(!hostOK(req,webPort) || (req.headers.origin&&!['http://localhost:'+webPort,'http://127.0.0.1:'+webPort].includes(req.headers.origin)))return respond(res,403,{error:'로컬 동일 출처 조회만 허용'});
  if(req.url?.startsWith('/api/')){
   const origin='http://'+req.headers.host, session=req.headers['x-process-session'];
   // All session, heartbeat, authority and command/result APIs require exact Origin + JSON + session.
   // The browser never receives the separate native ingest token. No cookies, CORS or URL tokens.
   if(req.url!=='/api/state'){
    if(req.method!=='POST')return respond(res,405,{error:'명시적인 JSON POST만 허용'});
    if(req.headers.origin!==origin||req.headers['content-type']!=='application/json'||req.headers['sec-fetch-site']&&req.headers['sec-fetch-site']!=='same-origin')return respond(res,403,{error:'허용된 동일 웹 출처만 조작할 수 있습니다.'});
    try{
     const b=await body(req);
     if(req.url==='/api/session')return respond(res,200,{session:control.session(b.client_key),server_id:store.serverId});
     control.auth(session);
     if(req.url==='/api/heartbeat'){if(b.observed_transfer)control.observed(session,b.observed_transfer);return respond(res,200,control.heartbeat(session));}
     if(req.url==='/api/acquire')return respond(res,200,control.acquire(session));
     if(req.url==='/api/release'){control.release(session);return respond(res,200,control.view(session));}
     if(req.url==='/api/command')return respond(res,202,control.submit(session,b));
     if(req.url==='/api/result')return respond(res,200,control.result(session,b.request_id));
     return respond(res,404,{error:'지원하지 않는 제어 경로'});
    }catch(e){return respond(res,422,{error:e.message});}
   }
  }
  if(req.method!=='GET')return respond(res,405,{error:'허용되지 않는 메서드'});
  if(req.url==='/api/state')return respond(res,200,store.view());
  const a=assets.get(req.url);return a?respond(res,200,a.body,a.type):respond(res,404,{error:'조회 경로 없음'});
 });
 web.requestTimeout=2000;web.headersTimeout=3000;web.maxConnections=32;web.setTimeout(3000,s=>s.destroy());
 let actualIngest=ingestPort;
 const ingest=mock?null:http.createServer(async(req,res)=>{
  const supplied=req.headers.authorization??'',expected='Bearer '+token;
  if(!hostOK(req,actualIngest)||req.headers.origin||Buffer.byteLength(supplied)!==Buffer.byteLength(expected)||!timingSafeEqual(Buffer.from(supplied),Buffer.from(expected)))return respond(res,403,{error:'발행 인증 실패'});
  if(req.method!=='POST'||req.url!=='/v1/state')return respond(res,405,{error:'인증된 상태 교환만 허용'});
  let size=0,parts=[];
  try{for await(const b of req){size+=b.length;if(size>524288){respond(res,413,{error:'상태 메시지 크기 한도'});req.destroy();return;}parts.push(b);}
   const e=JSON.parse(Buffer.concat(parts).toString('utf8'));store.accept(e);control.ingest(e);respond(res,200,{accepted:true,...control.frame(e)});
  }catch(e){respond(res,422,{error:e.message});}
 });
 if(ingest){ingest.requestTimeout=2000;ingest.headersTimeout=3000;ingest.maxConnections=8;ingest.setTimeout(3000,s=>s.destroy());}
 const listen=s=>new Promise((ok,no)=>{s.once('error',no);s.listen(s===web?port:ingestPort,'127.0.0.1',()=>{s.removeListener('error',no);ok();});});
 try{
  await listen(web);webPort=web.address().port;
  if(ingest){await listen(ingest);actualIngest=ingest.address().port;
   if(writeLink){await mkdir(path.dirname(linkFile),{recursive:true});const tmp=linkFile+'.'+process.pid+'.tmp';await writeFile(tmp,JSON.stringify({version:1,server_id:store.serverId,ingest_port:actualIngest,token}),{mode:0o600});await rename(tmp,linkFile);}
  }else{let n=0;const peer=scenario==='pressure'?new MockPressureOperator():scenario==='batch'?new MockBatchOperator():new MockOperator(scenario);mockPeer=peer;const emit=()=>{const e=peer.tick(++n);store.accept(e);control.ingest(e);peer.receive(control.frame(e));};emit();timer=setInterval(emit,250);}
 }catch(e){web.closeAllConnections();web.close();if(ingest){ingest.closeAllConnections();ingest.close();}throw e;}
 async function close(){if(stopping)return;stopping=true;clearInterval(timer);mockPeer?.close?.();for(const s of [web,ingest].filter(Boolean)){s.closeAllConnections();await new Promise(ok=>s.close(ok));}if(!mock&&writeLink){try{if(JSON.parse(await readFile(linkFile,'utf8')).token===token)await unlink(linkFile);}catch{}}}
 return {store,control,webPort,ingestPort:actualIngest,token,close,mockPeer};
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url)){
 const args=process.argv.slice(2),arg=(k,f)=>{const i=args.indexOf(k);return i<0?f:args[i+1];};
 try{const mock=args.includes('--mock');const app=await startServer({port:Number(arg('--port',8765)),ingestPort:Number(arg('--ingest-port',8766)),mock,scenario:arg('--scenario','manual')});console.log(`로컬 공정 UI http://localhost:${app.webPort} — ${mock?'모의 데이터·모의 조작':'언리얼 연결 대기'}`);console.log('실제 GPU 연결·동작 미검증 — 6단계에서 통합 검증 예정');for(const sig of ['SIGINT','SIGTERM'])process.on(sig,()=>app.close().then(()=>process.exit(0)));}
 catch(e){console.error('로컬 서버 시작 실패:',e.message);process.exitCode=1;}
}
