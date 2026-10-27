# Bazuko C++ (OpenGL 4.5 Core DSA)

Puerto nativo en C++20 del juego de supervivencia urbana 3D **Bazuko**, ambientado en el centro de Bogotá.

## Arquitectura y Decisiones Técnicas

1. **OpenGL 4.5 Core Profile con Direct State Access (DSA)**:
   - Evita el paradigma clásico de *bind-to-edit* propenso a errores de estado global.
   - Creación y configuración directa de búferes (`glCreateBuffers`, `glNamedBufferData`, `glNamedBufferSubData`).
   - Vertex Arrays desacoplados (`glCreateVertexArrays`, `glVertexArrayVertexBuffer`, `glVertexArrayAttribFormat`), cuya estructura mapea casi 1:1 a los formatos de entrada de Vulkan (`VkVertexInputAttributeDescription`).
   - Uniform Buffer Objects (UBOs) para Cámara, Luces y paletas de hasta 128 huesos (`CameraBlock`, `LightBlock`, `SkinBlock`).
2. **Compatibilidad con Vulkan a futuro**:
   - Capa de abstracción limpia en `src/renderer/`: `Buffer`, `VertexArray`, `Shader`, `Mesh`, `Camera`, `Renderer`.
   - La lógica de juego y el mundo en `src/world/` no realizan llamadas directas a la API gráfica.
3. **Bibliotecas clave**:
   - **Ventana & Contexto**: GLFW 3.4
   - **Loader OpenGL**: GLAD2 (OpenGL 4.5 Core)
   - **Matemáticas**: GLM (vectores, matrices, proyecciones)
   - **Interfaz Gráfica**: Dear ImGui (estilo oscuro urbano personalizado)
   - **Formatos y Modelos**: stb_image, cgltf, miniaudio, nlohmann/json

## Estructura del Proyecto

```
cpp/
├── CMakeLists.txt
├── assets/
│   └── shaders/
│       ├── default.vert       # GLSL 450 Core con UBOs de cámara y modelo
│       └── default.frag       # Iluminación difusa, ambiente y niebla atmosférica
├── src/
│   ├── main.cpp               # Bucle principal de juego
│   ├── core/
│   │   ├── Window.hpp/.cpp    # Manejo de ventana GLFW y contexto OpenGL 4.5 Core
│   │   ├── Input.hpp/.cpp     # Entrada de teclado, mouse, deltas y pointer lock
│   │   ├── Clock.hpp          # Delta time y tiempo transcurrido de alta precisión
│   │   └── Log.hpp            # Logging estructurado por niveles
│   ├── renderer/
│   │   ├── GL.hpp             # Loader GLAD y callbacks de depuración OpenGL
│   │   ├── Buffer.hpp/.cpp    # VertexBuffer, IndexBuffer, UniformBuffer (DSA)
│   │   ├── VertexArray.hpp/.cpp # VertexArray (DSA)
│   │   ├── Shader.hpp/.cpp    # Compilador y vinculador GLSL 450
│   │   ├── Mesh.hpp/.cpp      # Estructura de vértices y generadores de primitivas
│   │   ├── Camera.hpp/.cpp    # Cámara en tercera persona e isométrica con suavizado
│   │   └── Renderer.hpp/.cpp  # Pipeline de renderizado por pases y sombreado
│   ├── ui/
│   │   └── UIManager.hpp/.cpp # Integración Dear ImGui con tema oscuro bogotano
│   └── world/
│       └── CityLayout.hpp/.cpp# Grilla urbana de Bogotá (carreras, calles, andenes, edificios)
└── third_party/               # GLAD, stb_image, cgltf, miniaudio, nlohmann/json
```

## Compilación y Ejecución

### En Linux / WSL2 (Ubuntu):

```bash
cd cpp
mkdir -p build && cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
./bazuko
```

CMake usa GLFW y GLM del sistema cuando están disponibles y obtiene versiones fijadas
de GLFW, GLM e ImGui con FetchContent en instalaciones limpias. En Windows instala
Visual Studio 2022 con C++ y CMake; WebP se descubre mediante `find_package` o
`pkg-config` en Linux, y mediante `WEBP_ROOT` en Windows. Los modelos, audio y demás
recursos de `../public` se copian junto al ejecutable.

### Controles iniciales:

- `W`, `A`, `S`, `D`: Moverse por las calles y andenes de la ciudad.
- `Shift`: Correr.
- `Q`: Buscar y recoger un objeto cercano.
- `E`: Interactuar; recoger, comprar, robar un espejo o usar un consumible.
- `F`: Vender los objetos reciclables del costal.
- `C`: Fabricar un radio, vendaje, antibiótico o lona si tienes ingredientes.
- `B` / `L`: Pedir limosna.
- Acércate a la basura o pulsa `Q` junto a ella para recogerla.
- Usa un vendaje con `E` para curar salud y un antibiótico para curar enfermedades.
- Acércate a una fachada y pulsa `E` para comprar comida, medicina o materiales.
- Acércate a un auto y pulsa `E` para intentar robarlo; te pueden atrapar.
- Habla con el NPC junto a la recicladora usando `E`; entrega 5 reciclables allí con `F` para cobrar la misión.
- Los semáforos alternan el paso del tráfico en las intersecciones.
- También puedes fabricar arma, mochila, curitas y cartón de rifa con `C` si consigues sus materiales.
- `V`: Alternar entre cámara en Tercera Persona y Vista Isométrica.
- `Click izquierdo`: Bloquear cursor para mirar alrededor (estilo juego de acción 3D).
- `Tab`: Bloquear o liberar cursor del mouse.
- `Rueda del mouse`: Zoom de la cámara.
- `Esc`: Liberar cursor o salir.

## Animación y población

El personaje usa los clips `Idle`, `Walk` y `Run` de `public/models/animaciones-prueba.glb`. Se emparejan huesos Mixamo por nombre para las piezas de cuerpo, chaqueta, tenis y pelo; el render aplica `JOINTS_0`, `WEIGHTS_0` y las matrices inversas de bind del glTF. Los peatones eligen destinos alcanzables y siguen rutas de cuadrícula limitadas a aceras, plazas y zonas verdes; no atraviesan edificios para llegar a un punto aleatorio. Los vehículos circulan por circuitos cerrados construidos sobre cruces reales, mantienen distancia y frenan ante el jugador, peatones y semáforos; estos alternan fases con despeje entre sentidos.
