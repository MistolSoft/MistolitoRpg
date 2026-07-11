import json
import random
import math
import os

COMPATIBLE_BIOMES = [
    [0, 1, 2, 3, 0, 0, 0, 0],
    [1, 0, 11, 10, 1, 1, 1, 1],
    [2, 0, 6, 4, 2, 2, 2, 2],
    [3, 0, 9, 7, 3, 3, 3, 3],
    [4, 5, 6, 4, 4, 4, 4, 4],
    [5, 4, 11, 10, 5, 5, 5, 5],
    [6, 4, 2, 0, 6, 6, 6, 6],
    [7, 8, 9, 7, 7, 7, 7, 7],
    [8, 7, 12, 10, 8, 8, 8, 8],
    [9, 7, 3, 0, 9, 9, 9, 9],
    [10, 11, 12, 1, 5, 8, 10, 10],
    [11, 10, 1, 5, 11, 11, 11, 11],
    [12, 10, 8, 12, 12, 12, 12, 12]
]

BIOME_NAMES = [
    "Bosque Puro (B1)", "Costa Bosque (B2)", "Sabana (B3)", "Taiga (B4)",
    "Desierto Puro (D1)", "Costa Desierto (D2)", "Valle Seco (D3)",
    "Nieve Pura (N1)", "Costa Nieve (N2)", "Bosque Nevado (N3)",
    "Aguas Profundas (A1)", "Aguas Claras (A2)", "Aguas Heladas (A3)"
]

BIOME_COLORS = [
    "#1b5e20", "#00796b", "#81c784", "#4db6ac",
    "#fbc02d", "#ffb300", "#ffe082",
    "#ffffff", "#cfd8dc", "#90a4ae",
    "#0d47a1", "#0288d1", "#00acc1"
]

MONSTER_PROFILES = [
    ["Slime", "Goblin", "Lobo"],
    ["Slime", "Goblin", "Lobo"],
    ["Slime", "Orc", "Lobo"],
    ["Slime", "Orc", "Lobo"],
    ["Escorpion", "Buitre", "Dragon Arena"],
    ["Escorpion", "Buitre", "Dragon Arena"],
    ["Escorpion", "Orc", "Dragon Arena"],
    ["Lobo Invernal", "Golem Hielo", "Yeti"],
    ["Lobo Invernal", "Golem Hielo", "Yeti"],
    ["Lobo Invernal", "Orc", "Yeti"],
    ["Tiburón", "Sirena", "Kraken"],
    ["Slime", "Sirena", "Kraken"],
    ["Lobo Invernal", "Sirena", "Kraken"]
]

BIOME_CONFIGS = [
    {"threshold": 45, "reduction_m1": 0.6, "reduction_m2": 0.7},
    {"threshold": 70, "reduction_m1": 0.8, "reduction_m2": 0.8},
    {"threshold": 70, "reduction_m1": 0.8, "reduction_m2": 0.8},
    {"threshold": 70, "reduction_m1": 0.8, "reduction_m2": 0.8},
    {"threshold": 40, "reduction_m1": 0.5, "reduction_m2": 0.6},
    {"threshold": 70, "reduction_m1": 0.8, "reduction_m2": 0.8},
    {"threshold": 70, "reduction_m1": 0.8, "reduction_m2": 0.8},
    {"threshold": 35, "reduction_m1": 0.4, "reduction_m2": 0.5},
    {"threshold": 70, "reduction_m1": 0.8, "reduction_m2": 0.8},
    {"threshold": 70, "reduction_m1": 0.8, "reduction_m2": 0.8},
    {"threshold": 50, "reduction_m1": 0.7, "reduction_m2": 0.7},
    {"threshold": 70, "reduction_m1": 0.8, "reduction_m2": 0.8},
    {"threshold": 70, "reduction_m1": 0.8, "reduction_m2": 0.8}
]

