/** Índice conservador X/Z: guarda volúmenes completos, incluidos los que cruzan celdas. */
export class SpatialGrid {
  constructor(cellSize = 16) {
    this.cellSize = cellSize;
    this.cells = new Map();
    this.seen = new Set();
  }

  clear() {
    // Reutilizar los buckets evita recrearlos en cada subpaso del tráfico.
    for (const bucket of this.cells.values()) bucket.length = 0;
  }

  insert(item, minX, minZ, maxX = minX, maxZ = minZ) {
    const size = this.cellSize;
    for (let z = Math.floor(minZ / size); z <= Math.floor(maxZ / size); z++) {
      for (let x = Math.floor(minX / size); x <= Math.floor(maxX / size); x++) {
        const key = `${x},${z}`;
        let bucket = this.cells.get(key);
        if (!bucket) this.cells.set(key, bucket = []);
        bucket.push(item);
      }
    }
  }

  query(minX, minZ, maxX, maxZ, result = []) {
    result.length = 0;
    this.seen.clear();
    const size = this.cellSize;
    for (let z = Math.floor(minZ / size); z <= Math.floor(maxZ / size); z++) {
      for (let x = Math.floor(minX / size); x <= Math.floor(maxX / size); x++) {
        const bucket = this.cells.get(`${x},${z}`);
        if (!bucket) continue;
        for (const item of bucket) {
          if (this.seen.has(item)) continue;
          this.seen.add(item);
          result.push(item);
        }
      }
    }
    return result;
  }
}
