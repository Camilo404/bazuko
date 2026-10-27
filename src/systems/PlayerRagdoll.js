import * as THREE from 'three';
import { World, Body, Box, Plane, Vec3, ConeTwistConstraint, SAPBroadphase } from 'cannon-es';

const vector = value => new Vec3(value.x, value.y, value.z);
const up = new THREE.Vector3(0, 1, 0);

/** Once cajas articuladas. El mundo físico existe únicamente durante el atropello. */
export class PlayerRagdoll {
  constructor(player) {
    this.player = player;
    this.active = false;
    this.world = null;
    this.parts = [];
    this.saved = [];
    this.environment = new Map();
    this._point = new THREE.Vector3();
    this._quaternion = new THREE.Quaternion();
    this._parentQuaternion = new THREE.Quaternion();
  }

  start(direction, speed, collisions = null) {
    this.clear();
    const player = this.player;
    player.root.rotation.z = 0;
    player.root.position.copy(player.position);
    player.root.updateMatrixWorld(true);
    const rig = player.character.ragdollRig ?? {};
    const names = ['hips', 'spine', 'neck', 'head', 'leftArm', 'leftForeArm', 'leftHand',
      'rightArm', 'rightForeArm', 'rightHand', 'leftUpLeg', 'leftLeg', 'leftFoot', 'rightUpLeg', 'rightLeg', 'rightFoot'];
    player.character.group.traverse(node => {
      if (!node.isBone) return;
      const name = node.name.replace(/[^a-z0-9]/gi, '').toLowerCase().replace(/^mixamorig/, '');
      const key = names.find(key => key.toLowerCase() === name);
      if (key) rig[key] = node;
    });
    if (!rig.hips || !rig.spine || !rig.head) return false;
    this.world = new World({ gravity: new Vec3(0, -12, 0), allowSleep: true });
    this.world.broadphase = new SAPBroadphase(this.world);
    this.world.solver.iterations = 8;
    this.world.defaultContactMaterial.friction = .55;
    this.world.defaultContactMaterial.restitution = .08;
    this.floorY = player.position.y;
    this.floor = new Body({ mass: 0, shape: new Plane(), collisionFilterGroup: 1, collisionFilterMask: 2 });
    this.floor.quaternion.setFromEuler(-Math.PI / 2, 0, 0);
    this.floor.position.y = this.floorY;
    this.world.addBody(this.floor);
    const specs = [
      ['hips', 'spine', null, 12, .18], ['spine', 'neck', 'hips', 18, .20], ['head', null, 'spine', 5, .15],
      ['leftArm', 'leftForeArm', 'spine', 3, .085], ['leftForeArm', 'leftHand', 'leftArm', 2, .07],
      ['rightArm', 'rightForeArm', 'spine', 3, .085], ['rightForeArm', 'rightHand', 'rightArm', 2, .07],
      ['leftUpLeg', 'leftLeg', 'hips', 7, .105], ['leftLeg', 'leftFoot', 'leftUpLeg', 5, .085],
      ['rightUpLeg', 'rightLeg', 'hips', 7, .105], ['rightLeg', 'rightFoot', 'rightUpLeg', 5, .085]
    ];
    const byName = new Map();
    for (const [name, endName, parent, mass, radius] of specs) {
      const node = rig[name]; if (!node) continue;
      const a = node.getWorldPosition(new THREE.Vector3());
      let b = rig[endName]?.getWorldPosition(new THREE.Vector3());
      if (!b && name === 'spine') b = rig.head.getWorldPosition(new THREE.Vector3());
      if (!b) b = node.localToWorld(new THREE.Vector3(0, name === 'head' ? .24 : -.24, 0));
      if (a.distanceTo(b) < .08) b = a.clone().add(new THREE.Vector3(0, .18, 0));
      const length = a.distanceTo(b), center = a.clone().add(b).multiplyScalar(.5);
      const rotation = new THREE.Quaternion().setFromUnitVectors(up, b.clone().sub(a).normalize());
      const inverse = rotation.clone().invert();
      const body = new Body({ mass, shape: new Box(new Vec3(radius, Math.max(.06, length / 2), radius)),
        linearDamping: .12, angularDamping: .5, collisionFilterGroup: 2, collisionFilterMask: 1 });
      body.position.copy(vector(center)); body.quaternion.set(rotation.x, rotation.y, rotation.z, rotation.w);
      body.velocity.set(direction.x * Math.min(23, speed * 1.3), Math.min(7, 2 + speed * .34), direction.z * Math.min(23, speed * 1.3));
      body.angularVelocity.set(direction.z * 4 + (parent ? .8 : 0), .7, -direction.x * 4);
      this.world.addBody(body);
      const part = { name, node, body, joint: a, parent,
        offset: a.clone().sub(center).applyQuaternion(inverse),
        rotationOffset: inverse.multiply(node.getWorldQuaternion(new THREE.Quaternion())) };
      this.parts.push(part); byName.set(name, part);
      this.saved.push({ node, position: node.position.clone(), quaternion: node.quaternion.clone() });
    }
    for (const part of this.parts) {
      const parent = byName.get(part.parent); if (!parent) continue;
      const joint = vector(part.joint);
      const axis = part.body.quaternion.vmult(new Vec3(0, 1, 0));
      this.world.addConstraint(new ConeTwistConstraint(parent.body, part.body, {
        pivotA: parent.body.pointToLocalFrame(joint), pivotB: part.body.pointToLocalFrame(joint),
        axisA: parent.body.vectorToLocalFrame(axis), axisB: new Vec3(0, 1, 0),
        angle: part.parent === 'hips' || part.parent === 'spine' ? 1 : .85,
        twistAngle: .65, maxForce: 20000, collideConnected: false
      }));
    }
    this.origin = player.position.clone();
    this.pelvisOffset = this.parts[0].body.position.y - player.position.y;
    this.elapsed = 0; this.recovery = null; this.active = true;
    this._syncEnvironment(collisions);
    return true;
  }

