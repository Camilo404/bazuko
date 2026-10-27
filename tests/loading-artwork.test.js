import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { LoadingArtwork, shuffledArtwork } from '../src/ui/LoadingArtwork.js';

test('las parejas se barajan sin perder elementos ni repetir al cambiar de vuelta', () => {
  for(const value of [0,.25,.5,.99]) for(const previous of [1,2,3]) {
    const deck=shuffledArtwork([1,2,3],previous,()=>value);
    assert.deepEqual([...deck].sort(),[1,2,3]);
    assert.notEqual(deck[0],previous);
  }
});

test('cada pareja declarada contiene fondo y personaje WebP reales', () => {
  const ids=JSON.parse(fs.readFileSync(new URL('../src/ui/loading-artwork.json',import.meta.url)));
  assert.equal(new Set(ids).size,ids.length);
  for(const id of ids) for(const kind of ['fondo','personaje']) {
    const bytes=fs.readFileSync(new URL(`../public/images/pantalla-carga/${kind}_pantalla_carga_${id}.webp`,import.meta.url));
    assert.equal(bytes.subarray(0,4).toString(),'RIFF');
    assert.equal(bytes.subarray(8,12).toString(),'WEBP');
    assert.equal(bytes.readUInt32LE(4)+8,bytes.length);
    if(kind==='personaje') {
      assert.equal(bytes.subarray(12,16).toString(),'VP8X');
      assert.ok(bytes[20]&16,'el personaje debe conservar su canal alfa');
    }
  }
});

test('terminar la carga cancela la galería aunque las imágenes sigan decodificándose', async t => {
  let resolve;
  const decoding=new Promise(yes=>{resolve=yes;});
  const images=[],listeners=new Set(),inserted=[];
  const root={
    addEventListener:(_,fn)=>listeners.add(fn),removeEventListener:(_,fn)=>listeners.delete(fn),
    prepend:scene=>inserted.push(scene),querySelectorAll:()=>[],classList:{remove(){},add(){}}
  };
  for(const [name,value] of Object.entries({
    document:{createElement:()=>({append(){},setAttribute(){}})},
    Image:class {constructor(){images.push(this);} decode(){return decoding;}}
  })) {
    const original=Object.getOwnPropertyDescriptor(globalThis,name);
    Object.defineProperty(globalThis,name,{configurable:true,writable:true,value});
    t.after(()=>original?Object.defineProperty(globalThis,name,original):delete globalThis[name]);
  }
  const artwork=new LoadingArtwork(root,[2],'/juego/');
  artwork.start();
  assert.equal(images.length,2);
  assert.equal(images[0].src,'/juego/images/pantalla-carga/fondo_pantalla_carga_2.webp');
  assert.equal(images[1].src,'/juego/images/pantalla-carga/personaje_pantalla_carga_2.webp');
  artwork.stop({remove:true});resolve();
  await new Promise(yes=>setImmediate(yes));
  assert.equal(inserted.length,0);assert.equal(listeners.size,0);assert.equal(artwork.timer,undefined);
});
