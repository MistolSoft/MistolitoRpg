# Motor de Inferencia para IA de Combate (Arquitectura Híbrida)

> [!IMPORTANT]
> El diseño original basado en modelos monolíticos TFLite Micro (`combat_ai.tflite`) ha sido reemplazado por la **Arquitectura Híbrida de Policy Head**. Este documento especifica el funcionamiento actual del sistema de toma de decisiones del Pet en combate.

---

## 1. Concepto General y Arquitectura

Para permitir el aprendizaje en el dispositivo (on-device learning) sin sobrecargar los recursos limitados del ESP32-S3, el motor de inferencia se divide en una parte estática y otra dinámica (entrenable):

```
Input (9 states: 8 normalizados + 1 quality_score)
    ↓
ESP-DL Backbone (Estático, cargado desde SD como backbone.espdl)
    ↓
Feature Vector (16 floats)
    ↓
Policy Head (Entrenable en C puro, cargado desde policy_head.bin)
    ↓
Action Scores (N acciones activas)
    ↓
softmax → probabilidades → selección de acción (ACTION_ATTACK / ACTION_SKILL / ACTION_DEFEND / ACTION_FLEE)
```

### Componentes de la Arquitectura:

1. **ESP-DL Backbone (Estático)**: 
   * Ejecutado con aceleración por hardware Xtensa en el ESP32-S3.
   * Carga desde `/sdcard/BRAIN/COMBAT/backbone.espdl`.
   * Recibe un vector de 9 entradas (8 de estado y 1 score del Critic) y produce un vector de características latentes (`16 floats`).
2. **Policy Head (Entrenable en C)**:
   * Consiste en una capa completamente conectada sencilla: una matriz de pesos `W[16 x N]` y un bias `b[N]`, donde `N` es la cantidad de acciones desbloqueadas.
   * Se almacena como un archivo de floats binario simple `/sdcard/BRAIN/COMBAT/policy_head.bin` con su metadata correspondiente.
   * El entrenamiento on-device mediante PPO optimiza únicamente esta capa, haciendo la actualización sumamente ligera (~0.8 KB en total).
3. **Critic Model (C puro, Inferencia fija)**:
   * Recibe las características resumidas del historial de los últimos turnos (`8 features`).
   * Estructura: FC `8 -> 4 -> ReLU -> 4 -> 1 -> tanh`. Produce un score de calidad del combate `[-1.0, 1.0]`.
   * Se pre-entrena en PC y se carga desde `/sdcard/BRAIN/COMBAT/critic.bin`.

---

## 2. Entradas del Modelo (Input Layer)

El Backbone recibe un vector normalizado `[0.0, 1.0]` de 9 floats:

### A. Estado del Pet (Entradas 0-4)
1. **Salud (HP)**: Normalizado con respecto al HP máximo.
2. **Energía**: Normalizado con respecto a la energía máxima.
3. **Nivel**: Nivel del Pet normalizado al rango máximo (1-50).
4. **Profesión**: Escalar ordinal del tipo de clase (Novice, Warrior, Mage, Rogue).
5. **Clase de Armadura (AC)**: Normalizado en base al rango 10-25.

### B. Estado del Enemigo (Entradas 5-7)
6. **Salud del Enemigo (HP)**: Normalizado en base a su HP máximo.
7. **Bono de Ataque del Enemigo**: Normalizado.
8. **Nivel del Enemigo**: Nivel o tier normalizado.

### C. Contexto del Combate (Entrada 8)
9. **Quality Score**: Generado por el Critic Model. Representa el desempeño táctico del Pet en base al historial reciente de turnos (suerte del hit, suerte del daño, ventaja de vida, momentum, etc.).

---

## 3. Salidas y Toma de Decisiones

La salida de la `Policy Head` pasa por una función Softmax para generar una distribución de probabilidad sobre las `N` acciones disponibles. Las acciones son dinámicas y se expanden conforme el Pet sube de nivel (ej. desbloqueando habilidades o conjuros).

Las acciones son gestionadas a través del enumerado de combate:
* **ACTION_ATTACK** (Ataque básico)
* **ACTION_SKILL** (Habilidad o conjuro específico resuelto por `skill_resolver`)
* **ACTION_DEFEND** (Estrategia defensiva)
* **ACTION_FLEE** (Intento de huida)

---

## 4. Entrenamiento On-Device (PPO)

Cuando el Pet acumula suficientes transiciones de combate en el Replay Buffer de la SD, se dispara la fase de **Level Up** y se activa el **Modo Entrenamiento** (`GS_TRAINING`):

1. Se suspende la UI de LVGL y se liberan buffers de renderizado en PSRAM (liberando ~200-350 KB).
2. Se inicia la tarea del entrenador que corre PPO manual directamente en el dispositivo.
3. Se actualizan los pesos `W` y bias `b` de la Policy Head procesando los mini-batches leídos del buffer en la SD.
4. Tras converger, los nuevos pesos se persisten en `/sdcard/BRAIN/COMBAT/policy_head.bin`.
