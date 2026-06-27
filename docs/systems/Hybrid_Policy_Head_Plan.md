# Plan: Hybrid Policy Head Architecture

## Overview

Implementar una arquitectura híbrida donde:
- **ESP-DL** se usa para inference del backbone estático
- **Policy Head** (capa entrenable) se implementa en C puro
- El entrenamiento on-device fine-tunea solo la Policy Head
- Expansión dinámica: cuando sube de nivel, solo se redimensiona la matriz de pesos

## Architecture

```
Input (8 states)
    ↓
ESP-DL Backbone (estático, .espdl)
    ↓
Feature Vector (16 floats)
    ↓
Policy Head (trainable, C puro)
    ↓
Action Scores (N actions)
    ↓
softmax → probabilities → decide action → skill_resolver
```

## Critic Features (Combat History)

El critic recibe **features pre-calculadas** de los últimos 3-5 turnos, no turnos raw. Cada feature se normaliza a [-1, +1] usando fórmulas específicas que reflejan la asimetría riesgo/recompensa.

### Feature 1: Luck_hit (Suerte del Ataque)

Mide si superaste la probabilidad matemática de acertar.

```
if (hit)
    luck = 1.0f - p_success;
else
    luck = -p_success;
```

| P_success | hit | miss |
|-----------|-----|------|
| 0.05 (arriesgado) | +0.95 | -0.05 |
| 0.25 | +0.75 | -0.25 |
| 0.50 | +0.50 | -0.50 |
| 0.75 | +0.25 | -0.75 |
| 0.95 (seguro) | +0.05 | -0.95 |

- p_success = max(0, min(1, (d20 + attack_bonus - AC + 1) / 20))

### Feature 2: Luck_dmg (Suerte del Daño)

Mide la calidad del roll de daño, independiente del hit.

```
luck = ((dmg_real - dmg_min) / (dmg_max - dmg_min)) * 2.0f - 1.0f;
```

| dmg (arma 2d6) | luck |
|----------------|------|
| 2 (mínimo) | -1.0 |
| 7 (promedio) | 0.0 |
| 12 (máximo) | +1.0 |

### Feature 3: hp_advantage

```
hp_advantage = (my_hp - enemy_hp) / max_hp;
```

### Feature 4: damage_momentum

```
damage_momentum = (total_damage_dealt - total_damage_taken) / max_hp;
```

### Feature 5: success_rate

```
success_rate = hits / total_turnos;
```

### Feature 6: threat_level

```
threat_level = enemy_avg_dmg / my_hp;
```

### Feature 7: turns_alive

```
turns_alive = current_turn / expected_turns;
```

### Feature 8: win_probability

```
win_probability = my_hp / (my_hp + enemy_hp);
```

---

## Components

### 1. ESP-DL Backbone (Estático)

- Generado en PC con 16 outputs (feature vector)
- Nunca cambia en el dispositivo
- Carga desde `.espdl` en SD
- Output: `features[16]`

### 2. Critic Model (Small, Fixed)

- **Input**: 8 features de combate normalizadas ([-1, +1])
- **Capa 1**: FC(8 → 4) + ReLU
- **Capa 2**: FC(4 → 1) + tanh → quality_score [-1, +1]
- **Parámetros**: 8×4 + 4 + 4×1 + 1 = 41 floats (~164 bytes)
- **Entrenamiento**: en PC con datos de combate, exportado como .bin
- **Uso en ESP32**: solo forward pass, no training

### 3. Policy Head (Trainable)

- Matriz `W[16 × N]` + bias `b[N]`
- `N` = número de acciones desbloqueadas (1-6)
- Tamaño: 16×6×4 bytes = 384 bytes (muy pequeño)
- Operaciones: `scores = W × features + b` (matrix multiply)
- Activation: softmax para convertir a probabilidades

### 4. PPO Trainer (Simplificado)

- Solo entrena `W` y `b` de la Policy Head
- No más actor head expandible en PPO
- Resize de matriz cuando sube de nivel
- Checkpoint = solo `W` y `b` (float32)

