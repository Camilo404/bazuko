export const GRAPHICS_PROFILES = Object.freeze({
  baja: { pixelRatio: 1, shadowSize: 0, silhouette: false, animationBudget: 3, far: 180 },
  media: { pixelRatio: 1.25, shadowSize: 1024, silhouette: true, animationBudget: 2, far: 220 },
  alta: { pixelRatio: 1.5, shadowSize: 2048, silhouette: true, animationBudget: 1, far: 300 }
});

export function effectiveQuality(selected = 'alta', pressure = 0) {
  const levels = ['baja', 'media', 'alta'];
  return levels[Math.max(0, Math.max(0, levels.indexOf(selected)) - pressure)];
}
