import { test, expect } from '@playwright/test';

// Flujos del PR #4 original, adaptados a carga inicial, pausa e interfaz actuales.
test.beforeEach(async ({ page }) => {
  await page.addInitScript(() => localStorage.clear());
});

async function startNewGame(page) {
  await page.goto('/', { waitUntil: 'commit' });
  await expect(page.locator('#creator-modal')).toBeVisible({ timeout: 120_000 });
  await page.locator('#cr-name').fill('Parcero QA');
  await page.locator('#creator-done').click();
  await expect(page.locator('#loading-screen')).toBeHidden({ timeout: 120_000 });
  await expect(page.locator('#menu-title')).toHaveText('Nota del desarrollador');
  // La nota actual oculta temporalmente los créditos; conserva el enlace de colaboración.
  await expect(page.getByRole('link', { name: 'Ver issues del proyecto original' })).toHaveAttribute('href', 'https://github.com/juanagogo/bazuko/issues');
  await expect(page.locator('#menu')).toContainText('issues abiertos al público');
  await page.getByRole('button', { name: 'Continuar jugando', exact: true }).click();
  await expect(page.locator('#creator-modal')).toBeHidden();
  await expect(page.locator('#game-canvas')).toBeVisible();
  await expect(page.locator('#player-name')).toHaveText('Parcero QA');
}

test('partida nueva, talentos, recetas y edición del personaje', async ({ page }) => {
  await startNewGame(page);
  await expect(page.locator('#clock')).toHaveText(/Día 1 — \d{2}:\d{2}/);
  await page.keyboard.press('T');
  await expect(page.locator('#talent-panel')).toHaveClass(/open/);
  await expect(page.locator('#talent-panel')).toContainText('Árbol de Talentos');
  await page.keyboard.press('Escape');
  await expect(page.locator('#talent-panel')).not.toHaveClass(/open/);
  await expect(page.locator('#menu')).toBeHidden();

  await page.keyboard.press('C');
  await expect(page.locator('#craft-panel')).toHaveClass(/open/);
  await expect(page.locator('#craft-panel')).toContainText('Mesa de Crafteo');
  await expect(page.locator('#craft-panel .craft-recipe').first()).toBeVisible();
  // Un panel abierto pausa el juego: cerrarlo antes de editar la apariencia.
  await page.keyboard.press('Escape');
  await page.keyboard.press('P');
  await expect(page.locator('#creator-modal')).toBeVisible();
  await expect(page.locator('#creator-title')).toHaveText('Crea tu personaje');
  await page.locator('#creator-close').click();
  await expect(page.locator('#creator-modal')).toBeHidden();
});

test('ayuda bajo demanda, inventario y pausa sin abrir dos paneles al pulsar Escape', async ({ page }) => {
  await startNewGame(page);
  const help = page.locator('#controls-help-toggle');
  await expect(help).toHaveText('!');
  await expect(help).toHaveAttribute('aria-expanded', 'false');
  await help.click();
  await expect(page.locator('#controls-help-content')).toBeVisible();
  await expect(page.locator('#controls-help-content')).toContainText('Inventario');
  await help.click();
  await expect(help).toHaveAttribute('aria-expanded', 'false');

  await page.keyboard.press('I');
  await expect(page.locator('#inv-overlay')).toHaveClass(/open/);
  await page.keyboard.press('Escape');
  await expect(page.locator('#inv-overlay')).not.toHaveClass(/open/);
  await expect(page.locator('#menu')).toBeHidden();
  await page.keyboard.press('Escape');
  await expect(page.locator('#menu')).toBeVisible();
  await expect(page.locator('#menu-title')).toHaveText('Partida en Pausa');
  await page.keyboard.press('Escape');
  await expect(page.locator('#menu')).toBeHidden();
});