## File Structure

```
/sdcard/BRAIN/COMBAT/
    backbone.espdl              # Modelo base (estático, 9 inputs)
    critic.bin                  # Critic model (FC 8→4→1, pesos entrenados en PC)
    actor_init.bin              # Pesos iniciales de la Policy Head (16×N)
    policy_head.bin             # Pesos entrenables (float32)
    policy_head.meta            # Metadata: num_actions, epoch
    replay_a{N}/                # Replay buffer por action space
    checkpoints/                # Checkpoints de Policy Head
```

## Data Structures

### combat_turn_t
```c
typedef struct {
    bool hit;                   // ¿Acertó el ataque?
    float p_success;            // Probabilidad de éxito (0-1)
    float dmg_real;             // Daño real infligido
    float dmg_min;              // Daño mínimo posible
    float dmg_max;              // Daño máximo posible
    float my_hp;                // HP del pet después del turno
    float enemy_hp;             // HP del enemigo después del turno
    float my_damage_taken;      // Daño recibido en este turno
    uint8_t action_taken;       // Acción ejecutada
} combat_turn_t;
```

### combat_history_t
```c
#define MAX_HISTORY 5

typedef struct {
    combat_turn_t turns[MAX_HISTORY];
    uint8_t count;              // Turnos almacenados (0-5)
    uint8_t total_turns;        // Turnos totales del combate
    uint8_t hits;               // Total de aciertos
    float total_damage_dealt;   // Daño total infligido
    float total_damage_taken;   // Daño total recibido
} combat_history_t;
```

### critic_model_t
```c
typedef struct {
    float W1[8 * 4];           // Capa 1: 8→4
    float b1[4];               // Bias capa 1
    float W2[4 * 1];           // Capa 2: 4→1
    float b2[1];               // Bias capa 2
} critic_model_t;
```

### policy_head_t
```c
typedef struct {
    float *W;           // [16 x num_actions]
    float *b;           // [num_actions]
    uint8_t num_actions;
} policy_head_t;
```

### policy_head.meta (on SD)
```c
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint8_t num_actions;
    uint32_t epoch;
    uint32_t checksum;
} policy_head_meta_t;
```

## Workflow by Phase

### Phase 1: PC Setup (antes de flashear)

**Tarea 1.1: Modificar export_base_model.py**
- Cambiar el export para que outputtee 16 features (no 3 actions)
- El actor head exportado será el init para la Policy Head
- Generar: `backbone.espdl` + `actor_init.bin` (16×6 floats)

**Tarea 1.2: Entrenar modelo base completo**
- Entrenar 9→16→8 backbone + 16→6 policy head
- Extraer pesos de la policy head para `actor_init.bin`
- Guardar para subir a SD

**Tarea 1.3: Entrenar critic model**
- Crear dataset de combates con features calculadas
- Entrenar FC(8→4→1) para predecir quality_score
- Exportar pesos a `critic.bin` para subir a SD

### Phase 2: ESP32 Boot

**Tarea 2.1: Cargar backbone**
- ESP-DL carga `backbone.espdl` de SD
- Listo para inference

**Tarea 2.2: Cargar Critic Model**
- Si existe `critic.bin` en SD → cargar a RAM
- El critic no se entrena en ESP32, solo se usa para inference

**Tarea 2.3: Cargar Policy Head**
- Si existe `policy_head.bin` en SD → cargar a RAM
- Si no existe → cargar `actor_init.bin` (pesos pre-entrenados de PC)
- Verificar que `num_actions` del meta coincida con intent_unlock

### Phase 3: Combat (Inference)

**Tarea 3.1: Calcular critic features**
- Al inicio de cada turno, calcular las 8 features del critic:
  - `luck_hit` = 1 - p_success (si hit) o -p_success (si miss)
  - `luck_dmg` = ((dmg_real - dmg_min) / (dmg_max - dmg_min)) * 2 - 1
  - `hp_advantage`, `damage_momentum`, `success_rate`, `threat_level`, `turns_alive`, `win_probability`
