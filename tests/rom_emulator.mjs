import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
const modulePath=process.argv[2]??'build/emulator/node_modules/jsnes/src/index.js';
const output=process.argv[3]??'build/test-output';
const {NES}=await import(pathToFileURL(path.resolve(modulePath)));
for(const name of ['demo','sprite','horizontal','vertical','workflow']) {
  const base=path.join(output,name);
  const project=JSON.parse(fs.readFileSync(base+'.tzj','utf8'));
  const rom=fs.readFileSync(base+'.nes');
  let pixels;
  const nes=new NES({emulateSound:false,onFrame:frame=>pixels=Uint32Array.from(frame)});
  nes.loadROM(rom.toString('binary'));
  nes.ppu.clipToTvSize=false;
  for(let i=0;i<120;i++)nes.frame();
  assert(pixels,`${name}: no video output`);
  const frame=project.frames[0], data=Buffer.from(frame.pixels,'hex');
  let mismatches=0;
  const examples=[];
  // JSNES reserves output row 0 for its pre-render dummy scanline.
  for(let y=1;y<240;y++)for(let x=0;x<256;x++) {
    const slot=data[y*project.width+x];
    let color=project.background[frame.attributes[Math.floor(y/16)*(project.width/16)+Math.floor(x/16)]][slot];
    for(const sprite of frame.sprites) {
      if(!sprite.visible||x<sprite.x||x>=sprite.x+8||y<sprite.y||y>=sprite.y+8)continue;
      const sx=sprite.flip_x?7-(x-sprite.x):x-sprite.x;
      const sy=sprite.flip_y?7-(y-sprite.y):y-sprite.y;
      const spriteSlot=sprite.pixels[sy*8+sx];
      if(!spriteSlot)continue;
      if(!(sprite.behind&&slot))color=project.sprite_palettes[sprite.palette][spriteSlot];
      break;
    }
    const expected=nes.ppu.palTable.curTable[color];
    if(pixels[y*256+x]!==expected){mismatches++;if(examples.length<5)examples.push({x,y,actual:pixels[y*256+x].toString(16),expected:expected.toString(16)});}
  }
  assert.equal(mismatches,0,`${name}: rendered pixel differences ${JSON.stringify(examples)}`);
  const nam=fs.readFileSync(base+'.nam');
  for(let page=0;page<nam.length/1024;page++) {
    const addr=page===0?0x2000:project.width===512?0x2400:0x2800;
    for(let i=0;i<1024;i++)assert.equal(nes.ppu.vramMem[nes.ppu.vramMirrorTable[addr+i]],nam[page*1024+i],`${name}: nametable ${page} byte ${i}`);
  }
  assert.equal(nes.cpu.mem[0x2000],0x88,`${name}: wrong PPU control`);
  console.log(`PASS ${name}: booted 120 frames, 61,184 visible pixels match, all nametable bytes match`);
}
