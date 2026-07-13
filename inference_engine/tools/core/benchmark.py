import torch
import numpy as np
import torch.nn as nn

class Backbone(nn.Module):
    def __init__(self):
        super().__init__()
        self.fc1 = nn.Linear(6, 32)
        self.relu1 = nn.ReLU()
        self.fc2 = nn.Linear(32, 32)
        self.relu2 = nn.ReLU()
        self.fc3 = nn.Linear(32, 16)
        self.relu3 = nn.ReLU()

    def forward(self, x):
        return self.relu3(self.fc3(self.relu2(self.fc2(self.relu1(self.fc1(x))))))

class PolicyHead(nn.Module):
    def __init__(self, num_actions=4):
        super().__init__()
        self.fc = nn.Linear(16, num_actions)

    def forward(self, x):
        return self.fc(x)

class CoreModelSplit(nn.Module):
    def __init__(self, num_actions=4):
        super().__init__()
        self.backbone = Backbone()
        self.policy = PolicyHead(num_actions)

    def forward(self, x):
        features = self.backbone(x)
        logits = self.policy(features)
        return logits

def get_heuristic_action(hp, energy, exploration, density, difficulty, momentum):
    if hp < 0.45 or energy < 0.1:
        return 2
    elif difficulty < -0.5 and density > 0.3:
        if momentum > 0.0:
            return 1
        elif exploration < 0.8:
            return 0
        else:
            return 1
    elif momentum <= 0.2 and density > 0.1 and difficulty >= -0.5:
        return 3
    elif momentum > 0.2:
        return 1
    elif density <= 0.1 and exploration < 0.8:
        return 0
    else:
        return 1

def run_simulation():
    model = CoreModelSplit()
    try:
        model.load_state_dict(torch.load("../models/core_model.pth"))
    except Exception as e:
        try:
            model.load_state_dict(torch.load("models/core_model.pth"))
        except:
            print(f"Could not load trained model, using random weights: {e}")
    model.eval()

    state = {
        "hp": 1.0,
        "energy": 1.0,
        "exploration": 0.2,
        "density": 0.0,
        "difficulty": 0.0,
        "momentum": -0.2
    }

    action_names = ["SCAN", "MOVE", "REST", "FIGHT"]

    print(f"{'Turn':<5} | {'HP':<5} | {'EN':<5} | {'EXP':<5} | {'DENS':<5} | {'DIFF':<5} | {'MOM':<5} | {'Action':<6} | {'Eval':<15}")
    print("-" * 85)

    correct_decisions = 0
    total_decisions = 0

    for turn in range(1, 41):
        inputs = [
            state["hp"],
            state["energy"],
            state["exploration"],
            state["density"],
            state["difficulty"],
            state["momentum"]
        ]
        
        inp_tensor = torch.tensor([inputs], dtype=torch.float32)
        with torch.no_grad():
            logits = model(inp_tensor)
            probs = torch.softmax(logits, dim=1).numpy()[0]
            action = int(np.argmax(probs))

        expected = get_heuristic_action(
            state["hp"],
            state["energy"],
            state["exploration"],
            state["density"],
            state["difficulty"],
            state["momentum"]
        )

        total_decisions += 1
        if action == expected:
            eval_str = "OK"
            correct_decisions += 1
        else:
            eval_str = f"WRONG (Exp: {action_names[expected]})"

        print(f"{turn:<5} | {state['hp']:5.2f} | {state['energy']:5.2f} | {state['exploration']:5.2f} | {state['density']:5.2f} | {state['difficulty']:5.2f} | {state['momentum']:5.2f} | {action_names[action]:<6} | {eval_str}")

        if action == 2:
            state["hp"] = min(1.0, state["hp"] + 0.25)
            state["energy"] = min(1.0, state["energy"] + 0.3)
        elif action == 0:
            state["energy"] = max(0.0, state["energy"] - 0.05)
            state["exploration"] = min(1.0, state["exploration"] + 0.2)
            state["momentum"] = min(1.0, state["momentum"] + np.random.uniform(0.1, 0.4))
        elif action == 1:
            state["energy"] = max(0.0, state["energy"] - 0.1)
            state["exploration"] = max(0.1, state["exploration"] - 0.3)
            state["density"] = np.random.uniform(0.0, 1.0)
            state["difficulty"] = np.random.uniform(-0.5, 0.5)
            state["momentum"] = np.random.uniform(-0.5, 0.3)
        elif action == 3:
            state["hp"] = max(0.0, state["hp"] - np.random.uniform(0.05, 0.2))
            state["energy"] = max(0.0, state["energy"] - 0.15)
            state["density"] = max(0.0, state["density"] - 0.3)

        if state["hp"] <= 0.0:
            print(f"\nSimulation ended at turn {turn} (Pet died)")
            break

    accuracy = (correct_decisions / total_decisions) * 100
    print(f"\nDecision Accuracy: {accuracy:.1f}% ({correct_decisions}/{total_decisions})\n")

if __name__ == "__main__":
    run_simulation()
