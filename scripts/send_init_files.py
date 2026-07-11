#!/usr/bin/env python3
"""
MistolitoRPG USB Init Tool

This script sends initialization files to MistolitoRPG device via USB Serial.

Usage:
    python send_init_files.py --port COM3
    python send_init_files.py --port COM3 --wipe
"""

import serial
import base64
import os
import sys
import time
import argparse

CHUNK_SIZE = 128
BAUDRATE = 115200
TIMEOUT = 5


class MistolitoUSBInit:
    def __init__(self, port, baudrate=BAUDRATE):
        self.port = port
        self.baudrate = baudrate
        self.ser = None

    def connect(self):
        print(f"Connecting to {self.port}...")
        self.ser = serial.Serial(self.port, self.baudrate, timeout=TIMEOUT)
        time.sleep(2)

        self.ser.write(b'\n')
        time.sleep(0.2)

        self.ser.reset_input_buffer()
        self.ser.reset_output_buffer()
        time.sleep(0.5)

        while self.ser.in_waiting > 0:
            self.ser.readline()

        print("Connected!")
        return True

    def disconnect(self):
        if self.ser:
            self.ser.close()
            print("Disconnected.")

    def send_command(self, cmd, wait_for_response=True):
        full_cmd = f"CMD:{cmd}\n"
        self.ser.write(full_cmd.encode('utf-8'))
        self.ser.flush()

        time.sleep(0.5)

        if not wait_for_response:
            return ""

        for _ in range(30):
            if self.ser.in_waiting > 0:
                response = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if response:
                    if response.startswith("FILE_") or response.startswith("ERROR:") or response.startswith("STATUS:") or response.startswith("INIT_") or response.startswith("WIPE_") or response.startswith("START_"):
                        return response
            else:
                time.sleep(0.1)

        return ""

    def send_file(self, filepath, remote_path=None):
        filename = os.path.basename(filepath)
        filepath = os.path.abspath(filepath)

        if not os.path.exists(filepath):
            print(f"Error: File not found: {filepath}")
            return False

        filesize = os.path.getsize(filepath)

        if remote_path:
            remote_file_path = remote_path
        else:
            remote_file_path = f"/DATA/{filename}"

        print(f"Sending {filename} ({filesize} bytes)...")

        response = self.send_command(f"FILE_START:{remote_file_path}:{filesize}")
        print(f" Response: {response}")

        if not response.startswith("FILE_RECV_START"):
            print("FAILED")
            return False

        with open(filepath, 'rb') as f:
            sent = 0
            while True:
                chunk = f.read(CHUNK_SIZE)
                if not chunk:
                    break

                encoded = base64.b64encode(chunk).decode('utf-8')
                sent += len(chunk)

                pct = int((sent / filesize) * 100)
                print(f"\r {pct}%", end="", flush=True)

                response = self.send_command(f"FILE_DATA:{encoded}")

                if not response:
                    print(f" FAILED: timeout (no response from device)")
                    return False
                if "ERROR" in response:
                    print(f" FAILED: {response}")
                    return False

        print()

        response = self.send_command("FILE_END")

        if "FILE_RECV_OK" in response:
            print(" OK")
            return True
        else:
            print(" FAILED")
            return False

    def check_status(self):
        response = self.send_command("STATUS")
        print(f"Status: {response}")
        return response

    def delete_file(self, remote_path):
        print(f"Deleting {remote_path}...", end=" ", flush=True)
        response = self.send_command(f"DELETE:{remote_path}")
        
        if "DELETE_OK" in response or "FILE_NOT_FOUND" in response:
            print("OK")
            return True
        else:
            print(f"Response: {response}")
            return False

    def complete_init(self):
        print("Completing initialization...", end=" ", flush=True)
        response = self.send_command("INIT_COMPLETE")

        if "INIT_COMPLETE" in response:
            print("OK")
            print("\n" + "="*50)
            print("Initialization complete!")
            print("The device will reboot.")
            print("="*50)
            return True
        else:
            print("FAILED")
            return False

    def wipe(self):
        print("Wiping all data...", end=" ", flush=True)
        response = self.send_command("WIPE")

        if "WIPE_COMPLETE" in response:
            print("OK")
            print("Device wiped. Rebooting...")
            return True
        print("FAILED")
        return False

    def dump_replay(self, export_csv=None):
        print("Requesting replay dump...")
        self.ser.reset_input_buffer()

        full_cmd = "CMD:DUMP_REPLAY\n"
        self.ser.write(full_cmd.encode('utf-8'))
        self.ser.flush()
        time.sleep(0.5)

        files = {}
        current_file = None
        current_data = bytearray()
        total_transitions = 0
        line_count = 0

        while True:
            if self.ser.in_waiting > 0:
                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if not line:
                    continue
                line_count += 1

                if line.startswith('FILE_START:'):
                    parts = line.split(':')
                    if len(parts) >= 3:
                        filepath = parts[1]
                        files[filepath] = {'size': int(parts[2]), 'data': bytearray()}
                        current_file = filepath
                        current_data = files[filepath]['data']
                        print(f"Receiving: {filepath} ({files[filepath]['size']} bytes)")

                elif line.startswith('FILE_DATA:'):
                    if current_file:
                        b64_data = line[10:]
                        if len(b64_data) > 0 and b64_data != '=':
                            padded = b64_data + '=' * (4 - len(b64_data) % 4) if len(b64_data) % 4 else b64_data
                            current_data.extend(base64.b64decode(padded))

                elif line.startswith('FILE_END'):
                    if current_file:
                        total_transitions += len(files[current_file]['data']) // 78
                        print(f"  -> {len(files[current_file]['data'])} bytes ({total_transitions} transitions so far)")
                        current_file = None
                        current_data = bytearray()

                elif line.startswith('DUMP_END:'):
                    print(f"Dump complete: {line}")
                    break

                elif line.startswith('DUMP_ERROR:'):
                    print(f"Error: {line}")
                    break
            else:
                time.sleep(0.1)

        print(f"\nReceived {len(files)} files, {total_transitions} total transitions")

        if export_csv and total_transitions > 0:
            import csv
            with open(export_csv, 'w', newline='') as cf:
                writer = csv.writer(cf)
                writer.writerow(['state_0','state_1','state_2','state_3','state_4','state_5','state_6','state_7','state_8','action','reward','next_0','next_1','next_2','next_3','next_4','next_5','next_6','next_7','next_8','done'])

                action_counts = {}
                for filepath, fileinfo in files.items():
                    if 'chunk_' not in filepath:
                        continue
                    data = fileinfo['data']
                    import struct
                    for i in range(0, len(data), 78):
                        if i + 78 > len(data):
                            break
                        tr = data[i:i+78]
                        state = struct.unpack('<9f', tr[0:36])
                        action = tr[36]
                        reward = struct.unpack('<f', tr[37:41])[0]
                        next_state = struct.unpack('<9f', tr[41:77])
                        done = tr[77]
                        writer.writerow([f'{v:.4f}' for v in state] + [action, f'{reward:.6f}'] + [f'{v:.4f}' for v in next_state] + [done])
                        action_counts[action] = action_counts.get(action, 0) + 1

            print(f"Exported to {export_csv}")

            if action_counts:
                print(f"\nAction distribution:")
                for action in sorted(action_counts.keys()):
                    count = action_counts[action]
                    pct = 100.0 * count / total_transitions
                    print(f"  Action {action}: {count} ({pct:.1f}%)")

        return files, total_transitions


