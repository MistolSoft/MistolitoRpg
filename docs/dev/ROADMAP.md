# ROADMAP: MistolitoRPG

Este documento define las fases de desarrollo y el estado actual del proyecto.

---

## Estado Actual: FASE 0 - Documentación

**Inicio:** 2026-03-28  
**Estado:** Documentación Conceptual COMPLETA

---

## FASE 0: Documentación y Diseño

### Documentación Completada ✓

| Documento | Estado | Descripción |
|-----------|--------|-------------|
| Visión General del Proyecto.md | ✓ | Concepto general y pilares del juego |
| Especificaciones Técnicas Completas.md | ✓ | Hardware ESP32-S3, pines, memoria |
| Arquitectura de Implementación.md | ✓ | Componentes de software y librerías |
| Arquitectura del Cerebro.md | ✓ | Capas cognitivas y motor de decisiones |
| Sistema_de_Reglas_y_RPG.md | ✓ | SRD 5.1 adaptado, atributos, d20, idle |
| Progresion_Skiles_y_Clases.md | ✓ | Level up, ascensión, árboles de skills |
| Sistema_de_Memoria_Fractal.md | ✓ | Base de datos vectorial en SD |
| Protocolo_de_Comunicacion_Narrativa.md | ✓ | Bridge, contexto, LLM |
| DNA_System_Manual.md | ✓ | ADN, block-genome, encriptación |
| Proceso_de_Genesis_y_Materializacion.md | ✓ | Ritual de génesis, incubación |
| Lore_y_Sistema_de_Mundo.md | ✓ | Mundo data-driven, biomas |
| Ciclo_de_Vida_y_Cronología.md | ✓ | Ticks, chronos engine, persistencia |
| Matriz_de_Interacción_de_Sensores.md | ✓ | IMU, touch, cámara, botón, sistema idle |
| Estilo_Visual_y_HUD.md | ✓ | Paleta, layout, pantallas |
| Sistema_de_Vida_y_Muerte.md | ✓ | Ciclo vida/muerte, mundo persistente |
| Economia_de_DP_y_Energia.md | ✓ | Sistema idle, Energía, DP |
| Generacion_Procedural.md | ✓ | Skills, Clases, Biomas por IA |
| Estados_Emocionales_del_Pet.md | ✓ | Estados, triggers, visualización |
| Interaccion_Usuario_Pet.md | ✓ | Detalle de interacciones, bonus |

### Documentación Faltante

**NOTA:** La documentación conceptual está COMPLETA. Los documentos restantes son de implementación técnica.

#### Crítica - Especificación técnica para desarrollo

