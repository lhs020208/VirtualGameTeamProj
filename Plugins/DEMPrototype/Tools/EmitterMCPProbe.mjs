// Read-only MCP capability discovery; no scene mutation or guessed tool calls.
let session;
async function request(method, params, id) {
  const headers = {'Content-Type':'application/json', Accept:'application/json, text/event-stream'};
  if (session) headers['Mcp-Session-Id'] = session;
  const response = await fetch('http://127.0.0.1:8000/mcp', {
    method:'POST', headers, body:JSON.stringify({jsonrpc:'2.0', method, params, ...(id === undefined ? {} : {id})}),
    signal:AbortSignal.timeout(15000)
  });
  session = response.headers.get('Mcp-Session-Id') || session;
  const body = await response.text();
  if (!response.ok) throw Error(`${response.status}: ${body}`);
  if (!body.trim()) return null;
  if (response.headers.get('content-type')?.includes('text/event-stream')) {
    return body.split(/\r?\n/).filter(x=>x.startsWith('data:')).map(x=>JSON.parse(x.slice(5))).find(x=>x.id === id);
  }
  return JSON.parse(body);
}
const initialize=await request('initialize',{protocolVersion:'2024-11-05',capabilities:{},clientInfo:{name:'codex-emitter-inspection',version:'1.0'}},1);
await request('notifications/initialized',{});
const tools=await request('tools/list',{},2);
const resources=await request('resources/list',{},3);
const toolsets=await request('tools/call',{name:'list_toolsets',arguments:{}},4);
const description=await request('tools/call',{name:'describe_toolset',arguments:{toolset_name:'ToolsetRegistry.AgentSkillToolset'}},5);
console.log(JSON.stringify({initialize,tools,resources,toolsets,description},null,2));
