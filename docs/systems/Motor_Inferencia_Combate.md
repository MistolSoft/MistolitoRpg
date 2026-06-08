# **Motor de Inferencia para IA de Combate**

Este documento describe el sistema de inteligencia artificial para la toma de decisiones tácticas del Pet durante el combate, basado en una red neuronal ligera ejecutada en el ESP32-S3.

---

## **1. Concepto General**

El sistema actual de combate selecciona acciones basándose en probabilidades aleatorias y condiciones básicas (HP bajo, recursos disponibles). El nuevo Motor de Inferencia reemplaza esta lógica con una red neuronal que recibe el estado actual del Pet y del enemigo, y decide la acción óptima entre tres opciones: atacar, defender o huir.

El modelo de inferencia se almacena en la tarjeta SD, permitiendo su actualización sin necesidad de reflashear el firmware. Cada turno de combate, el ESP32-S3 ejecuta el modelo en el motor TFLite Micro, utilizando los recursos de PSRAM y Flash externos disponibles.

---

## **2. Entradas del Modelo (Input Layer)**

La red recibe un vector normalizado que representa el estado completo del Pet y del enemigo en el momento de la decisión.

### **A. Estado del Pet**

| Variable | Rango Original | Descripción |
|----------|----------------|-------------|
| Salud (HP) | 0 - HP máximo | Vida actual del Pet |
| Energía | 0 - Energía máxima | Capacidad de acción restante |
| Fuerza (STR) | 3 - 18 | Atributo físico |
| Destreza (DEX) | 3 - 18 | Atributo de agilidad |
| Constitución (CON) | 3 - 18 | Atributo de resistencia |
| Inteligencia (INT) | 3 - 18 | Atributo mental |
| Sabiduría (WIS) | 3 - 18 | Atributo perceptual |
| Carisma (CHA) | 3 - 18 | Atributo social |
| Nivel (Level) | 1 - 50 | Nivel total del Pet |
| Profesión (Class) | 0 - 3 | Novice, Warrior, Mage, Rogue |
| Armadura (AC) | 10 - 20 | Clase de armadura base |
| Dado de daño | 4 - 12 | Tamaño del dado de ataque base |
| Bono de daño | 0 - 10 | Bonus fijo al daño base |

### **B. Estado del Enemigo**

| Variable | Rango Original | Descripción |
|----------|----------------|-------------|
| Salud (HP) | 0 - HP máximo | Vida actual del enemigo |
| Armadura (AC) | 10 - 20 | Clase de armadura |
| Bono de ataque | 0 - 15 | Modificador para acertar golpes |
| Dado de daño | 4 - 12 | Tamaño del dado de daño enemigo |
| Bono de daño | 0 - 10 | Bonus fijo al daño enemigo |
| Nivel | 1 - 50 | Nivel o tier del enemigo |

### **C. Preprocesamiento**

Todas las variables se normalizan al rango [0.0, 1.0] antes de ingresar a la red. Las variables categóricas (profesión) se codifican como un valor escalar con significado ordinal.

---

## **3. Salidas del Modelo (Output Layer)**

La red produce tres valores de probabilidad que suman 1.0. La acción seleccionada es la de mayor probabilidad.

| Salida | Descripción |
|--------|-------------|
| Atacar | Ejecuta el ataque básico o la rotación automática de skills actual |
| Defender | Reduce el daño recibido durante el próximo turno enemigo |
| Huir | Intenta escapar del combate |

La red no controla la selección de enemigo target ni el uso de recursos especiales (superiority dice, action surge, spell slots). Estos aspectos siguen siendo gestionados por la lógica existente.

---

## **4. Arquitectura de la Red**

El modelo es una red feed-forward completamente conectada (dense), diseñada para ser ejecutada eficientemente en el tensor array del ESP32-S3.

| Capa | Neuronas | Activación |
|------|----------|------------|
| Entrada | ~20 | Normalización |
| Oculta 1 | 32 | ReLU |
| Oculta 2 | 16 | ReLU |
| Salida | 3 | Softmax |

### **Cuantización**

El modelo se convierte a formato TFLite con cuantización post-training a enteros de 8 bits (int8). Esto reduce el tamaño del modelo y acelera la inferencia en hardware sin unidad de punto flotante dedicada.

### **Tamaño estimado**

Entre 8 KB y 20 KB una vez cuantizado, con un tensor arena de inferencia de aproximadamente 16 KB.

---

## **5. Almacenamiento y Carga**

| Elemento | Ubicación |
|----------|-----------|
| Modelo TFLite cuantizado | `/sdcard/MODELS/combat_ai.tflite` |
| Tensor arena (runtime) | PSRAM (heap_caps_malloc con MALLOC_CAP_SPIRAM) |
| Buffer del modelo | PSRAM o Flash según tamaño |

El modelo se lee desde la tarjeta SD al iniciar el sistema o al entrar al estado de combate. Si el archivo no existe, el sistema cae en la lógica de decisión aleatoria actual.

---

## **6. Ciclo de Decisión en Combate**

Por cada turno de combate, el flujo de decisión es:

1. El coordinador del juego recolecta el estado actual del Pet y del enemigo activo
2. Los valores se normalizan y se empaquetan en el tensor de entrada
3. El motor TFLite Micro ejecuta la inferencia
4. La salida Softmax se evalúa y se selecciona la acción con mayor probabilidad
5. La acción seleccionada se ejecuta (atacar, defender o huir)

El motor de inferencia no bloquea el loop principal; el tiempo de ejecución depende del tamaño del modelo pero se espera que sea inferior a 50 ms por inferencia.

---

## **7. Dependencias del Sistema**

| Componente | Propósito |
|------------|-----------|
| ESP-TFLite-Micro | Motor de inferencia TFLite para microcontroladores |
| esp-nn | Librería de kernels optimizados para Tensilica |
| SDMMC / VFS | Lectura del archivo .tflite desde la tarjeta SD |
| PSRAM | Memoria para tensor arena y buffer del modelo |

---

## **8. Evolución Futura**

Este sistema está diseñado para extenderse progresivamente:

- **Modelo de selección de skills**: una red separada que elige qué skill o spell usar entre los disponibles
- **Modelo de selección de target**: una red que decide qué enemigo atacar en combates múltiples
- **Modelo de gestión de recursos**: una red que decide cuándo gastar recursos especiales (action surge, superiority dice, spell slots)
- **Modelo de exploración**: una red para decidir direcciones o prioridades durante la fase de búsqueda

Cada modelo adicional será un archivo TFLite independiente en la tarjeta SD, y se ejecutará según el contexto.
