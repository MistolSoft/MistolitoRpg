# PLAN: Sistema de Entrenamiento On-Device con PPO

## Visión general

El sistema permite que el modelo de combate **aprenda y evolucione** durante el gameplay del ESP32-S3. Cuando el pet acumula suficientes transiciones de combate, desbloquea su level up. En ese momento, el dispositivo entra en **modo entrenamiento**: bloquea el juego, libera recursos de display, y usa toda la potencia de cálculo para actualizar el modelo con PPO. Todo el sistema de almacenamiento vive en la SD card (16 GB disponibles).

---

## FASE 1: Sistema de Experiencia (Replay Buffer en SD)

### 1.1 Estructura de archivos en SD

```
/sdcard/BRAIN/COMBAT/
    replay/
        chunk_0001.bin     # Chunk 1: primeras 10,000 transiciones
        chunk_0002.bin     # Chunk 2: siguientes 10,000 transiciones
        chunk_0003.bin     # Chunk 3: etc.
        header.bin         # Metadata global: total chunks, total transiciones, versión
    episodes/
        ep_00001.bin       # Episodios completos (para debug y análisis)
        ep_00002.bin
    checkpoints/
        policy_head_001.bin
        value_head_001.bin
        checkpoint_001.meta
    exploration/
        counters.bin       # Contadores de exploración forzada por acción
```

### 1.2 Formato de transición

Cada transición:
- Estado actual (8 floats) → 32 bytes
- Acción tomada (1 uint8) → 1 byte
- Reward (1 float) → 4 bytes
- Siguiente estado (8 floats) → 32 bytes
- Done flag (1 uint8) → 1 byte
- Total: 70 bytes por transición

### 1.3 Sistema de chunks (sin límite de tamaño)

**No hay buffer circular.** Los datos no se sobreescriben. Se acumulan indefinidamente.

- Cada chunk contiene 10,000 transiciones (700 KB)
- Cuando un chunk se llena, se crea uno nuevo
- El entrenador procesa chunks en orden o por muestreo
- Con 16 GB de SD: ~22 millones de chunks posibles (22 mil millones de transiciones)
- A 100 combates/día × 10 turnos/combate × 1 transición/turno = 1,000 transiciones/día
- Eso son ~10,000 días (~27 años) de gameplay antes de llenar la SD

### 1.4 Header global

Estructura en `header.bin`:
- Total de chunks creados
- Total de transiciones acumuladas
- Timestamp del último chunk
- Checksum para integridad

### 1.5 Cómo se registra cada paso del combate

