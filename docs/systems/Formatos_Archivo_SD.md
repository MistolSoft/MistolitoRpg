# Formatos de Archivos en la SD y Estructuras Binarias

MistolitoRPG utiliza un sistema de tablas de juego estructuradas en binario para optimizar el acceso en el hardware ESP32-S3. Esto elimina la necesidad de parsear archivos JSON grandes en tiempo de ejecución, reduciendo a cero la fragmentación del heap y el consumo excesivo de RAM.

Las tablas se definen originalmente en archivos JSON en la PC y son compiladas a binario antes de subirse a la tarjeta SD.

## Estructura de Directorios en la SD

El sistema de archivos de la tarjeta SD (`/sdcard`) organiza las tablas de juego en el directorio `/DATA/TABLES/`:

```
/sdcard/
├── DATA/
│   └── TABLES/
│       ├── config.bin               # Parámetros de configuración global
│       ├── professions.bin          # Definiciones y stats base de las profesiones
│       ├── enemies.bin              # Listado y progresiones de enemigos
│       ├── enemy_tiers.bin          # Tiers de enemigos y rangos de nivel
│       ├── transition_intervals.bin # Rangos de transiciones para subir nivel
│       ├── skills.bin               # Habilidades activas y pasivas
│       ├── perks.bin                # Perks aprendibles
│       ├── spells.bin               # Conjuros utilizables por la profesión mago
│       ├── features.bin             # Rasgos y habilidades desbloqueables
│       ├── resources.bin            # Progresiones de recursos por nivel
│       └── damage_progression.bin   # Progresiones de daño y dados por nivel
│
└── CONFIG/
    └── state.json                   # Estado persistido no estructurado (DP, vidas, etc.)
```

---

## Estructuras Binarias (C / Python Structs)

Todos los archivos binarios de tabla se estructuran mediante registros de tamaño fijo y están empacados (`__attribute__((packed))`). A continuación se detallan las estructuras implementadas en C en `game_tables_structs.h`:

### 1. Configuración Global (`config.bin`)
*   **Tamaño del registro:** 16 bytes.
*   **Estructura C:**
```c
typedef struct {
    uint32_t cycle_length;
    uint32_t max_stats_per_level;
    uint32_t max_skills_per_level;
    uint32_t base_stat_dc;
} __attribute__((packed)) config_record_t;
```

### 2. Profesiones (`professions.bin`)
*   **Tamaño del registro:** 43 bytes.
*   **Estructura C:**
```c
typedef struct {
    uint8_t id;
    char name[16];
    uint8_t req_str;
    uint8_t req_con;
    uint8_t req_dex;
    uint8_t req_int;
    uint8_t req_wis;
    uint8_t req_cha;
    uint8_t bonus_str;
    uint8_t bonus_con;
    uint8_t bonus_dex;
    uint8_t bonus_int;
    uint8_t bonus_wis;
    uint8_t bonus_cha;
    uint8_t success_dc;
    uint8_t dp_cost;
    uint8_t hp_rest_threshold;
    uint8_t recovery_chance;
    uint8_t base_hp;
    uint8_t base_energy;
    uint8_t base_ac;
    uint8_t damage_dice;
    uint8_t damage_bonus;
    uint8_t dice_count;
    uint8_t hit_dice;
    uint8_t hp_per_level;
} __attribute__((packed)) profession_record_t;
```

### 3. Enemigos (`enemies.bin`)
*   **Tamaño del registro:** 31 bytes.
*   **Estructura C:**
```c
typedef struct {
    uint8_t id;
    char name[16];
    uint8_t tier;
    uint8_t base_hp;
    uint8_t hp_per_level;
    uint8_t base_ac;
    uint8_t ac_per_level;
    uint8_t damage_dice;
    uint8_t damage_bonus;
    uint8_t damage_per_level;
    uint8_t attack_bonus;
    uint8_t exp_base;
    uint8_t exp_per_level;
} __attribute__((packed)) enemy_record_t;
```

