import os
import struct
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

def main():
    model = CoreModelSplit()
    
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(os.path.dirname(os.path.dirname(script_dir)))
    
    checkpoint_path = os.path.join(project_dir, "models", "core_model.pth")
    model.load_state_dict(torch.load(checkpoint_path, map_location="cpu"))
    model.eval()

    output_dir = os.path.join(project_dir, "models", "core")
    os.makedirs(output_dir, exist_ok=True)
    
    onnx_path = os.path.join(output_dir, "backbone.onnx")

    dummy = torch.randn(1, 6)
    torch.onnx.export(
        model.backbone,
        dummy,
        onnx_path,
        input_names=["input"],
        output_names=["output"],
        opset_version=17
    )
    print(f"Backbone ONNX exported to: {onnx_path}")

    W = model.policy.fc.weight.detach().numpy().astype(np.float32)
    b = model.policy.fc.bias.detach().numpy().astype(np.float32)

    bin_path = os.path.join(output_dir, "actor_init.bin")
    magic = 0x50484544
    version = 1
    num_actions = 4
    epoch = 0
    meta = struct.pack("<IIBII", magic, version, num_actions, epoch, 0)
    with open(bin_path, "wb") as f:
        f.write(meta)
        f.write(W.T.flatten().tobytes())
        f.write(b.tobytes())
    print(f"Actor init weights exported to: {bin_path}")

if __name__ == "__main__":
    main()