En `game_coordinator.c`, después de cada acción del modelo:
1. Guardar estado actual (8 features) en buffer temporal del episodio
2. Ejecutar modelo → obtener acción (o exploración forzada)
3. Ejecutar acción en el combate → obtener reward inmediato
4. Escribir transición (s, a, r, s', done) al chunk actual en SD
5. Al terminar el combate, guardar episodio completo en `/episodes/`

### 1.6 Reward function

```
r = (daño_infligido - daño_recibido) / hp_max_total
    + bonus_si_mato_enemigo (+0.5)
    - penalty_si_muero (-1.0)
    + small_bonus_si_huyo_con_vida (+0.2)
```

Se calcula por turno.

---

## FASE 2: Level Up Trigger (Transiciones = Experiencia)

### 2.1 Un solo trigger: transiciones acumuladas

**No hay dos triggers separados.** La cantidad de transiciones ES la experiencia del pet. El level up ocurre automáticamente cuando se alcanza el umbral de transiciones requerido.

- Nivel 1 → 2: requiere 500 transiciones
- Nivel 2 → 3: requiere 1,000 transiciones
- Nivel 3 → 4: requiere 2,000 transiciones
- Nivel N → N+1: requiere N × 500 transiciones (escala lineal)

El pet **no puede subir de nivel** sin tener suficientes transiciones. Las transiciones son la experiencia literal.

### 2.2 Flujo del trigger

1. Cada combate terminado → incrementar contador de transiciones del pet
2. Cuando contador ≥ umbral del nivel actual → level up habilitado
3. Level up habilitado → el juego muestra "¡Listo para entrenar!"
4. El jugador acepta (o se activa automáticamente) → entrar en modo entrenamiento

---

## FASE 3: Modo Entrenamiento (Bloquea el juego)

### 3.1 El modo entrenamiento es un evento que bloquea TODO

Cuando se activa el modo entrenamiento:
- **Juego bloqueado**: no se procesan combates, no se avanza tiempo
- **LVGL suspendido**: se libera el framebuffer de PSRAM (~150 KB) y se reduce a mínimo (solo pantalla de estado fija)
- **Display**: muestra una pantalla estática con progreso del entrenamiento
- **Todas las tareas del juego pausadas**: coordinator, workers, storage (excepto el entrenador)
- **PSRAM liberada**: todo lo que LVGL usaba queda disponible para buffers de entrenamiento

### 3.2 Asignación de memoria en modo entrenamiento

| Recurso | Modo normal | Modo entrenamiento |
|---------|-------------|---------------------|
| LVGL framebuffer | ~150 KB PSRAM | ~10 KB PSRAM (pantalla mínima) |
| Sprite buffers | ~200 KB PSRAM | Liberados |
| Brain vectors | Variable PSRAM | Liberados |
| JSON buffers | Variable PSRAM | Liberados |
| **Buffers de entrenamiento** | 0 | ~2 MB PSRAM (activaciones, gradients, batch) |

### 3.3 Arquitectura del entrenador

El entrenador es una **task dedicada** que corre cuando se activa el modo entrenamiento. No es background, es el modo principal del dispositivo.

**Prioridad**: Máxima (task principal durante entrenamiento)
**Core**: Core 0 y Core 1 (ambos cores para entrenamiento)
**Stack**: ~16 KB (más que suficiente para backward pass)

### 3.4 Memoria disponible para entrenamiento

Con PSRAM liberada de LVGL:
- Total PSRAM disponible: ~7.5 MB
- Buffers de entrenamiento:
  - Pesos de heads: ~1 KB
  - Gradients: ~1 KB
  - Optimizer state: ~1 KB
  - Activaciones para backward: ~500 KB
  - Batch de transiciones (leído de SD): ~5 MB (70,000 transiciones × 70 bytes)
  - Temporales: ~1 MB
- **Total: ~6.5 MB** → cabe cómodo en PSRAM liberada

### 3.5 Duración del entrenamiento

Depende de la cantidad de transiciones:
- 1,000 transiciones: ~5-10 minutos
- 10,000 transiciones: ~30-60 minutos
- 100,000 transiciones: ~2-4 horas
- 1,000,000 transiciones: ~8-24 horas

El entrenador muestra progreso en pantalla (epoch actual, loss, tiempo restante estimado).

---

## FASE 4: Modelo de Entrenamiento (PPO)

### 4.1 Arquitectura del modelo

**Actor (Policy Network):**
- Capas base: 8→16→8 (frozen, no se entrenan)
- Head: 8→N_acciones (entrenable)
- Output: probabilidades π(a|s) via softmax

**Critic (Value Network):**
- Capas base: 8→16→8 (frozen, idénticas al Actor)
- Head: 8→1 (entrenable)
- Output: V(s) valor estimado del estado

### 4.2 Algoritmo PPO

**Ciclo de entrenamiento por level up:**

1. Leer chunks de transiciones desde SD
2. Calcular returns G_t para cada transición (con γ=0.99)
3. Para cada mini-batch (64-256 transiciones):
   a. Forward pass con política actual → π_nueva(a|s)
   b. Forward pass con política vieja → π_vieja(a|s) (se guarda en buffer)
   c. Calcular ratio = π_nueva / π_vieja
   d. Calcular advantage = G_t - V(s)
   e. Loss_actor = -min(ratio × advantage, clip(ratio, 0.8, 1.2) × advantage)
   f. Loss_critic = MSE(V(s), G_t)
   g. Backward pass manual (solo en heads)
   h. Actualizar pesos de heads con SGD + momentum
4. Repetir K epochs (ej: 4-10) sobre todos los chunks
5. Validar: si loss > threshold → revertir a checkpoint anterior
6. Si loss convergió → guardar checkpoint, notificar level up completo

### 4.3 Backward pass manual (sin autograd)

Implementar en C puro:
- Forward: Linear(x, W, b) + ReLU (solo para capas base en inferencia)
- Backward del Linear: grad_W = input^T × grad_output, grad_b = sum(grad_output)
- Backward del ReLU: grad_input = grad_output * (x > 0)
- SGD con momentum: v = momentum × v - lr × grad, w = w + v
- Loss backward: Softmax + CrossEntropy gradientes

### 4.4 Hiperparámetros sugeridos

| Parámetro | Valor | Notas |
|-----------|-------|-------|
| Learning rate | 1e-3 a 1e-4 | Más bajo para heads que para red completa |
| Gamma (γ) | 0.99 | Factor de descuento |
| Lambda (λ) | 0.95 | Para Generalized Advantage Estimation |
| Clip range | 0.2 | PPO standard (0.8 - 1.2 en ratio) |
| Epochs por update | 4-8 | Más epochs = más uso de datos |
| Batch size | 64-256 | Depende de memoria disponible |
| Momentum (SGD) | 0.9 | Estándar |
| Max gradient norm | 0.5 | Gradient clipping para estabilidad |

---

## FASE 5: Expansión del Espacio de Acciones

### 5.1 Cuando se desbloquea una acción nueva

Ejemplo: pet sube de nivel 5 → desbloquea "HECHIZO"

1. Expandir Actor head: 8→3 se convierte en 8→4
2. Inicializar pesos de la neurona nueva (HECHIZO) copiando los pesos de la acción más similar (ej: ATACAR si es ofensiva)
3. Expandir Critic head si es necesario
4. Activar contador de exploración forzada para la acción nueva en SD

### 5.2 Sistema de exploración forzada

Cada acción nueva tiene un contador de "usos forzados" restantes:
- Al desbloquear: contador = 50-100 usos forzados
- En cada combate, cuando se elige acción:
  - Si hay acciones con usos forzados > 0:
    - Con probabilidad del 20% → forzar uso de esa acción
    - Decrementar contador
  - Si no → seguir política normal del modelo
- Cuando contador llega a 0 → la acción se integra normalmente a la política

### 5.3 Persistencia del contador

Todo en SD:
- Guardar contadores en `/sdcard/BRAIN/COMBAT/exploration/counters.bin`
- Si el dispositivo se reinicia, los contadores persisten en SD
- El entrenamiento solo se ejecuta cuando todos los contadores llegaron a 0 (todas las acciones nuevas fueron exploradas suficiente)

---

## FASE 6: Integración con el Sistema Existente

### 6.1 Modificaciones al game_coordinator.c

Agregar al loop de combate:
- Lógica de exploración forzada (chequear contadores antes de usar modelo)
- Registro de transiciones al buffer (post-acción)
- Cálculo de reward por turno
- Detección de level up → activar modo entrenamiento

### 6.2 Nuevos archivos en components/mistolito_core/

| Archivo | Propósito |
|---------|-----------|
| `replay_buffer.c/h` | Buffer en SD con chunks, lectura/escritura |
| `ppo_trainer.c/h` | Algoritmo PPO, forward/backward manual |
| `experience_logger.c/h` | Registro de transiciones durante combate |
| `reward_calculator.c/h` | Cálculo de reward por turno |
| `exploration_manager.c/h` | Gestión de exploración forzada |
| `combat_ai_manager.c/h` | Orquestador: level up → expandir → entrenar |
| `training_mode.c/h` | Gestión del modo entrenamiento (pantalla, memoria) |

### 6.3 Estructura de tareas FreeRTOS

**Modo normal:**
```
coordinator (Core 0, prio 4) → state machine, combate, UI
storage (Core 0, prio 1) → SD card ops
display (Core 1, prio 5) → LVGL rendering
```

**Modo entrenamiento:**
```
display_training (Core 1, prio 2) → pantalla de progreso mínima
ppo_trainer (Core 0 y 1, prio 5) → entrenamiento PPO completo
storage (Core 0, prio 1) → SD card ops para chunks
```

---

## FASE 7: Validación y Monitoreo

### 7.1 Métricas a registrar en SD

Para cada ciclo de entrenamiento:
- Epoch number
- Loss total (actor + critic)
- Loss actor (promedio)
- Loss critic (promedio)
- Clips aplicados (cuántas veces se disparó el clip)
- Tasa de aprendizaje efectiva
- Número de transiciones procesadas
- Duración del entrenamiento
- Checkpoint aceptado o rechazado

### 7.2 Validación post-entrenamiento

Antes de aceptar un checkpoint:
1. Evaluar política actual contra buffer de validación
2. Si accuracy > umbral (ej: 60%) → aceptar
3. Si accuracy < umbral → rechazar, revertir

---

## FASE 8: Referencias y Recursos de Implementación

### 8.1 ESP-DL (Espressif) - Solo inferencia

ESP-DL es la biblioteca oficial de Espressif para inferencia de modelos en ESP32. **No soporta entrenamiento on-device.**

- **GitHub**: https://github.com/espressif/esp-dl
- **Documentación**: https://docs.espressif.com/projects/esp-dl
- **Capacidades**: Load/run `.espdl`, operadores eficientes (Conv2d, Gemm), planificador de memoria estática, INT8 quantization via ESP-PPQ
- **Limitación**: Sin backward pass, sin gradientes, sin optimizadores

> "Neither ESP-DL nor ESP-NN supports on-device training." — arXiv paper (2604.23012)

### 8.2 RLtools - PPO completo para microcontroladores

Biblioteca C++ header-only que ya tiene PPO implementado y probado en ESP32.

- **GitHub**: https://github.com/rl-tools/rl-tools (987 stars)
- **Paper**: https://arxiv.org/abs/2306.03530
- **Documentación**: https://docs.rl.tools
- **Plataformas probadas**: ESP32 (Xtensa LX7), ESP32-C3 (RISC-V), Teensy 4.1, Crazyflie, Pixhawk 6C
- **Algoritmos**: PPO, SAC, TD3
- **Memoria**: Opción estática (`RL_TOOLS_STATIC_MEM`) para microcontroladores
- **Benchmark ESP32**: Inferencia a 3.6 kHz con DSP optimization
- **TinyRL**: Primera demo de entrenamiento de RL directamente en microcontrolador

**Parámetros PPO del paper (RLtools):**

| Parámetro | Valor |
|-----------|-------|
| Actor | [64, 64] ReLU |
| Critic | [64, 64] ReLU |
| Batch size | 256 |
| Epochs | 2 |
| γ (gamma) | 0.9 |
| GAE λ | 0.95 |
| ε clip | 0.2 |
| Adam α | 1e-3 |

### 8.3 Implementaciones de Backpropagation en C

Referencias para backward pass manual en C puro:

| Repo | Descripción | Link |
|------|-------------|------|
| `exactful/neural-network-in-c` | MNIST en C puro, forward+backward+weight update | https://github.com/exactful/neural-network-in-c |
| `mounirouadi/Deep-Neural-Network-in-C` | Red profunda en C con save/load weights (40★) | https://github.com/mounirouadi/Deep-Neural-Network-in-C |
| `BobbyAnguelov/NeuralNetwork` | Backprop en C++ bien documentado (117★) | https://github.com/BobbyAnguelov/NeuralNetwork |
| `jakezhaojb/Backpropagation-C` | Ejemplo simple de backprop (24★) | https://github.com/jakezhaojb/Backpropagation-C |
| Medium Article | "Building Neural Network Framework in C" con código | https://medium.com/analytics-vidhya/building-neural-network-framework-in-c-using-backpropagation-8ad589a0752d |

**Matemáticas del backward pass (del código de referencia):**

```c
// Forward:
z = W * x + b        // Linear layer
a = relu(z)          // Activation

// Backward:
// Output layer:
delta_output = (a - y) * relu_derivative(z)

// Hidden layer:
delta_hidden = (W_next^T * delta_next) * relu_derivative(z)

// Weight update:
W = W - learning_rate * input * delta
```

### 8.4 PPO-Clip Loss (de CleanRL)

Implementación de referencia del PPO-Clip loss:

```python
# Policy loss (PPO-Clip)
pg_loss1 = -advantages * ratio
pg_loss2 = -advantages * clamp(ratio, 1-clip_coef, 1+clip_coef)
pg_loss = max(pg_loss1, pg_loss2).mean()

# Value loss (con clipping opcional)
v_loss_unclipped = (value - returns) ** 2
v_clipped = old_value + clamp(value - old_value, -clip, clip)
v_loss = 0.5 * max(v_loss_unclipped, (v_clipped - returns) ** 2).mean()

# Entropy bonus
entropy_loss = -log_probs * probs  # Entropy of categorical distribution

# Total loss
loss = pg_loss - ent_coef * entropy_loss + vf_coef * v_loss
```

- **CleanRL PPO**: https://github.com/vwxyzjn/cleanrl/blob/master/cleanrl/ppo_atari.py
- **OpenAI Spinning Up PPO**: https://spinningup.openai.com/en/latest/algorithms/ppo.html
- **PPO Paper**: https://arxiv.org/abs/1707.06347

### 8.5 HEPPO-GAE - FPGA accelerator for PPO

Paper sobre hardware-efficient PPO con GAE en FPGA. Útil como referencia para optimizaciones futuras.

- **Paper**: https://arxiv.org/html/2501.12703v2
- **Key insight**: 8-bit uniform quantization para rewards, 4x reducción de memoria
- **Link**: https://arxiv.org/abs/2501.12703

### 8.6 Deep Reinforcement Learning on ESP32 (Hackster.io)

Proyecto que implementa policy gradient desde scratch en C++ en ESP32.

- **Link**: https://www.hackster.io/aslamahrahiman/deep-reinforcement-learning-on-esp32-843928
- **GitHub**: https://github.com/aslamahrahman/Policy-Gradient-Network-Arduino
- **Key insight**: Bibliotecas de manipulación de matrices escritas en C++ desde cero

### 8.7 On-Device Vision Training (Paper reciente)

Paper que demuestra entrenamiento de visión directamente en microcontrolador. Complementario a nuestro sistema.

- **Paper**: https://arxiv.org/pdf/2604.23012
- **Key insight**: "ESP-DL and ESP-NN are well-suited for deploying pre-trained, quantized production models at high speed. Neither supports on-device training."

### 8.8 Decisión de Implementación

**Decisión: Puro C puro (no RLtools, no ESP-DL)**

Razones:
1. **Modelo tiny** (316 params) - backward pass trivial
2. **Arquitectura simple** - Linear + ReLU + Linear + ReLU + heads
3. **Sin dependencias** - solo math.h (exp, log, sqrt, cos)
4. **Control total** - podemos adaptar a nuestro caso de uso específico
5. **Futuro** - permite agregar complejidad después (más capas, más acciones, etc.)

**Funciones necesarias en C:**

```c
// Forward pass
void linear_forward(float *input, float *weights, float *bias, float *output, int in_size, int out_size);
void relu_forward(float *input, float *output, int size);
void softmax_forward(float *input, float *output, int size);

// Backward pass
void linear_backward(float *input, float *grad_output, float *grad_weights, float *grad_bias, int in_size, int out_size);
void relu_backward(float *input, float *grad_output, float *grad_input, int size);
void softmax_cross_entropy_backward(float *probs, int label, float *grad_input, int size);

// Optimizer
void sgd_update(float *weights, float *grad_weights, float *velocity, float lr, float momentum, int size);

// PPO
void compute_gae(float *rewards, float *values, float *dones, float *advantages, float *returns, int T, float gamma, float lambda);
float ppo_clip_loss(float ratio, float advantage, float clip_coef);
```

---

## FASE 6.5: Sistema de DNA Intentions + Skills (Completada)

### Resumen

El sistema de decisiones del modelo opera en **dos capas**:

```
┌─────────────────────────────────────────────────────┐
│  CAPA 1: MODELO (decide INTENCIÓN)                 │
│  • 6 salidas: ATTACK, DEFEND, HEAL, MAGIC,         │
│    SUPPORT, FLEE                                    │
│  • Modelo base + actor head                         │
│  • Input: 8 features del estado del combate         │
│  • Output: probabilidades π(intent|s)               │
├─────────────────────────────────────────────────────┤
│  CAPA 2: SKILL RESOLVER (ejecuta SKILL)            │
│  • Lee intent_type de cada pet_skill_t              │
│  • Filtra skills elegibles (desbloqueadas + usos)   │
│  • Selecciona la de mayor prioridad                 │
│  • Ejecuta la acción con stats reales del pet       │
└─────────────────────────────────────────────────────┘
```

### DNA: Intenciones por Pet

Cada pet tiene un `dna_t` con campo `intent_unlock[6]`:

| Índice | Intención | Constante |
|--------|-----------|-----------|
| 0 | Ataque | `INTENT_ATTACK` |
| 1 | Defensa | `INTENT_DEFEND` |
| 2 | Curación | `INTENT_HEAL` |
| 3 | Magia | `INTENT_MAGIC` |
| 4 | Soporte | `INTENT_SUPPORT` |
| 5 | Huida | `INTENT_FLEE` |

- `intent_unlock[i] > 0` → la intención está desbloqueada
- Cada pet puede tener diferentes intenciones según su DNA
- Las intenciones varían por pet, no están hardcodeadas

### Skills: Campo `intent_type`

Cada skill tiene un campo `intent_type` en su JSON que indica a qué intención pertenece:

```json
{
  "id": 411,
  "name": "Power Attack",
  "intent_type": "attack",
  "category": "heavy_strike",
  "effect": { "damage_dice": 10, "damage_bonus": 3 }
}
```

El `intent_type` se lee del JSON al aprender la skill y se guarda en `pet_skill_t.intent_type`.

### Flujo de Decisión

```
1. exploration_manager_should_force()
   ├─ SÍ (intención forzada) → skill_resolver_resolve(intent)
   │   └─ Filtra skills donde pet_skill_t.intent_type == intent
   │   └─ Retorna skill_id de mayor prioridad
   └─ NO → inference_engine_run()
       └─ Modelo decide intención (0-5)
       └─ skill_resolver_resolve(intent)
           └─ Retorna skill_id
2. Ejecutar la acción correspondiente a la skill
```

### Reglas de Elegibilidad

- Skill es elegible solo si:
  1. La intención está desbloqueada en DNA (`intent_unlock[intent] > 0`)
  2. La skill tiene `intent_type == intent` en `pet_skill_t`
  3. La skill tiene `uses_remaining > 0` (o es `at_will`)
  4. Cumple `level_req` y `stat_req`
- Si un pet tiene intención desbloqueada, DEBE tener al menos una skill para esa intención

### Archivos Relacionados

| Archivo | Cambio |
|---------|--------|
| `dna_engine.h` | `intent_unlock[6]` en `dna_t` |
| `mistolito.h` | `intent_type` (uint8_t) en `pet_skill_t` |
| `skills_perks_engine.h` | `learn_candidate_t` con `intent_type` |
| `skills_perks_engine.c` | Lee `intent_type` desde JSON, guarda al aprender |
| `storage_task.c` | Save/load de `intent_type` en `pet_data.json` |
| `exploration_manager.c/h` | Contadores de exploración por intención |
| `skill_resolver.c/h` | Mapeo intención → skill específica |
| `skills_warrior.json` | `intent_type` agregado a cada skill |
| `skills_mage.json` | `intent_type` agregado a cada skill |
| `skills_rogue.json` | `intent_type` agregado a cada skill |
| `skills_novice.json` | `intent_type` agregado a cada skill |

---

## Resumen de archivos a crear

### En firmware/components/mistolito_core/

1. `replay_buffer.c` + `replay_buffer.h`
2. `ppo_trainer.c` + `ppo_trainer.h`
3. `experience_logger.c` + `experience_logger.h`
4. `reward_calculator.c` + `reward_calculator.h`
5. `exploration_manager.c` + `exploration_manager.h`
6. `combat_ai_manager.c` + `combat_ai_manager.h`
7. `training_mode.c` + `training_mode.h`

### En inference_engine/tools/ (para PC)

8. `export_base_model.py` → exporta capas base + heads por separado
9. `validate_checkpoint.py` → valida checkpoints en PC antes de deploy

### En inference_engine/models/

10. `policy_head_base.bin` → pesos iniciales del Actor head
11. `value_head_base.bin` → pesos iniciales del Critic head

---

## Orden de implementación

1. **Replay buffer** (SD, chunks, lectura/escritura) ✅
2. **Reward calculator** (reward function por turno) ✅
3. **Experience logger** (registro de transiciones en combate) ✅
4. **Exportador de heads** (separar modelo en base + heads) ✅
5. **Training mode** (gestión de pantalla, memoria, pausa del juego) ✅
6. **PPO trainer** (backward manual, SGD, clipping) ✅
7. **Exploration manager** (contadores en SD, forzado aleatorio) ✅
8. **Skill resolver** (mapeo intención → skill) ✅
9. **Combat AI manager** (orquestador: level up → expandir → entrenar)
10. **Integración con game_coordinator.c** (modificar combate loop)
11. **Validación y monitoreo** (métricas en SD)

---

## FASE 6: Implementación del PPO Trainer (Completada)

### 6.1 Archivos creados

- `firmware/components/mistolito_core/ppo_trainer.h` - Header con tipos y API
- `firmware/components/mistolito_core/ppo_trainer.c` - Implementación completa
- `firmware/components/mistolito_core/CMakeLists.txt` - Actualizado con `ppo_trainer.c`

### 6.2 Arquitectura implementada

**Estructuras de datos:**
- `ppo_config_t` - Hiperparámetros (lr, gamma, lambda, clip, etc.)
- `ppo_layer_t` - Capa con weights, bias, grads, velocity
- `ppo_model_t` - Actor (8→3) + Critic (8→1) + base features
- `ppo_trajectory_t` - Transición con state, action, reward, done, old_log_prob, value, advantage, return
- `ppo_checkpoint_meta_t` - Metadata de checkpoint (magic, version, epoch, loss, timestamp)

**Funciones de red neuronal:**
- `linear_forward()` - Forward pass de capa lineal
- `linear_backward()` - Backward pass con acumulación de gradientes
- `relu_forward()` / `relu_backward()` - Activación ReLU
- `softmax_forward()` - Softmax numerical stable
- `softmax_cross_entropy_backward()` - Gradient de CE loss

**Optimizador:**
- `sgd_update()` - SGD con momentum + gradient clipping por norma global
- `clip_gradients()` - Clips gradientes si norma > max_grad_norm
- `compute_grad_norm()` - Calcula norma L2 de gradientes

**PPO:**
- `compute_gae()` - Generalized Advantage Estimation
- `ppo_compute_actor_loss()` - PPO-Clip loss
- `ppo_compute_critic_loss()` - MSE loss para value
- `ppo_compute_entropy()` - Entropy bonus

**Checkpoint:**
- `ppo_save_checkpoint()` - Guarda weights + bias + meta en SD

### 6.3 Decisiones de implementación

1. **Sin fallbacks** - Falla inmediata en errores de memoria, SD, o validación
2. **Logging detallado** - Cada error incluye contexto (función, tamaño, path)
3. **PSRAM para todo** - Buffers grandes en PSRAM (batch, trajectory, etc.)
4. **Base features separadas** - Para backward pass futuro en capas base
5. **Gradient accumulation** - Acumula gradientes antes de update para estabilidad

### 6.4 Próximos pasos

1. ~~**Exploration manager** - Contadores de exploración forzada en SD~~ ✅
2. ~~**Skill resolver** - Mapeo intención → skill~~ ✅
3. **Combat AI manager** - Orquestador level up → expandir → entrenar
4. **Integración** - Conectar game_coordinator con PPO trainer
5. **Validación** - Métricas y monitoreo en SD

---

## FASE 7: Exploration Manager + Skill Resolver (Completada)

### 7.1 Archivos creados

- `firmware/components/mistolito_core/exploration_manager.h` - Header con tipos y API
- `firmware/components/mistolito_core/exploration_manager.c` - Implementación completa
- `firmware/components/mistolito_core/skill_resolver.h` - Header con tipos y API
- `firmware/components/mistolito_core/skill_resolver.c` - Implementación completa

### 7.2 Exploration Manager

**Estructuras:**
- `exploration_counter_t` - {intent_type, forced_uses_remaining, total_forced}
- `exploration_header_t` - {magic, version, total_intents, timestamp}

**Funciones:**
- `exploration_manager_init()` - Carga contadores desde SD
- `exploration_manager_should_force()` - Probabilidad 20% si hay usos forzados
- `exploration_manager_consume_use()` - Decrementa contador, persiste en SD
- `exploration_manager_unlock_intent()` - Desbloquea intención nueva (50 usos)
- `exploration_manager_reset()` - Reinicia todos los contadores

**SD path:** `/sdcard/BRAIN/COMBAT/exploration/counters.bin`

### 7.3 Skill Resolver

**Estructuras:**
- `skill_priority_t` - {skill_id, uses_remaining, dp_cost, priority_score}
- `skill_resolver_result_t` - {skill_id, skill_index, success}

**Funciones:**
- `skill_resolver_init()` - Carga skills del pet desde SD
- `skill_resolver_resolve()` - Filtra por intención + elegibilidad, retorna mejor
- `skill_resolver_get_eligible_skills()` - Retorna array de skills elegibles
- `skill_resolver_has_eligible()` - Check rápido de elegibilidad

**Selección:** prioridad por `uses_remaining` (más usos = mayor prioridad), luego por `dp_cost` (menor costo = mayor prioridad).

### 7.4 Integración en game_coordinator.c

```c
// En decide_combat_action():
if (exploration_manager_should_force(pet->intent_unlock, &forced_intent)) {
    exploration_manager_consume_use(forced_intent);
    result = skill_resolver_resolve(pet, forced_intent, ...);
    if (result.success) {
        action = ACTION_SKILL;
        skill_index = result.skill_index;
    }
} else {
    // ... lógica normal del modelo
}
```

### 7.5 Próximos pasos

1. **Combat AI manager** - Orquestador level up → expandir → entrenar
2. **Integración** - Conectar game_coordinator con PPO trainer
3. **Validación** - Métricas y monitoreo en SD
