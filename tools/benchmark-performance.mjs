// Ejecutar con Vite o preview activos. Cada ejecución usa un perfil de navegador aislado.
import { chromium } from '@playwright/test';
import { mkdir, writeFile } from 'node:fs/promises';
import { parseArgs } from 'node:util';

const { values } = parseArgs({ options: {
  url: { type: 'string', default: 'http://127.0.0.1:5173/' },
  seconds: { type: 'string', default: '15' },
  output: { type: 'string', default: `work/performance/${new Date().toISOString().replaceAll(':', '-')}` },
  headless: { type: 'boolean', default: false }
} });
const seconds = Number(values.seconds);
if (!Number.isFinite(seconds) || seconds < 5 || seconds > 30) throw new Error('--seconds debe estar entre 5 y 30.');
const viewport = { width: 1280, height: 800 };
const scenarios = [
  { name: 'inicio', x: 112.98, z: 97.84, quality: 'baja' },
  { name: 'colpatria', x: 105, z: 200, quality: 'baja' },
  { name: 'lago', x: 78.5, z: 280, quality: 'baja' },
  { name: 'terminal', x: 75, z: -30, quality: 'baja' },
  { name: 'inicio-alta', x: 112.98, z: 97.84, quality: 'alta' }
];
await mkdir(values.output, { recursive: true });
const browser = await chromium.launch({ headless: values.headless });
const context = await browser.newContext({ viewport, deviceScaleFactor: 1 });
const page = await context.newPage();
const report = { createdAt: new Date().toISOString(), url: values.url, viewport, seconds,
  seed: 89123, adaptiveResolution: false, gpu: null, errors: [], scenarios: [] };
page.on('pageerror', error => report.errors.push(error.message));
try {
  await page.addInitScript(seed => {
    Math.random = () => ((seed = Math.imul(seed, 1664525) + 1013904223 | 0) >>> 0) / 4294967296;
    localStorage.clear();
  }, report.seed);
  await page.goto(values.url, { waitUntil: 'commit' });
  await page.locator('#creator-modal').waitFor({ state: 'visible', timeout: 120000 });
  await page.locator('#cr-name').fill('Rendimiento');
  await page.locator('#creator-done').click();
  await page.getByRole('button', { name: 'Continuar jugando', exact: true }).click({ timeout: 180000 });
  report.gpu = await page.evaluate(() => {
    const gl = window.__game.renderer.instance.getContext();
    const extension = gl.getExtension('WEBGL_debug_renderer_info');
    const renderer = extension ? gl.getParameter(extension.UNMASKED_RENDERER_WEBGL) : gl.getParameter(gl.RENDERER);
    return { renderer, software: /swiftshader|llvmpipe|softpipe|software/i.test(renderer) };
  });
  console.log('WebGL:', report.gpu.renderer);
  if (report.gpu.software) console.log('Renderizado por software: estas cifras no representan los FPS en una GPU física.');
  // Evitar que una reducción de resolución o calidad se confunda con una mejora de código.
  await page.evaluate(() => { window.__game.renderer.adaptResolution = () => {}; });
  await page.waitForTimeout(5000);
  for (const scenario of scenarios) {
    await page.evaluate(({ x, z, quality }) => {
      const game = window.__game;
      game.renderer.setQuality(quality);
      game.player.position.set(x, game.world.active.getGroundHeight(x, z), z);
      game.player.root.position.copy(game.player.position);
      game.stats.salud = game.stats.hambre = game.stats.sed = 100;
    }, scenario);
    await page.waitForTimeout(5000);
    await page.evaluate(() => {
      const game = window.__game;
      game.profiler.reset(); game.profiler.enabled = true; game.performanceHUD.metrics.reset();
    });
    await page.waitForTimeout(seconds * 1000);
    const measurement = await page.evaluate(() => {
      const game = window.__game;
      game.profiler.enabled = false;
      return { ...game.getPerformanceReport(), averageFPS: game.performanceHUD.metrics.average,
        measuredFrames: game.performanceHUD.metrics.frames, elapsedMs: game.performanceHUD.metrics.elapsed };
    });
    report.scenarios.push({ ...scenario, ...measurement });
    await page.screenshot({ path: `${values.output}/${scenario.name}.png` });
    console.log(`${scenario.name}: ${measurement.averageFPS.toFixed(1)} FPS medios, p95 ${measurement.frames.p95.toFixed(1)} ms, p99 ${measurement.frames.p99.toFixed(1)} ms, 1% bajos ${measurement.frames.low1Percent.toFixed(1)} FPS`);
    await writeFile(`${values.output}/report.json`, JSON.stringify(report, null, 2));
  }
  if (report.errors.length) throw new Error(report.errors.join('\n'));
} catch (error) {
  report.errors.push(error.message);
  await page.screenshot({ path: `${values.output}/error.png` }).catch(() => {});
  await writeFile(`${values.output}/report.json`, JSON.stringify(report, null, 2));
  throw error;
} finally { await browser.close(); }
console.log(`Informe y capturas: ${values.output}`);