  _syncEnvironment(collisions) {
    if (!this.world || !collisions) return;
    const point = this.player.position;
    const nearby = [...(collisions.colliders ?? []), ...(collisions.dynamicColliders ?? [])]
      .filter(entry => entry.box.distanceToPoint(point) < 28)
      .sort((a, b) => a.box.distanceToPoint(point) - b.box.distanceToPoint(point)).slice(0, 40);
    const present = new Set(nearby);
    for (const [entry, body] of this.environment) if (!present.has(entry)) {
      this.world.removeBody(body); this.environment.delete(entry);
    }
    for (const entry of nearby) {
      const { min, max } = entry.box;
      const oriented = entry.obb;
      let body = this.environment.get(entry);
      if (!body) {
        body = new Body({ mass: 0, shape: new Box(new Vec3(Math.max(.01, (max.x - min.x) / 2),
          Math.max(.01, (max.y - min.y) / 2), Math.max(.01, (max.z - min.z) / 2))),
        collisionFilterGroup: 1, collisionFilterMask: 2 });
        this.environment.set(entry, body); this.world.addBody(body);
      }
      const half = body.shapes[0].halfExtents;
      const hx = Math.max(.01, oriented?.halfSize.x ?? (max.x - min.x) / 2), hy = Math.max(.01, oriented?.halfSize.y ?? (max.y - min.y) / 2), hz = Math.max(.01, oriented?.halfSize.z ?? (max.z - min.z) / 2);
      if (Math.abs(half.x - hx) + Math.abs(half.y - hy) + Math.abs(half.z - hz) > 1e-5) {
        half.set(hx, hy, hz); body.shapes[0].updateConvexPolyhedronRepresentation();
        body.shapes[0].updateBoundingSphereRadius(); body.updateBoundingRadius();
      }
      body.position.set(oriented?.center.x ?? (min.x + max.x) / 2, oriented?.center.y ?? (min.y + max.y) / 2, oriented?.center.z ?? (min.z + max.z) / 2);
      if (oriented) body.quaternion.set(oriented.quaternion.x, oriented.quaternion.y, oriented.quaternion.z, oriented.quaternion.w);
      else body.quaternion.set(0, 0, 0, 1);
      body.aabbNeedsUpdate = true;
    }
  }

  update(dt, collisions) {
    if (!this.active) return;
    const player = this.player;
    if (this.recovery !== null) {
      this.recovery = Math.min(1, this.recovery + dt / .4);
      for (const saved of this.saved) {
        saved.node.position.lerpVectors(saved.fallenPosition, saved.position, this.recovery);
        saved.node.quaternion.slerpQuaternions(saved.fallenQuaternion, saved.quaternion, this.recovery);
      }
      if (this.recovery === 1) this.clear();
      return;
    }
    this._syncEnvironment(collisions);
    this.world.step(1 / 120, Math.min(dt, .1), 12);
    this.elapsed += dt;
    const pelvis = this.parts[0].body.position;
    player.position.set(pelvis.x, Math.max(this.floorY, pelvis.y - this.pelvisOffset), pelvis.z);
    player.root.position.copy(player.position); player.root.updateMatrixWorld(true);
    for (const part of this.parts) {
      this._quaternion.set(part.body.quaternion.x, part.body.quaternion.y, part.body.quaternion.z, part.body.quaternion.w);
      this._point.copy(part.offset).applyQuaternion(this._quaternion).add(part.body.position);
      part.node.parent.worldToLocal(this._point); part.node.position.copy(this._point);
      part.node.parent.getWorldQuaternion(this._parentQuaternion).invert();
      part.node.quaternion.copy(this._parentQuaternion).multiply(this._quaternion).multiply(part.rotationOffset);
      part.node.updateMatrixWorld(true);
    }
    const resting = this.parts.every(part => part.body.velocity.lengthSquared() < .08 && part.body.angularVelocity.lengthSquared() < .1);
    if (this.elapsed >= 5 || (this.elapsed > 2 && resting)) {
      player.position.y = this.floorY; player.root.position.copy(player.position);
      this.world = null; this.environment.clear(); this.parts.length = 0;
      if (player.stats?.salud <= 0) { this.active = false; return; }
      this.recovery = 0;
      for (const saved of this.saved) {
        saved.fallenPosition = saved.node.position.clone(); saved.fallenQuaternion = saved.node.quaternion.clone();
      }
    }
  }

  clear() {
    for (const saved of this.saved) { saved.node.position.copy(saved.position); saved.node.quaternion.copy(saved.quaternion); }
    this.active = false; this.world = null; this.parts.length = 0; this.saved.length = 0;
    this.environment.clear(); this.recovery = null;
  }
}