class MapSimulation:
    def __init__(self, size=40):
        self.size = size
        self.grid = {}
        self.center = size // 2

    def get_tile(self, x, y):
        return self.grid.get((x, y))

    def is_compatible_with_all(self, biome, neighbors):
        for nb in neighbors:
            if biome not in COMPATIBLE_BIOMES[nb]:
                return False
        return True

    def is_costa(self, biome):
        return biome in [1, 5, 8]

    def is_agua(self, biome):
        return 10 <= biome <= 12

    def materialize_tile(self, x, y):
        neighbor_tiles = []
        for dx in [-1, 0, 1]:
            for dy in [-1, 0, 1]:
                if dx == 0 and dy == 0:
                    continue
                nb = self.get_tile(x + dx, y + dy)
                if nb is not None:
                    neighbor_tiles.append(nb)

        if not neighbor_tiles:
            biome = random.randint(0, 12)
            total_target = random.randint(10, 80)
            d1 = random.randint(0, total_target)
            d2 = random.randint(0, total_target - d1)
            d3 = total_target - d1 - d2
            self.grid[(x, y)] = {"biome": biome, "densities": [d1, d2, d3]}
            return

        neighbor_biomes = [n["biome"] for n in neighbor_tiles]
        
        has_agua_neighbor = any(self.is_agua(b) for b in neighbor_biomes)

        compatible_pool = []
        for b in range(13):
            if self.is_costa(b) and not has_agua_neighbor:
                continue
            if self.is_compatible_with_all(b, neighbor_biomes):
                compatible_pool.append(b)

        if compatible_pool:
            if random.random() < 0.7:
                chosen_nb = random.choice(neighbor_biomes)
                if chosen_nb in compatible_pool:
                    chosen_biome = chosen_nb
                else:
                    chosen_biome = random.choice(compatible_pool)
            else:
                chosen_biome = random.choice(compatible_pool)
        else:
            chosen_biome = neighbor_biomes[0]

        avg_m1 = sum(n["densities"][0] for n in neighbor_tiles) / len(neighbor_tiles)
        avg_m2 = sum(n["densities"][1] for n in neighbor_tiles) / len(neighbor_tiles)
        avg_m3 = sum(n["densities"][2] for n in neighbor_tiles) / len(neighbor_tiles)

        m1, m2, m3 = avg_m1, avg_m2, avg_m3
        low_sum = m1 + m2
        cfg = BIOME_CONFIGS[chosen_biome]
        if low_sum > cfg["threshold"]:
            old_m1, old_m2 = m1, m2
            m1 = old_m1 * cfg["reduction_m1"]
            m2 = old_m2 * cfg["reduction_m2"]
            m3 = m3 + ((old_m1 - m1) + (old_m2 - m2))

        delta_m1 = random.randint(-30, 30)
        delta_m2 = random.randint(-30, 30)
        delta_m3 = random.randint(-30, 30)

        f_m1 = max(0, min(100, m1 + delta_m1))
        f_m2 = max(0, min(100, m2 + delta_m2))
        f_m3 = max(0, min(100, m3 + delta_m3))

        total = f_m1 + f_m2 + f_m3
        if total > 100:
            d1 = int((f_m1 * 100) / total)
            d2 = int((f_m2 * 100) / total)
            d3 = 100 - d1 - d2
            densities = [d1, d2, d3]
        else:
            densities = [int(f_m1), int(f_m2), int(f_m3)]

        self.grid[(x, y)] = {"biome": chosen_biome, "densities": densities}

    def generate(self):
        queue = [(self.center, self.center)]
        visited = set(queue)

        while queue:
            x, y = queue.pop(0)
            self.materialize_tile(x, y)

            for dx, dy in [(-1,0), (1,0), (0,-1), (0,1), (-1,-1), (-1,1), (1,-1), (1,1)]:
                nx, ny = x + dx, y + dy
                if 0 <= nx < self.size and 0 <= ny < self.size:
                    if (nx, ny) not in visited:
                        visited.add((nx, ny))
                        queue.append((nx, ny))

    def export_html(self, filepath):
        tiles_js = []
        for (x, y), tile in self.grid.items():
            b = tile["biome"]
            m_names = MONSTER_PROFILES[b]
            d = tile["densities"]
            total = sum(d)
            safe = 100 - total
            tiles_js.append({
                "x": x, "y": y,
                "biome": b,
                "name": BIOME_NAMES[b],
                "color": BIOME_COLORS[b],
                "densities": d,
                "monsters": f"{m_names[0]}: {d[0]}%, {m_names[1]}: {d[1]}%, {m_names[2]}: {d[2]}%, Vacío/Seguro: {safe}%"
            })

        html_content = f"""<!DOCTYPE html>
<html>
<head>
    <title>Visualizador del Mapa Procedural - MistolitoRPG</title>
    <style>
        body {{
            margin: 0;
            background-color: #121212;
            color: #e0e0e0;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            min-height: 100vh;
        }}
        h1 {{
            margin: 20px 0 10px 0;
            font-size: 24px;
            color: #ffffff;
        }}
        #controls {{
            margin-bottom: 15px;
            display: flex;
            gap: 15px;
            align-items: center;
        }}
        #viewMode {{
            background-color: #1e1e1e;
            color: #fff;
            border: 1px solid #444;
            padding: 6px 12px;
            border-radius: 4px;
            cursor: pointer;
        }}
        #container {{
            position: relative;
            background-color: #1e1e1e;
            border-radius: 8px;
            padding: 10px;
            box-shadow: 0 4px 20px rgba(0,0,0,0.5);
        }}
        canvas {{
            border: 1px solid #333;
            cursor: crosshair;
        }}
        #tooltip {{
            position: absolute;
            background-color: rgba(0,0,0,0.85);
            color: #fff;
            padding: 10px;
            border-radius: 4px;
            font-size: 12px;
            pointer-events: none;
            display: none;
            border: 1px solid #444;
            box-shadow: 0 2px 10px rgba(0,0,0,0.5);
        }}
        #legend {{
            display: grid;
            grid-template-columns: repeat(4, 1fr);
            gap: 10px;
            margin-top: 15px;
            background-color: #1e1e1e;
            padding: 15px;
            border-radius: 8px;
            font-size: 11px;
            width: 760px;
        }}
        .legend-item {{
            display: flex;
            align-items: center;
            gap: 5px;
        }}
        .color-box {{
            width: 14px;
            height: 14px;
            border-radius: 2px;
            border: 1px solid #333;
        }}
    </style>
</head>
<body>
    <h1>Visualizador del Mapa Procedural</h1>
    <div id="controls">
        <label for="viewMode">Modo de Vista:</label>
        <select id="viewMode" onchange="draw()">
            <option value="biomes">Biomas del Terreno</option>
            <option value="heatmap">Mapa de Calor (Peligro/Densidad)</option>
        </select>
    </div>
    <div id="container">
        <canvas id="mapCanvas" width="800" height="800"></canvas>
        <div id="tooltip"></div>
    </div>
    <div id="legend"></div>

    <script>
        const tiles = {json.dumps(tiles_js)};
        const size = {self.size};
        const tileSize = 20;

        const canvas = document.getElementById("mapCanvas");
        const ctx = canvas.getContext("2d");
        const tooltip = document.getElementById("tooltip");
        const legend = document.getElementById("legend");

        const biomeNames = {json.dumps(BIOME_NAMES)};
        const biomeColors = {json.dumps(BIOME_COLORS)};

        for (let i = 0; i < 13; i++) {{
            const div = document.createElement("div");
            div.className = "legend-item";
            div.innerHTML = `<div class="color-box" style="background-color: ${{biomeColors[i]}}"></div><span>${{biomeNames[i]}}</span>`;
            legend.appendChild(div);
        }}

        function draw() {{
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            const viewMode = document.getElementById("viewMode").value;
            tiles.forEach(t => {{
                if (viewMode === "biomes") {{
                    ctx.fillStyle = t.color;
                }} else {{
                    const d = t.densities;
                    const total = d[0] + d[1] + d[2];
                    if (total === 0) {{
                        ctx.fillStyle = "#2e7d32";
                    }} else {{
                        const risk = d[0] * 0.1 + d[1] * 0.4 + d[2] * 1.0;
                        const hue = 120 - (risk * 1.2);
                        const lightness = 30 + (total * 0.2);
                        ctx.fillStyle = `hsl(${{hue}}, 85%, ${{lightness}}%)`;
                    }}
                }}
                ctx.fillRect(t.x * tileSize, t.y * tileSize, tileSize - 1, tileSize - 1);
            }});
        }}

        canvas.addEventListener("mousemove", (e) => {{
            const rect = canvas.getBoundingClientRect();
            const mouseX = e.clientX - rect.left;
            const mouseY = e.clientY - rect.top;

            const tx = Math.floor(mouseX / tileSize);
            const ty = Math.floor(mouseY / tileSize);

            const tile = tiles.find(t => t.x === tx && t.y === ty);
            if (tile) {{
                tooltip.style.left = (e.clientX - rect.left + 15) + "px";
                tooltip.style.top = (e.clientY - rect.top + 15) + "px";
                tooltip.style.display = "block";
                tooltip.innerHTML = `
                    <strong>Coord:</strong> (${{tile.x - 20}}, ${{20 - tile.y}})<br>
                    <strong>Bioma:</strong> ${{tile.name}}<br>
                    <strong>Población:</strong><br>
                    ${{tile.monsters.split(', ').join('<br>')}}
                `;
            }} else {{
                tooltip.style.display = "none";
            }}
        }});

        canvas.addEventListener("mouseleave", () => {{
            tooltip.style.display = "none";
        }});

        draw();
    </script>
</body>
</html>
"""
        with open(filepath, "w", encoding="utf-8") as f:
            f.write(html_content)

def main():
    sim = MapSimulation(size=40)
    sim.generate()
    sim.export_html("map_visualizer.html")
    print("Simulación completada. Abrí 'map_visualizer.html' en tu navegador para ver el resultado.")

if __name__ == "__main__":
    main()