### 4. Tiers de Enemigos (`enemy_tiers.bin`)
*   **Tamaño del registro:** 3 bytes.
*   **Estructura C:**
```c
typedef struct {
    uint8_t tier;
    uint8_t min_level;
    uint8_t max_level;
} __attribute__((packed)) enemy_tier_record_t;
```

### 5. Intervalos de Transición (`transition_intervals.bin`)
*   **Tamaño del registro:** 8 bytes.
*   **Estructura C:**
```c
typedef struct {
    uint32_t max_level;
    uint32_t transitions_per_level;
} __attribute__((packed)) transition_interval_record_t;
```

### 6. Habilidades (`skills.bin`) y Perks (`perks.bin`)
*   **Requisitos de Atributos:**
```c
typedef struct {
    uint8_t str;
    uint8_t con;
    uint8_t dex;
    uint8_t intel;
    uint8_t wis;
    uint8_t cha;
} __attribute__((packed)) stat_req_t;
```
*   **Habilidad (Skill):**
```c
typedef struct {
    uint8_t id;
    char name[32];
    uint8_t profession_level_req;
    uint8_t profession_req_mask;
    stat_req_t stat_req;
    uint8_t dp_cost;
    uint8_t success_dc;
    uint8_t intent_type;
} __attribute__((packed)) skill_record_t;
```
*   **Perk:**
```c
typedef struct {
    uint8_t id;
    char name[32];
    uint8_t profession_level_req;
    uint8_t profession_req_mask;
    stat_req_t stat_req;
    uint8_t dp_cost;
    uint8_t success_dc;
} __attribute__((packed)) perk_record_t;
```

### 7. Conjuros (`spells.bin`)
*   **Tamaño del registro:** 51 bytes.
*   **Estructura C:**
```c
typedef struct {
    char id[16];
    char name[32];
    uint8_t level;
    uint8_t dp_cost;
    uint8_t success_dc;
} __attribute__((packed)) spell_record_t;
```

### 8. Rasgos / Arquetipos (`features.bin`)
*   **Tamaño del registro:** 50 bytes.
*   **Estructura C:**
```c
typedef struct {
    char name[32];
    uint8_t profession;
    uint8_t level;
    uint8_t dp_cost;
    uint8_t success_dc;
    char archetype_req[14];
} __attribute__((packed)) feature_record_t;
```

### 9. Recursos por Nivel (`resources.bin`)
*   **Tamaño del registro:** 15 bytes.
*   **Estructura C:**
```c
typedef struct {
    uint8_t action_surge_max;
    uint8_t indomitable_max;
    uint8_t superiority_dice_max;
    uint8_t superiority_dice_size;
} __attribute__((packed)) warrior_lvl_res_t;

typedef struct {
    uint8_t slots[9];
} __attribute__((packed)) mage_lvl_res_t;

typedef struct {
    uint8_t sneak_attack_dice;
} __attribute__((packed)) rogue_lvl_res_t;

typedef struct {
    uint8_t level;
    warrior_lvl_res_t warrior;
    mage_lvl_res_t mage;
    rogue_lvl_res_t rogue;
} __attribute__((packed)) resource_record_t;
```

---

## Proceso de Compilación y Transferencia

La generación de estos archivos binarios se delega a un pipeline local que se ejecuta en la PC del desarrollador:

1.  **Edición**: Se modifican las tablas en formato JSON dentro de `firmware/data` (por ejemplo, `game_tables.json` o `tables/enemies.json`).
2.  **Compilación**: El script de Python `scripts/compile_tables.py` parsea la estructura JSON y escribe los bytes exactos utilizando la librería nativa de Python `struct`.
3.  **Transmisión**: El script de cargador serial `scripts/send_init_files.py` primero invoca a `compile_tables.py` de manera transparente y luego realiza el envío por USB de cada `.bin` a la ruta final en la SD.

Este desacoplamiento permite mantener la facilidad de uso y legibilidad de JSON durante el desarrollo de contenido de RPG, sin penalizar la velocidad y consumo del microcontrolador.