- Actualizar `combat_history` con el turno actual

**Tarea 3.2: Run critic**
- `critic_calc_features(history, features)` → promedio de últimos 3-5 turnos
- `critic_model_forward(critic, features, &quality_score)` → quality_score [-1, +1]

**Tarea 3.3: Concatenate quality_score to state**
- `input[9] = {state[8], quality_score}`

**Tarea 3.4: Run backbone**
- `inference_engine_run(input, 9, features, 16)` → ESP-DL

**Tarea 3.5: Run policy head**
- `policy_head_forward(features, scores)` → tu código C
- `softmax(scores, num_actions)`

**Tarea 3.6: Decision**
- Filtrar actions según `intent_unlock[6]` (ignorar las bloqueadas)
- Elegir mejor score entre las desbloqueadas
- `skill_resolver_resolve(pet, best_action)` → skill_id
- Retornar `ACTION_SKILL`

### Phase 4: Level Up (Expand Model)

**Tarea 4.1: Detectar nuevos desbloqueos**
- En `process_level_up()`, iterar `intent_unlock[i]`
- Si `intent_unlock[i] == new_level` → intención desbloqueada

**Tarea 4.2: Expandir Policy Head**
- Si `N` crece: `realloc(W, 16 × N_new)`
- Inicializar nuevas columnas con valores pequeños (o copiar de columna similar)
- Actualizar `policy_head.num_actions`

**Tarea 4.3: Switch replay buffer**
- Si `N` creció: `storage_replay_set_action_count(N_new)` → dataset limpio

### Phase 5: Training (GS_TRAINING)

**Tarea 5.1: Setup**
- Cargar checkpoint de `policy_head.bin` si existe
- Configurar PPO para entrenar solo la Policy Head

**Tarea 5.2: PPO Training**
- Replay buffer: solo datos del dataset actual (`replay_aN/`)
- Policy Head solo tiene `W[16×N]` + `b[N]` para entrenar
- Muy pocos parámetros → training rápido
- GAE, PPO-Clip loss, etc. funcionan igual

**Tarea 5.3: Save checkpoint**
- Guardar `policy_head.W` + `policy_head.b` → `policy_head.bin`
- Guardar metadata → `policy_head.meta`
- Opcional: guardar checkpoint epoch para rollback

### Phase 6: Death/Rebirth

**Tarea 6.1: Reset on death**
- Borrar `policy_head.bin` (reiniciar a pesos pre-entrenados)
- O: cargar `actor_init.bin` de nuevo
- `storage_replay_reset()` → borra replay buffer
- `storage_checkpoints_reset()` → borra checkpoints

## Key Functions to Implement

### New Functions

```c
// Critic Features
float critic_calc_luck_hit(bool hit, float p_success);
float critic_calc_luck_dmg(float dmg_real, float dmg_min, float dmg_max);
void critic_calc_features(combat_history_t *history, float features[8]);
void critic_update_history(combat_history_t *history, combat_turn_t *turn);

// Critic Model
esp_err_t critic_model_init(critic_model_t *critic);
esp_err_t critic_model_forward(critic_model_t *critic, const float features[8], float *quality_score);
esp_err_t critic_model_load(critic_model_t *critic);

// Policy Head
esp_err_t policy_head_init(policy_head_t *ph, uint8_t num_actions);
void policy_head_deinit(policy_head_t *ph);
esp_err_t policy_head_forward(const float *features, float *scores);
esp_err_t policy_head_expand(policy_head_t *ph, uint8_t new_action_idx);
float policy_head_action_score(policy_head_t *ph, const float *features, uint8_t action);

// PPO (simplificado)
esp_err_t ppo_train_policy_head(policy_head_t *ph, const ppo_config_t *config, ...);

// Storage
esp_err_t policy_head_save(policy_head_t *ph);
esp_err_t policy_head_load(policy_head_t *ph);

// Inference glue
combat_action_e decide_combat_action_hybrid(pet_t *pet, enemy_t *enemy);
```

