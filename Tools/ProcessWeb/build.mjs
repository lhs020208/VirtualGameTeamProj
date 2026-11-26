import {cp,mkdir,readFile} from 'node:fs/promises';
import {spawnSync} from 'node:child_process';
import {mockEnvelope} from './mock.mjs';
import {validateEnvelope} from './public/protocol.mjs';
for(const p of ['server.mjs','mock.mjs','public/app.mjs','public/protocol.mjs','control.mjs','public/commands.mjs','public/manual.mjs','mock-batch.mjs','public/recipes.mjs','public/batch.mjs','public/transfer-pressure.mjs','mock-pressure.mjs']){const r=spawnSync(process.execPath,['--check',p],{stdio:'inherit'});if(r.status)process.exit(r.status);}
for(const s of ['flowing','paused','unknown','unsupported','stale','error','complete','long','tiny'])validateEnvelope(mockEnvelope(1,s));
const html=await readFile('public/index.html','utf8');for(const p of ['/styles.css','/app.mjs'])if(!html.includes(p))throw Error('Missing entry '+p);
await mkdir('dist',{recursive:true});await cp('public','dist',{recursive:true});console.log('Web build passed: native ES modules checked; dist/ generated. No production dependencies.');