| Documento | Prioridad | Descripción |
|-----------|-----------|-------------|
| API_Bridge_Spec.md | ALTA | Endpoints, payloads JSON, auth, rate limits, errores |
| Formatos_Archivo_SD.md | COMPLETED | Estructura de /DATA/TABLES/*.bin, empaquetado y compilation flow |
| Protocolo_Embeddings.md | ALTA | Dimensiones del vector, serialización hex, threshold |
| Estructura_Block_Genome.md | ALTA | CBOR + Deflate + AES-256-GCM + Base64 exacto |
| Formato_Sprites.md | ALTA | Formato, resolución, paleta, frames de animación |

#### Firmware - Arquitectura de código

| Documento | Prioridad | Descripción |
|-----------|-----------|-------------|
| Tareas_FreeRTOS.md | MEDIA | Definición de tareas, prioridades, colas, semáforos |
| Maquina_Estados_Cerebro.md | MEDIA | Diagrama: Pleno, Limbo, Sleep, Muerte, Génesis |
| Driver_I2C.md | MEDIA | Protocolo QMI8658 y CST816D |
| Driver_SPI.md | MEDIA | Bus compartido LCD/SD, manejo de CS |
| Integracion_LVGL.md | MEDIA | Display driver, input driver, memoria |

#### Datos - Contenido del juego

| Documento | Prioridad | Descripción |
|-----------|-----------|-------------|
| Catalogo_Biomas.md | MEDIA | Biomas base con modificadores |
| Catalogo_Clases.md | MEDIA | Clases por temática, requisitos |
| Arbol_Skills_Base.md | MEDIA | Skills padre/hijo iniciales |
| Bestiario_Base.md | BAJA | Entidades por tipo de mundo |
| Lista_Perks.md | BAJA | Perks posibles y efectos |

#### Proceso - Flujo de trabajo

| Documento | Prioridad | Descripción |
|-----------|-----------|-------------|
| Pipeline_Assets.md | BAJA | Flujo para crear/integrar sprites |
| Script_Laboratorio.md | MEDIA | Especificación del script de ADN externo |
| Guia_Testing.md | BAJA | Estrategia de pruebas en hardware |

---

## FASE 1: MVP - Mínimo Producto Viable

**Prerrequisito:** FASE 0 completada

### Objetivos

- [ ] Boot sequence funcional
- [ ] Display LVGL operativo
- [ ] Lectura de SD card
- [ ] Touch input básico
- [ ] Sistema de Ticks funcional
- [ ] Vectores de impulso (Hambre, Energía, Humor)
- [ ] Muerte y reset de DB

### Milestones

| ID | Nombre | Descripción |
|----|--------|-------------|
| M1.1 | Hello World | LVGL renderizando en pantalla |
| M1.2 | SD Mount | Lectura/escritura de archivos |
| M1.3 | Touch Basic | Input táctil funcional |
| M1.4 | Tick Engine | Loop principal con cronómetro |
| M1.5 | Drive Vectors | Sistema de necesidades |
| M1.6 | Death Protocol | HP y reset de DB |

---

## FASE 2: Cerebro y Memoria

**Prerrequisito:** FASE 1 completada

### Objetivos

- [ ] Motor de decisiones vectoriales
- [ ] Navegación por similitud de coseno
- [ ] Integración con API de embeddings
- [ ] Bridge con LLM (narrativa)
- [ ] Memoria fractal operativa

### Milestones

| ID | Nombre | Descripción |
|----|--------|-------------|
| M2.1 | Vector Engine | Cálculo de decisiones |
| M2.2 | Cosine Nav | Navegación por similitud |
| M2.3 | Embedding API | Integración externa |
| M2.4 | LLM Bridge | Generación narrativa |
| M2.5 | Fractal Write | Escritura en árbol SD |

---

## FASE 3: Génesis y ADN

**Prerrequisito:** FASE 2 completada

### Objetivos

- [ ] Ritual de Génesis UI
- [ ] Desencriptación de Block-Genome
- [ ] Materialización del mundo
- [ ] Incubación del Pet
- [ ] Primera crónica

### Milestones

| ID | Nombre | Descripción |
|----|--------|-------------|
| M3.1 | Genesis UI | Selección de tríada |
| M3.2 | World Gen | Materialización desde API |
| M3.3 | DNA Decrypt | Validación y desencriptación |
| M3.4 | Pet Spawn | Nacimiento del campeón |

---

## FASE 4: RPG Completo

**Prerrequisito:** FASE 3 completada

### Objetivos

- [ ] Motor d20 completo
- [ ] Sistema de niveles y EXP
- [ ] Skills y especializaciones
- [ ] Ascensión de clase
- [ ] Exploración pasiva

### Milestones

| ID | Nombre | Descripción |
|----|--------|-------------|
| M4.1 | d20 Engine | Tiradas con modificadores |
| M4.2 | Level Up | Progresión y stats |
| M4.3 | Skill Tree | Desbloqueo y especialización |
| M4.4 | Ascension | Ritual de clase |
| M4.5 | Idle Explore | Exploración autónoma |

---

## FASE 5: Sensores y Pulido

**Prerrequisito:** FASE 4 completada

### Objetivos

- [ ] IMU completamente integrado
- [ ] Cámara funcional (Vision Divina)
- [ ] Animaciones del Pet
- [ ] Efectos visuales
- [ ] Optimización de memoria

### Milestones

| ID | Nombre | Descripción |
|----|--------|-------------|
| M5.1 | IMU Full | Movimiento completo |
| M5.2 | Camera | Vision Divina |
| M5.3 | Animations | Sprites animados |
| M5.4 | Effects | Glitch, typewriter |
| M5.5 | Optimize | PSRAM optimization |

---

## Notas

- Cada fase depende de la anterior
- Los milestones pueden paralelizarse dentro de su fase
- La documentación debe aprobarse antes de pasar a FASE 1
