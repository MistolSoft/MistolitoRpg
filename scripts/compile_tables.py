#!/usr/bin/env python3
import json
import struct
import os
import sys

def get_stat_req(stat_req_obj):
    if not stat_req_obj:
        return (0, 0, 0, 0, 0, 0)
    return (
        stat_req_obj.get("str", 0),
        stat_req_obj.get("dex", 0),
        stat_req_obj.get("con", 0),
        stat_req_obj.get("int", 0),
        stat_req_obj.get("wis", 0),
        stat_req_obj.get("cha", 0)
    )

def compile_config(json_dir, out_dir):
    config_path = os.path.join(json_dir, "tables", "config.json")
    if not os.path.exists(config_path):
        config_path = os.path.join(json_dir, "config.json")
    
    with open(config_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    
    cfg = data.get("config", data)
    
    packed = struct.pack(
        "<IIfIIIIIIIIIII",
        cfg.get("exp_base", 50),
        cfg.get("exp_linear", 50),
        float(cfg.get("exp_multiplier", 1.15)),
        cfg.get("hp_bonus_base", 5),
        cfg.get("hp_bonus_step", 5),
        cfg.get("cycle_length", 20),
        cfg.get("dp_gain_chance", 30),
        cfg.get("profession_unlock_dp", 10),
        cfg.get("rest_base_ticks", 5),
        cfg.get("rest_dice_sides", 5),
        cfg.get("rest_recovery_percent", 10),
        cfg.get("max_stats_per_level", 2),
        cfg.get("max_skills_per_level", 2),
        cfg.get("base_stat_dc", 10)
    )
    
    with open(os.path.join(out_dir, "config.bin"), "wb") as f_out:
        f_out.write(packed)

def compile_professions(json_dir, out_dir):
    prof_path = os.path.join(json_dir, "tables", "professions.json")
    if not os.path.exists(prof_path):
        prof_path = os.path.join(json_dir, "game_tables.json")
        
    with open(prof_path, "r", encoding="utf-8") as f:
        data = json.load(f)
        
    professions = data.get("professions", [])
    
    with open(os.path.join(out_dir, "professions.bin"), "wb") as f_out:
        for prof in professions:
            name_bytes = prof["name"].encode("utf-8")[:15]
            name_padded = name_bytes + b"\x00" * (16 - len(name_bytes))
            
            req = prof.get("req", {})
            bonus = prof.get("bonus_stats", [])
            
            packed = struct.pack(
                "<B16sBBBBBBBBBBBBBBBBBBBBBBBB",
                prof["id"],
                name_padded,
                req.get("str", 0),
                req.get("con", 0),
                req.get("dex", 0),
                req.get("int", 0),
                req.get("wis", 0),
                req.get("cha", 0),
                1 if "str" in bonus else 0,
                1 if "con" in bonus else 0,
                1 if "dex" in bonus else 0,
                1 if "int" in bonus else 0,
                1 if "wis" in bonus else 0,
                1 if "cha" in bonus else 0,
                prof.get("success_dc", 10 if prof["id"] > 0 else 0),
                prof.get("dp_cost", 10 if prof["id"] > 0 else 0),
                prof.get("hp_rest_threshold", 60),
                prof.get("recovery_chance", 75),
                prof["base_hp"],
                prof["base_energy"],
                prof["base_ac"],
                prof["damage_dice"],
                prof["damage_bonus"],
                prof["dice_count"],
                prof.get("hit_dice", 8),
                prof.get("hp_per_level", 5)
            )
            f_out.write(packed)

def compile_enemies(json_dir, out_dir):
    enemy_path = os.path.join(json_dir, "tables", "enemies.json")
    with open(enemy_path, "r", encoding="utf-8") as f:
        data = json.load(f)
        
    enemies = data.get("enemies", [])
    tiers = data.get("enemy_tiers", [])
    
    with open(os.path.join(out_dir, "enemies.bin"), "wb") as f_out:
        for e in enemies:
            name_bytes = e["name"].encode("utf-8")[:15]
            name_padded = name_bytes + b"\x00" * (16 - len(name_bytes))
            
            packed = struct.pack(
                "<B16sBBBBBBBBBHHB",
                e["id"],
                name_padded,
                e["tier"],
                e["base_hp"],
                e["hp_per_level"],
                e["base_ac"],
                e["ac_per_level"],
                e["damage_dice"],
                e["damage_bonus"],
                e["damage_per_level"],
                e["attack_bonus"],
                e["exp_base"],
                e["exp_per_level"],
                e.get("detect_dc", 10)
            )
            f_out.write(packed)
            
    with open(os.path.join(out_dir, "enemy_tiers.bin"), "wb") as f_out:
        for t in tiers:
            packed = struct.pack(
                "<BBB",
                t["tier"],
                t["min_level"],
                t["max_level"]
            )
            f_out.write(packed)

def compile_transition_intervals(json_dir, out_dir):
    tables_path = os.path.join(json_dir, "game_tables.json")
    with open(tables_path, "r", encoding="utf-8") as f:
        data = json.load(f)
        
    intervals = data.get("level_tables", {}).get("transition_intervals", [])
    
    with open(os.path.join(out_dir, "transition_intervals.bin"), "wb") as f_out:
        for interval in intervals:
            packed = struct.pack(
                "<II",
                interval["max_level"],
                interval["transitions_per_level"]
            )
            f_out.write(packed)

def compile_skills_perks(json_dir, out_dir):
    tables_path = os.path.join(json_dir, "game_tables.json")
    with open(tables_path, "r", encoding="utf-8") as f:
        data = json.load(f)
        
    skills = data.get("skills", [])
    perks = data.get("perks", [])
    
    intent_map = {"attack": 0, "defend": 1, "heal": 2, "magic": 3, "support": 4, "flee": 5}
    
    with open(os.path.join(out_dir, "skills.bin"), "wb") as f_out:
        for s in skills:
            name_bytes = s["name"].encode("utf-8")[:31]
            name_padded = name_bytes + b"\x00" * (32 - len(name_bytes))
            
            mask = sum(1 << p for p in s.get("profession_req", []))
            str_req, dex_req, con_req, int_req, wis_req, cha_req = get_stat_req(s.get("stat_req", {}))
            
            intent_str = s.get("intent_type", "attack")
            intent_val = intent_map.get(intent_str, 0)
            
            packed = struct.pack(
                "<B32sBBBBBBBBBBB",
                s["id"],
                name_padded,
                s["profession_level_req"],
                mask,
                str_req,
                dex_req,
                con_req,
                int_req,
                wis_req,
                cha_req,
                s["dp_cost"],
                s["success_dc"],
                intent_val
            )
            f_out.write(packed)
            
    with open(os.path.join(out_dir, "perks.bin"), "wb") as f_out:
        for p in perks:
            name_bytes = p["name"].encode("utf-8")[:31]
            name_padded = name_bytes + b"\x00" * (32 - len(name_bytes))
            
            mask = sum(1 << pr for pr in p.get("profession_req", []))
            str_req, dex_req, con_req, int_req, wis_req, cha_req = get_stat_req(p.get("stat_req", {}))
            
            packed = struct.pack(
                "<B32sBBBBBBBBBB",
                p["id"],
                name_padded,
                p["profession_level_req"],
                mask,
                str_req,
                dex_req,
                con_req,
                int_req,
                wis_req,
                cha_req,
                p["dp_cost"],
                p["success_dc"]
            )
            f_out.write(packed)

def compile_spells(json_dir, out_dir):
    spell_files = [
        ("spells_cantrips.json", 0),
        ("spells_level_1.json", 1),
        ("spells_level_2.json", 2),
        ("spells_level_3.json", 3)
    ]
    
    with open(os.path.join(out_dir, "spells.bin"), "wb") as f_out:
        for filename, lvl in spell_files:
            filepath = os.path.join(json_dir, "tables", filename)
            if not os.path.exists(filepath):
                continue
                
            with open(filepath, "r", encoding="utf-8") as f:
                data = json.load(f)
                
            spells = data.get("spells", [])
            for s in spells:
                id_bytes = s["id"].encode("utf-8")[:23]
                id_padded = id_bytes + b"\x00" * (24 - len(id_bytes))
                
                name_bytes = s["name"].encode("utf-8")[:31]
                name_padded = name_bytes + b"\x00" * (32 - len(name_bytes))
                
                packed = struct.pack(
                    "<24s32sBBB",
                    id_padded,
                    name_padded,
                    lvl,
                    s["dp_cost"],
                    s["success_dc"]
                )
                f_out.write(packed)

def compile_features(json_dir, out_dir):
    features_path = os.path.join(json_dir, "tables", "features_by_level.json")
    with open(features_path, "r", encoding="utf-8") as f:
        data = json.load(f)
        
    features_by_level = data.get("features_by_level", {})
    prof_map = {"warrior": 1, "mage": 2, "rogue": 3}
    
    with open(os.path.join(out_dir, "features.bin"), "wb") as f_out:
        for prof_name, levels in features_by_level.items():
            prof_id = prof_map.get(prof_name, 0)
            if prof_id == 0:
                continue
                
            for lvl_str, lvl_data in levels.items():
                lvl = int(lvl_str)
                
                features_av = lvl_data.get("features_available", [])
                for f_av in features_av:
                    name_bytes = f_av["name"].encode("utf-8")[:31]
                    name_padded = name_bytes + b"\x00" * (32 - len(name_bytes))
                    
                    packed = struct.pack(
                        "<BB32sBB16s",
                        prof_id,
                        lvl,
                        name_padded,
                        f_av["dp_cost"],
                        f_av["success_dc"],
                        b"\x00" * 16
                    )
                    f_out.write(packed)
                    
                features_arch = lvl_data.get("features_available_by_archetype", {})
                for arch_name, arch_list in features_arch.items():
                    arch_bytes = arch_name.encode("utf-8")[:15]
                    arch_padded = arch_bytes + b"\x00" * (16 - len(arch_bytes))
                    
                    for f_ar in arch_list:
                        name_bytes = f_ar["name"].encode("utf-8")[:31]
                        name_padded = name_bytes + b"\x00" * (32 - len(name_bytes))
                        
                        packed = struct.pack(
                            "<BB32sBB16s",
                            prof_id,
                            lvl,
                            name_padded,
                            f_ar["dp_cost"],
                            f_ar["success_dc"],
                            arch_padded
                        )
                        f_out.write(packed)

def compile_resources(json_dir, out_dir):
    res_path = os.path.join(json_dir, "tables", "resources.json")
    with open(res_path, "r", encoding="utf-8") as f:
        data = json.load(f)
        
    resources = data.get("resources", {})
    
    warrior_res = resources.get("warrior", {})
    mage_res = resources.get("mage", {})
    rogue_res = resources.get("rogue", {})
    
    with open(os.path.join(out_dir, "resources.bin"), "wb") as f_out:
        for lvl in range(1, 21):
            as_max = 1
            as_cbl = warrior_res.get("action_surge", {}).get("count_by_level", {})
            for req_lvl_str, cnt in as_cbl.items():
                if lvl >= int(req_lvl_str):
                    as_max = cnt
            
            ind_max = 0
            ind_cbl = warrior_res.get("indomitable", {}).get("count_by_level", {})
            for req_lvl_str, cnt in ind_cbl.items():
                if lvl >= int(req_lvl_str):
                    ind_max = cnt
                    
            sup_max = 0
            sup_cbl = warrior_res.get("superiority_dice", {}).get("count_by_level", {})
            for req_lvl_str, cnt in sup_cbl.items():
                if lvl >= int(req_lvl_str):
                    sup_max = cnt
                    
            sup_size = 8
            sup_dbl = warrior_res.get("superiority_dice", {}).get("dice_sides_by_level", {})
            for req_lvl_str, sides in sup_dbl.items():
                if lvl >= int(req_lvl_str):
                    sup_size = sides
                    
            slots = [0] * 9
            mage_slots = mage_res.get("spell_slots", {}).get("slots_by_level", {}).get(str(lvl), {})
            for i in range(1, 10):
                slots[i - 1] = mage_slots.get(str(i), 0)
                
            sneak_dice = 0
            rogue_sa = rogue_res.get("sneak_attack", {}).get("dice_count_by_level", {})
            for req_lvl_str, cnt in rogue_sa.items():
                if lvl >= int(req_lvl_str):
                    sneak_dice = cnt
                    
            packed = struct.pack(
                "<BBBBBBBBBBBBBBB",
                lvl,
                as_max,
                ind_max,
                sup_max,
                sup_size,
                *slots,
                sneak_dice
            )
            f_out.write(packed)

def compile_damage_progression(json_dir, out_dir):
    tables_path = os.path.join(json_dir, "game_tables.json")
    with open(tables_path, "r", encoding="utf-8") as f:
        data = json.load(f)
        
    level_tables = data.get("level_tables", {})
    prof_map = {"novice": 0, "warrior": 1, "mage": 2, "rogue": 3}
    
    with open(os.path.join(out_dir, "damage_progression.bin"), "wb") as f_out:
        for prof_name, table in level_tables.items():
            prof_id = prof_map.get(prof_name, -1)
            if prof_id == -1:
                continue
            
            prog = table.get("damage_progression", [])
            for entry in prog:
                packed = struct.pack(
                    "<BBBBBBBB",
                    prof_id,
                    entry["level"],
                    entry.get("min_damage", 0),
                    entry.get("max_damage", 0),
                    entry.get("extra_dice", 0),
                    entry.get("crit", 0),
                    entry.get("sneak_dice", 0),
                    entry.get("skill_uses", 0)
                )
                f_out.write(packed)

def compile_dna(json_dir, out_dir):
    dna_path = os.path.join(json_dir, "dna", "pet_dna.json")
    if not os.path.exists(dna_path):
        print(f"Warning: pet_dna.json not found at {dna_path}")
        return
    with open(dna_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    stat_keys = ["str", "dex", "con", "int", "wis", "cha"]
    packed = b""
    for key in stat_keys:
        code = data.get(key, "")[:6]
        code_bytes = code.encode("utf-8")
        code_padded = code_bytes + b"\x00" * (7 - len(code_bytes))
        packed += code_padded
    with open(os.path.join(out_dir, "pet_dna.bin"), "wb") as f_out:
        f_out.write(packed)

def compile_zones(json_dir, out_dir):
    enemy_path = os.path.join(json_dir, "tables", "enemies.json")
    with open(enemy_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    zones = data.get("world_zones", [])
    zone_enemies = data.get("zone_enemies", [])
    
    with open(os.path.join(out_dir, "world_zones.bin"), "wb") as f_out:
        for z in zones:
            name_bytes = z["name"].encode("utf-8")[:15]
            name_padded = name_bytes + b"\x00" * (16 - len(name_bytes))
            packed = struct.pack(
                "<B16shhhh",
                z["id"],
                name_padded,
                z["min_x"],
                z["max_x"],
                z["min_y"],
                z["max_y"]
            )
            f_out.write(packed)
            
    with open(os.path.join(out_dir, "zone_enemies.bin"), "wb") as f_out:
        for ze in zone_enemies:
            packed = struct.pack(
                "<BB",
                ze["zone_id"],
                ze["enemy_id"]
            )
            f_out.write(packed)

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    
    json_dir = os.path.join(project_root, "firmware", "data")
    out_dir = os.path.join(json_dir, "binary")
    os.makedirs(out_dir, exist_ok=True)
    
    print("Compiling game tables to binary...")
    compile_config(json_dir, out_dir)
    compile_professions(json_dir, out_dir)
    compile_enemies(json_dir, out_dir)
    compile_transition_intervals(json_dir, out_dir)
    compile_skills_perks(json_dir, out_dir)
    compile_spells(json_dir, out_dir)
    compile_features(json_dir, out_dir)
    compile_resources(json_dir, out_dir)
    compile_damage_progression(json_dir, out_dir)
    compile_dna(json_dir, out_dir)
    compile_zones(json_dir, out_dir)
    print("Compilation completed successfully! Binaries written to:", out_dir)

if __name__ == "__main__":
    main()