## Differences from Current Implementation

| Antes | Ahora |
|-------|-------|
| PPO tenía su propio modelo completo | PPO solo entrena Policy Head (pequeño) |
| PPO forward para inference | ESP-DL forward para backbone |
| 2 modelos desconectados | 1 backbone + 1 critic + 1 policy head conectados |
| Modelo completo en checkpoint | Solo Policy Head en checkpoint |
| .espdl con 3 outputs | .espdl con 16 features |
| Sin historia de combate | 8 features de historial al backbone |

## Memory Estimate

- **Backbone (ESP-DL)**: cargado en PSRAM por ESP-DL, ~50KB
- **Critic Model**: `W1[8×4] + b1[4] + W2[4×1] + b2[1]` = 41 floats = ~0.2KB
- **Policy Head**: `W[16×6] + b[6]` = 132 floats = ~0.5KB
- **Combat History**: `5 × sizeof(combat_turn_t)` = ~0.5KB
- **PPO memory**: replay buffer sigue en SD, solo加载 para training
- **Total training memory**: << 100KB

## Files to Delete/Modify

### Delete
- `ppo_trainer.c/h` (reemplazado por versión simplificada)
- `combat_ai_heads.h/cpp`
- Referencias a `ppo_model_t` con actor/critic completos

### Modify
- `export_base_model.py` - output 16 features
- `decide_combat_action()` - usar hybrid flow
- `game_coordinator.c` - GS_TRAINING simplificado
- `storage_task.c` - agregar policy_head save/load
- `CMakeLists.txt` - limpiar archivos removidos

### New
- `critic_features.c/h` (luck_hit, luck_dmg, etc.)
- `critic_model.c/h` (forward pass del critic)
- `policy_head.c/h`
- `ppo_train_policy.c/h` (PPO simplificado solo para policy head)

## Questions to Resolve

1. ¿Cuántos features debe outputtear el backbone? (16 es ejemplo)
2. ¿Inicializar nuevos pesos en 0 o random? (0 = todos equally bad, random = exploration)
3. ¿Guardar `actor_init.bin` de PC en flash o SD?

## Milestones

1. [ ] PC: Modificar export para 16 features
2. [ ] PC: Generar `backbone.espdl` + `actor_init.bin`
3. [ ] PC: Entrenar critic model y exportar `critic.bin`
4. [ ] ESP32: Implementar `critic_features.c/h` (luck_hit, luck_dmg, etc.)
5. [ ] ESP32: Implementar `critic_model.c/h` (forward pass)
6. [ ] ESP32: Implementar `policy_head.c/h`
7. [ ] ESP32: Modificar `decide_combat_action()` para hybrid flow con critic
8. [ ] ESP32: Implementar PPO simplificado para policy head
9. [ ] ESP32: Hook level up → expand policy head
10. [ ] ESP32: Hook GS_TRAINING → train policy head
11. [ ] Test: Combat with critic features
12. [ ] Test: Training loop
13. [ ] Test: Level up → expand → train

---

## Annex A: ESP-DL Capabilities and Limitations

**Source:** `PPO_Entrenamiento_OnDevice.md` - Section 8.1

**ESP-DL is inference-only.** No backward pass, no gradients, no optimizers.

From arXiv paper 2604.23012:
> "Neither ESP-DL nor ESP-NN supports on-device training."

| Capability | ESP-DL |
|------------|--------|
| Forward pass | ✅ Yes |
| Backward pass | ❌ No |
| Gradient computation | ❌ No |
| On-device training | ❌ No |
| Quantization (INT8) | ✅ Yes via ESP-PPQ |
| Hardware acceleration | ✅ Yes (Xtensa vector instructions) |

**Implication:** Training must be done in custom C code, not using ESP-DL.

---

## Annex B: Reference Implementations for Training in C

**Source:** `PPO_Entrenamiento_OnDevice.md` - Section 8.3

### Backpropagation Math

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

### Reference Repos for C Neural Networks

