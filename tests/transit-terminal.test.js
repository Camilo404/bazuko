import test from 'node:test';
import assert from 'node:assert/strict';
import * as THREE from 'three';
import { TERMINAL, terminalGroundHeight, buildTransitTerminal } from '../src/world/bogota/TransitTerminal.js';
import { Bogota } from '../src/world/districts/Bogota.js';
import { BusService } from '../src/world/bogota/BusService.js';
import { CollisionSystem } from '../src/systems/CollisionSystem.js';
import { southBusPathX } from '../src/world/bogota/Colpatria.js';
import { viaductSurface } from '../src/world/bogota/ColpatriaViaduct.js';

test('la estación propia permite recorrer la plataforma desde la rampa sin paredes invisibles', () => {
  const model = buildTransitTerminal({ createSign: () => new THREE.Texture(), asphalt: new THREE.MeshStandardMaterial() });
  const collisions = new CollisionSystem(); collisions.colliders.push(...model.userData.colliders);
  let position = new THREE.Vector3(75,.02,-1);
  for (let i=0;i<560;i++) {
    const next = collisions.resolveMove(position,new THREE.Vector3(0,0,-.1));
    assert.ok(next.z < position.z - .099);
    next.y = terminalGroundHeight(next.x,next.z); position=next;
  }
  assert.ok(model.children.some(o=>o.name==='Rampa peatonal'));
  assert.equal(terminalGroundHeight(75,-30),.92);
  assert.equal(terminalGroundHeight(78.5,-30),.02);
  assert.ok(Math.abs(terminalGroundHeight(75,-58)-.02)<1e-8);
});

test('dos buses recorren sentidos contrarios, paran cuatro segundos y respetan sus carriles', () => {
  const route = new Bogota().getBusRoute();
  const buses=[new BusService(route),new BusService({...route,x:TERMINAL.southLane,pathX:southBusPathX,startZ:route.endZ,endZ:route.startZ})];
  for(const bus of buses) {
    const sign=bus.direction; let stopped=0, arrived=false, departed=false;
    for(let frame=0;frame<3000;frame++) {
      const before=bus.position.z; bus.update(.05);
      if(bus.direction*(bus.position.z-before)<-1) break;
      if(bus.dwellRemaining>0) { stopped+=.05; arrived=true; assert.equal(bus.position.z,-30); }
      if(arrived && sign*(bus.position.z+30)>1) { departed=true; break; }
    }
    assert.ok(arrived&&departed); assert.ok(stopped>=3.95&&stopped<=4.1);
  }
  assert.equal(buses[0].direction,-1);assert.equal(buses[1].direction,1);
});

test('ambas carrocerías caben en el puente y los dos colliders conservan orientación propia', () => {
  const route=new Bogota().getBusRoute();
  const buses=[new BusService(route),new BusService({...route,x:71.5,pathX:southBusPathX,startZ:route.endZ,endZ:route.startZ})];
  const collisions=new CollisionSystem();
  for(let z=28;z<300;z+=.5) {
    for(const bus of buses) {
      bus.position.z=z;bus._alignToGround();const h=bus.heading;
      for(const along of [-bus.length/2,0,bus.length/2])for(const side of [-bus.width/2,bus.width/2]) {
        const x=bus.position.x+h.x*along-h.z*side,wz=z+h.z*along+h.x*side;
        assert.notEqual(viaductSurface(x,wz),null,`${bus.direction}: ${x}, ${wz}`);
      }
    }
    collisions.setVehicles([],buses);
    assert.equal(collisions.dynamicColliders.length,2);
    assert.ok(collisions.dynamicColliders.every(e=>e.obb.quaternion.equals(e.vehicle.group.quaternion)));
  }
});

test('los buses contrarios pueden cruzarse en las curvas sin superponer sus cajas', () => {
  const route=new Bogota().getBusRoute();
  const buses=[new BusService(route),new BusService({...route,x:71.5,pathX:southBusPathX,startZ:route.endZ,endZ:route.startZ})];
  const collisions=new CollisionSystem();
  for(let z=100;z<230;z+=2)for(let offset=-20;offset<=20;offset+=2) {
    buses[0].position.z=z;buses[1].position.z=z+offset;
    buses.forEach(b=>b._alignToGround());collisions.setVehicles([],buses);
    const [a,b]=collisions.dynamicColliders.map(e=>e.obb),gap=b.center.clone().sub(a.center);
    let separated=false;
    for(const axis of [a.axes[0],a.axes[2],b.axes[0],b.axes[2]]) {
      const r=o=>o.axes.reduce((sum,v,i)=>sum+Math.abs(v.dot(axis))*o.halfSize.getComponent(i),0);
      if(Math.abs(gap.dot(axis))>r(a)+r(b))separated=true;
    }
    assert.ok(separated,`cruce solapado en Z ${z}, ${z+offset}`);
  }
});
