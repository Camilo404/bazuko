# Integración del repositorio original

Fecha: 21 de octubre de 2026. Base original integrada: juanagogo/bazuko, main, commit 77f66bd. Rama de trabajo: integracion-upstream-pr6. Respaldo previo: respaldo-main-antes-integracion-pr6.

Se resolvieron los conflictos conservando las adaptaciones locales de ciudad, tráfico, ragdoll, inventario, audio, cambuche, compañero, palomas y rendimiento. Las funciones de Cristianlv11 ya adaptadas se conciliaron contra su versión común para evitar duplicarlas. Se incorporaron radio y pilas, inercia de movimiento, cámara dinámica, asfalto húmedo, abstinencia, alucinaciones y proyectiles con reacción de peatones. Se conservan los créditos existentes.

Compatibilidad: radio con volumen general, silencio y pausa; objetos nuevos utilizables desde el inventario; caché de puntos del radar sin congelar al perro; humedad desactivada en calidad baja; inercia limpiada al terminar un atropello. Se descartó la dependencia overpass sin referencias en el código, que exigía Node 0.10; la tipografía sigue usando @fontsource/overpass.

Validación: 192 pruebas unitarias; compilación de producción; descubrimiento de los dos escenarios Playwright. Los escenarios de navegador no se ejecutaron: Chromium no está instalado en este entorno. No se verificaron visualmente los cambios ni su sonido en un navegador.
