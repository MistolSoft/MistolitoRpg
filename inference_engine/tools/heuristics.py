def calc_hit_prob(target_ac, attack_bonus):
    roll_needed = target_ac - attack_bonus
    if roll_needed < 2:
        roll_needed = 2
    if roll_needed > 20:
        roll_needed = 20
    return (21.0 - float(roll_needed)) / 20.0

def calc_defense_prob(pet_ac, enemy_attack):
    roll_needed = pet_ac - enemy_attack
    if roll_needed < 2:
        roll_needed = 2
    if roll_needed > 20:
        roll_needed = 20
    return (21.0 - float(roll_needed)) / 20.0

def calc_avg_pet_damage(dice_count, dice_size, str_mod, min_damage):
    avg_roll = (dice_size + 1) / 2.0
    return max(1.0, dice_count * avg_roll + str_mod + min_damage)

def calc_avg_enemy_damage(dice_size, damage_bonus):
    avg_roll = (dice_size + 1) / 2.0
    return max(1.0, avg_roll + damage_bonus)

def decide_action_balanced(hit_prob, defense_prob, pet_hp_ratio, enemy_hp_ratio, threat_level, dmg_efficiency, quality_score, last_action):
    if pet_hp_ratio < 0.15:
        return 2
    if pet_hp_ratio < 0.3 and enemy_hp_ratio > 2.0:
        return 2

    if quality_score < -0.3:
        if last_action == 0:
            if threat_level > 0.5:
                return 1
            elif pet_hp_ratio < 0.3:
                return 2
        elif last_action == 1:
            if hit_prob > 0.5:
                return 0
        elif last_action == 2:
            if hit_prob > 0.4:
                return 0

    if quality_score > 0.3:
        if last_action == 0:
            return 0
        elif last_action == 1:
            if hit_prob > 0.6:
                return 0

    if hit_prob > 0.65 and threat_level < 0.4 and dmg_efficiency > 0.3:
        return 0
    if hit_prob > 0.5 and pet_hp_ratio > 0.6 and enemy_hp_ratio < 1.0:
        return 0
    if hit_prob > 0.45 and pet_hp_ratio > 0.5 and threat_level < 0.5:
        return 0

    if threat_level > 0.6 and pet_hp_ratio < 0.4:
        return 1
    if enemy_hp_ratio > 1.8 and pet_hp_ratio < 0.5:
        return 1
    if defense_prob > 0.5 and pet_hp_ratio < 0.6:
        return 1

    return 0

def decide_action_aggressive(hit_prob, defense_prob, pet_hp_ratio, enemy_hp_ratio, threat_level, dmg_efficiency, quality_score, last_action):
    # Rara vez huye
    if pet_hp_ratio < 0.05:
        return 2
    
    # Solo defiende si el HP es crítico y el enemigo puede matarnos
    if pet_hp_ratio < 0.2 and threat_level > 0.8:
        return 1
        
    # Por defecto, ataca siempre a menos que la probabilidad de acertar sea casi nula y el daño bajo
    if hit_prob < 0.2 and dmg_efficiency < 0.1 and defense_prob > 0.6:
        return 1
        
    return 0

def decide_action_cautious(hit_prob, defense_prob, pet_hp_ratio, enemy_hp_ratio, threat_level, dmg_efficiency, quality_score, last_action):
    # Huye fácilmente
    if pet_hp_ratio < 0.25:
        return 2
    if pet_hp_ratio < 0.4 and threat_level > 0.7:
        return 2
        
    # Defiende frecuentemente si hay amenaza
    if threat_level > 0.4 or pet_hp_ratio < 0.6:
        return 1
        
    # Solo ataca si es seguro
    if hit_prob > 0.6 and threat_level < 0.3:
        return 0
        
    return 1

def decide_action(hit_prob, defense_prob, pet_hp_ratio, enemy_hp_ratio, threat_level, dmg_efficiency, quality_score, last_action, profile="balanced"):
    if profile == "aggressive":
        return decide_action_aggressive(hit_prob, defense_prob, pet_hp_ratio, enemy_hp_ratio, threat_level, dmg_efficiency, quality_score, last_action)
    elif profile == "cautious":
        return decide_action_cautious(hit_prob, defense_prob, pet_hp_ratio, enemy_hp_ratio, threat_level, dmg_efficiency, quality_score, last_action)
    else:
        return decide_action_balanced(hit_prob, defense_prob, pet_hp_ratio, enemy_hp_ratio, threat_level, dmg_efficiency, quality_score, last_action)

