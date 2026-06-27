# Motor de Inferencia - MistolitoRPG

Herramientas para generar el dataset sintético, entrenar y exportar el modelo de red neuronal para la IA de combate del Pet.

## Estructura

```
inference_engine/
├── data/
│   ├── tables/          # Copia de las tablas JSON del firmware (enemies, skills, etc.)
│   └── datasets/        # Datasets generados (CSV)
├── models/              # Modelos entrenados (.h5, .tflite)
├── tools/               # Scripts de utilidad
├── venv/                # Entorno virtual Python
└── requirements.txt     # Dependencias
```

## Uso

1. Activar el entorno virtual
2. Correr los scripts en orden
3. El modelo .tflite generado se copia a la SD del ESP32