def main():
    parser = argparse.ArgumentParser(description='MistolitoRPG USB Init Tool')
    parser.add_argument('--port', required=True, help='Serial port (e.g., COM3 or /dev/ttyUSB0)')
    parser.add_argument('--tables-dir', default='firmware/data',
                        help='Directory containing game_tables.json')
    parser.add_argument('--dna-dir', default='firmware/data/dna',
                        help='Directory containing DNA JSON file')
    parser.add_argument('--pet-data', default='firmware/data/pet_data.json',
                        help='Path to pet_data.json')
    parser.add_argument('--models-dir', default='inference_engine/models',
                        help='Directory containing model .tflite files')
    parser.add_argument('--baud', type=int, default=BAUDRATE, help='Baud rate')
    parser.add_argument('--wipe', action='store_true', help='Wipe device and reboot')
    parser.add_argument('--no-wipe', action='store_true', help='Skip SD card wipe')
    parser.add_argument('--keep-pet', action='store_true', help='Skip deleting pet_data.json to preserve progress')
    parser.add_argument('--status', action='store_true', help='Check device status only')
    parser.add_argument('--dump-replay', action='store_true', help='Dump replay data from device to CSV')
    parser.add_argument('--export', default=None, help='CSV output path for --dump-replay')

    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)

    init = MistolitoUSBInit(args.port, args.baud)

    try:
        init.connect()

        if args.status:
            init.check_status()
            return

        if args.dump_replay:
            init.dump_replay(args.export)
            return

        if args.wipe:
            init.wipe()
            return

        if not args.no_wipe:
            print("\n--- Wiping SD card ---")
            init.wipe()
            print()

        print("\n--- Compiling tables to binary ---")
        import subprocess
        compile_script = os.path.join(script_dir, "compile_tables.py")
        subprocess.check_call([sys.executable, compile_script])

        binary_dir = os.path.join(project_root, "firmware", "data", "binary")
        binary_files = [
            ("config.bin", "/DATA/TABLES/config.bin"),
            ("professions.bin", "/DATA/TABLES/professions.bin"),
            ("enemies.bin", "/DATA/TABLES/enemies.bin"),
            ("enemy_tiers.bin", "/DATA/TABLES/enemy_tiers.bin"),
            ("transition_intervals.bin", "/DATA/TABLES/transition_intervals.bin"),
            ("skills.bin", "/DATA/TABLES/skills.bin"),
            ("perks.bin", "/DATA/TABLES/perks.bin"),
            ("spells.bin", "/DATA/TABLES/spells.bin"),
            ("features.bin", "/DATA/TABLES/features.bin"),
            ("resources.bin", "/DATA/TABLES/resources.bin"),
            ("damage_progression.bin", "/DATA/TABLES/damage_progression.bin"),
            ("world_zones.bin", "/DATA/TABLES/world_zones.bin"),
            ("zone_enemies.bin", "/DATA/TABLES/zone_enemies.bin"),
        ]
        
        print("\n--- Sending binary tables ---")
        for filename, remote_path in binary_files:
            local_path = os.path.join(binary_dir, filename)
            if os.path.exists(local_path):
                if not init.send_file(local_path, remote_path):
                    print(f"Failed to send {filename}")
                    return
            else:
                print(f"Warning: {filename} not found at {local_path}")

        dna_file = os.path.join(binary_dir, 'pet_dna.bin')
        if os.path.exists(dna_file):
            if not init.send_file(dna_file, "/DATA/DNA/pet_dna.bin"):
                print("Failed to send pet_dna.bin")
                return
        else:
            print("Warning: pet_dna.bin not found")

        models_dir = args.models_dir
        if not os.path.isabs(models_dir):
            models_dir = os.path.join(project_root, models_dir)

        backbone_file = os.path.join(models_dir, 'backbone.espdl')
        if os.path.exists(backbone_file):
            if not init.send_file(backbone_file, "/MODELS/backbone.espdl"):
                print("Failed to send backbone.espdl")
                return
        else:
            print("Warning: backbone.espdl not found in models dir")

        actor_init_file = os.path.join(models_dir, 'actor_init.bin')
        if os.path.exists(actor_init_file):
            if not init.send_file(actor_init_file, "/BRAIN/COMBAT/actor_init.bin"):
                print("Failed to send actor_init.bin")
                return
        else:
            print("Warning: actor_init.bin not found in models dir")

        value_head_file = os.path.join(models_dir, 'value_head.bin')
        if os.path.exists(value_head_file):
            if not init.send_file(value_head_file, "/BRAIN/COMBAT/value_head.bin"):
                print("Failed to send value_head.bin")
                return
        else:
            print("Warning: value_head.bin not found in models dir")

        critic_file = os.path.join(models_dir, 'critic_weights.bin')
        if os.path.exists(critic_file):
            if not init.send_file(critic_file, "/BRAIN/COMBAT/critic.bin"):
                print("Failed to send critic_weights.bin")
                return
        else:
            print("Warning: critic_weights.bin not found in models dir")

        if not args.keep_pet:
            print("Deleting pet_data.bin to force DNA-based initialization...")
            init.delete_file("/BRAIN/PET/pet_data.bin")

        if not init.complete_init():
            return

    except serial.SerialException as e:
        print(f"Serial error: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nCancelled by user")
    finally:
        init.disconnect()


if __name__ == "__main__":
    main()
