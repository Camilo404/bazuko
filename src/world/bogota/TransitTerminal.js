import * as THREE from 'three';

export const TERMINAL = Object.freeze({ x: 75, z: -30, width: 4, length: 40,
  x0: 54, x1: 96, z0: -74, z1: 0, platformY: .92, northLane: 78.5, southLane: 71.5 });
export const inTerminal = (x, z) => x >= 54 && x <= 96 && z >= -74 && z <= 0;
export function terminalGroundHeight(x, z) {
  if (x >= 73 && x <= 77 && z >= -58 && z <= -2) {
    if (z < -50) return .02 + .9 * (z + 58) / 8;
    if (z > -10) return .02 + .9 * (-2 - z) / 8;
    return TERMINAL.platformY;
  }
  return x <= 59 || x >= 91 ? .12 : .02;
}

export function paintTerminalEntrance(map) {
  for (let z = 0; z <= 7; z++) for (let x = 27; x <= 48; x++) {
    map[z][x] = x <= 28 || x >= 47 ? 'sidewalk' : x >= 35 && x <= 40 ? 'busway' : 'road';
  }
}

/** Estación propia con isla accesible, dos accesos en rampa y galerías de vidrio. */
export function buildTransitTerminal({ createSign, asphalt }) {
  const group = new THREE.Group(); group.name = 'Estación Portal del Centro';
  const steel = new THREE.MeshStandardMaterial({ color: '#48575b', roughness: .65, metalness: .3 });
  const concrete = new THREE.MeshStandardMaterial({ color: '#b4b5aa', roughness: .9 });
  const glass = new THREE.MeshStandardMaterial({ color: '#accbd0', transparent: true, opacity: .3, roughness: .3, depthWrite: false });
  const red = new THREE.MeshStandardMaterial({ color: '#b32c35', roughness: .7 });
  const yellow = new THREE.MeshStandardMaterial({ color: '#ddbd35', roughness: .8 });
  const colliders = [];
  const box = (w,h,d,x,y,z,material,name,solid = true) => {
    const mesh = new THREE.Mesh(new THREE.BoxGeometry(w,h,d),material);
    mesh.position.set(x,y,z); mesh.castShadow = mesh.receiveShadow = true; mesh.name = name; group.add(mesh);
    if (solid) colliders.push({ box: new THREE.Box3().setFromObject(mesh), mesh, layered: true });
    return mesh;
  };
  const street = box(42,.08,74,75,-.02,-37,asphalt,'Calle ampliada',false);
  const p = street.geometry.attributes.position, uv = street.geometry.attributes.uv;
  for (let i=0;i<uv.count;i++) uv.setXY(i,(p.getX(i)+75)/14,(p.getZ(i)-37)/14);
  for (const x of [56.5,93.5]) box(5,.12,74,x,.06,-37,concrete,'Andén lateral',false);
  box(4,.9,40,75,.47,-30,concrete,'Plataforma transitable',false);
  for (const [z, reverse] of [[-54,false],[-6,true]]) {
    const g = new THREE.BufferGeometry();
    const a = reverse ? .92 : .02, b = reverse ? .02 : .92;
    g.setAttribute('position',new THREE.Float32BufferAttribute([73,a,z-4,77,a,z-4,73,b,z+4,77,b,z+4],3));
    g.setIndex([0,2,1,1,2,3]); g.computeVertexNormals();
    const ramp = new THREE.Mesh(g,concrete); ramp.name='Rampa peatonal'; ramp.receiveShadow=true; group.add(ramp);
  }
  // Estructura esbelta: pasillo central libre y puertas enfrentadas a ambos carriles.
  for (const z of [-49,-43,-35,-27,-19,-11]) for (const x of [73.12,76.88]) {
    box(.14,3.5,.14,x,2.67,z,steel,'Columna');
  }
  for (const [a,b] of [[-50,-43],[-39,-25],[-21,-10]]) for (const x of [73.02,76.98]) {
    box(.06,2.1,b-a,x,2.15,(a+b)/2,glass,'Panel de vidrio');
    box(.08,.3,b-a,x,1.1,(a+b)/2,steel,'Zócalo');
  }
  box(4.8,.16,41,75,4.5,-30,steel,'Cubierta');
  for (const x of [73,77]) box(.12,.28,41,x,4.3,-30,red,'Franja de estación',false);
  for (const z of [-46,-30,-14]) {
    box(.5,.4,2,74,.12+1,z,steel,'Banca');
    box(.08,.55,2,73.8,1.55,z,steel,'Espaldar');
  }
  for (const x of [73.2,76.8]) box(.09,.015,40,x,.935,-30,yellow,'Borde de seguridad',false);
  for (const z of [-41,-23]) for (const x of [72.9,77.1]) {
    box(.04,.6,3,x,3.75,z,red,'Dintel de acceso',false);
  }
  for (const z of [-58,-2]) for (let x=60;x<=90;x+=.85) box(.45,.015,2,x,.14,z,concrete,'Paso peatonal',false);
  const sign = (title,sub,z) => {
    const mesh = new THREE.Mesh(new THREE.PlaneGeometry(3.6,.8),new THREE.MeshBasicMaterial({
      map:createSign({ title,sub,bg:'#b32c35',fg:'#ffffff',w:768,h:160,border:false }),side:THREE.DoubleSide
    }));
    mesh.position.set(75,3.9,z); group.add(mesh);
  };
  sign('PORTAL DEL CENTRO','TRANSMILENIO · ACCESO PEATONAL',-9.8);
  sign('NORTE  ←  ·  →  SUR','ANDENES A AMBOS LADOS',-30);
  for (const x of [53.8,96.2]) box(.4,4,74,x,2,-37,steel,'Límite lateral',true).visible=false;
  box(42,4,.4,75,2,-74.2,steel,'Final de vía',true).visible=false;
  group.userData.colliders=colliders;
  return group;
}