| Repo | Description | Stars |
|------|-------------|-------|
| `exactful/neural-network-in-c` | MNIST in pure C, forward+backward | - |
| `mounirouadi/Deep-Neural-Network-in-C` | Deep network in C with save/load | 40★ |
| `BobbyAnguelov/NeuralNetwork` | Backprop in C++ well documented | 117★ |
| `jakezhaojb/Backpropagation-C` | Simple backprop example | 24★ |

---

## Annex C: PPO-Clip Loss Reference

**Source:** `PPO_Entrenamiento_OnDevice.md` - Section 8.4

```python
# Policy loss (PPO-Clip)
pg_loss1 = -advantages * ratio
pg_loss2 = -advantages * clamp(ratio, 1-clip_coef, 1+clip_coef)
pg_loss = max(pg_loss1, pg_loss2).mean()

# Value loss (with optional clipping)
v_loss_unclipped = (value - returns) ** 2
v_clipped = old_value + clamp(value - old_value, -clip, clip)
v_loss = 0.5 * max(v_loss_unclipped, (v_clipped - returns) ** 2).mean()

# Entropy bonus
entropy_loss = -log_probs * probs

# Total loss
loss = pg_loss - ent_coef * entropy_loss + vf_coef * v_loss
```

**References:**
- CleanRL PPO: https://github.com/vwxyzjn/cleanrl
- OpenAI Spinning Up: https://spinningup.openai.com
- PPO Paper: https://arxiv.org/abs/1707.06347

---

## Annex D: Hyperparameters

**Source:** `PPO_Entrenamiento_OnDevice.md` - Section 4.4

| Parameter | Value | Notes |
|-----------|-------|-------|
| Learning rate | 1e-3 to 1e-4 | Lower for heads than full network |
| Gamma (γ) | 0.99 | Discount factor |
| Lambda (λ) | 0.95 | GAE parameter |
| Clip range | 0.2 | PPO standard (0.8 - 1.2 in ratio) |
| Epochs per update | 4-8 | More epochs = more data usage |
| Batch size | 64-256 | Depends on available memory |
| Momentum (SGD) | 0.9 | Standard |
| Max gradient norm | 0.5 | Gradient clipping for stability |

### From RLtools paper (tested on ESP32):

| Parameter | Value |
|-----------|-------|
| Actor | [64, 64] ReLU |
| Critic | [64, 64] ReLU |
| Batch size | 256 |
| Epochs | 2 |
| γ (gamma) | 0.9 |
| GAE λ | 0.95 |
| ε clip | 0.2 |
| Adam α | 1e-3 |

---

## Annex E: Memory Allocation During Training

**Source:** `PPO_Entrenamiento_OnDevice.md` - Section 3.2

| Resource | Normal Mode | Training Mode |
|----------|-------------|---------------|
| LVGL framebuffer | ~150 KB PSRAM | ~10 KB PSRAM (minimal) |
| Sprite buffers | ~200 KB PSRAM | Released |
| Brain vectors | Variable PSRAM | Released |
| JSON buffers | Variable PSRAM | Released |
| **Training buffers** | 0 | ~2 MB PSRAM |

**Available during training:** ~7.5 MB PSRAM

Training buffers allocation:
- Weights of heads: ~1 KB
- Gradients: ~1 KB
- Optimizer state: ~1 KB
- Activations for backward: ~500 KB
- Batch of transitions (from SD): ~5 MB
- Temporaries: ~1 MB
- **Total: ~6.5 MB**

---

## Annex F: Policy Head Size Calculation

For `features=16` and `max_actions=6`:

```
W matrix: 16 × 6 = 96 floats × 4 bytes = 384 bytes
b vector: 6 floats × 4 bytes = 24 bytes
Total: 408 bytes (~0.4 KB)
```

For training (gradients + velocity):
- W gradients: 384 bytes
- b gradients: 24 bytes
- W velocity: 384 bytes
- b velocity: 24 bytes
- Total per training step: ~816 bytes (~0.8 KB)

**This is trivially small for ESP32-S3 PSRAM (8MB available).**