#!/usr/bin/env python3
import vlc
import json
import sys
import os
import argparse
import time
import socket
import threading
import signal
import subprocess
from pathlib import Path

class RadioDaemon:
    def __init__(self, socket_path="/tmp/radio.sock"):
        self.socket_path = socket_path
        self.running = True
        self.setup_vlc()
        
    def setup_vlc(self):
        vlc_opts = [
            '--quiet',
            '--no-video',
            '--file-caching=5000',
            '--audio-resampler=soxr',
            '--aout=auhal'  # Use Direct IO for macOS
        ]
        
        try:
            self.instance = vlc.Instance(' '.join(vlc_opts))
            if not self.instance:
                raise Exception("Could not create VLC instance")
            self.player = self.instance.media_player_new()
        except Exception as e:
            print(f"Error creating VLC instance: {e}", file=sys.stderr)
            sys.exit(1)

    def start(self):
        # Remove socket file if it exists
        try:
            os.unlink(self.socket_path)
        except OSError:
            if os.path.exists(self.socket_path):
                raise

        # Create Unix domain socket
        self.server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.server.bind(self.socket_path)
        self.server.listen(1)
        
        # Handle SIGTERM gracefully
        signal.signal(signal.SIGTERM, self.handle_signal)
        signal.signal(signal.SIGINT, self.handle_signal)

        while self.running:
            try:
                conn, addr = self.server.accept()
                data = conn.recv(1024).decode()
                if data:
                    response = self.handle_command(data)
                    conn.send(response.encode())
                conn.close()
            except Exception as e:
                if self.running:  # Only print error if we're still meant to be running
                    print(f"Error handling connection: {e}", file=sys.stderr)

        # Cleanup
        self.cleanup()

    def cleanup(self):
        try:
            self.player.stop()
            self.server.close()
            os.unlink(self.socket_path)
        except:
            pass

    def handle_signal(self, signum, frame):
        self.running = False
        self.cleanup()
        sys.exit(0)

    def handle_command(self, command_str):
        try:
            print(f"Debug: Received command string: {command_str}", file=sys.stderr)
            command = json.loads(command_str)
            action = command.get('action')
            print(f"Debug: Parsed action: {action}", file=sys.stderr)

            if action == 'set-device':
                device_id = command.get('device_id')
                print(f"Debug: Daemon received set-device command for ID: {device_id}", file=sys.stderr)
                if device_id is not None:
                    try:
                        # Stop current playback
                        self.player.stop()
                        
                        print(f"Debug: Listing available devices:", file=sys.stderr)
                        # Get list of devices for debugging
                        mods = self.player.audio_output_device_enum()
                        if mods:
                            mod = mods
                            while mod:
                                mod = mod.contents
                                if mod.device:
                                    d_id = mod.device.decode('utf-8')
                                    d_name = mod.description.decode('utf-8')
                                    print(f"Debug: Available device - ID: {d_id}, Name: {d_name}", file=sys.stderr)
                                if not mod.next:
                                    break
                                mod = mod.next
                            vlc.libvlc_audio_output_device_list_release(mods)

                        # Try to set device
                        print(f"Debug: Attempting direct device set with ID: {device_id}", file=sys.stderr)
                        result = self.player.audio_output_device_set("auhal", str(device_id).encode('utf-8'))
                        print(f"Debug: Set device result: {result}", file=sys.stderr)
                        
                        return json.dumps({"status": "ok", "message": f"Device set to {device_id}"})
                    except Exception as e:
                        error_msg = f"Failed to set device: {str(e)}"
                        print(f"Debug: Error in daemon: {error_msg}", file=sys.stderr)
                        return json.dumps({"status": "error", "message": error_msg})
            
            elif action == 'play':
                station_url = command.get('url')
                if station_url:
                    media = self.instance.media_new(station_url)
                    self.player.set_media(media)
                    self.player.play()
                    return json.dumps({"status": "ok", "message": "Playing"})
                    
            elif action == 'stop':
                self.player.stop()
                return json.dumps({"status": "ok", "message": "Stopped"})
                
            elif action == 'volume':
                volume = command.get('level', 100)
                self.player.audio_set_volume(int(volume))
                return json.dumps({"status": "ok", "message": f"Volume set to {volume}"})
                
            elif action == 'status':
                state = str(self.player.get_state())
                volume = self.player.audio_get_volume()
                return json.dumps({
                    "status": "ok",
                    "state": state,
                    "volume": volume
                })
                
            print(f"Debug: Unknown action: {action}", file=sys.stderr)
            return json.dumps({"status": "error", "message": "Unknown command"})
            
        except Exception as e:
            print(f"Debug: Exception in handle_command: {e}", file=sys.stderr)
            return json.dumps({"status": "error", "message": str(e)})

class RadioClient:
    def __init__(self, socket_path="/tmp/radio.sock", stations_file=None):
        self.socket_path = socket_path
        if stations_file is None:
            stations_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'stations.json')
        self.stations_file = stations_file
        self.load_stations()

    def load_stations(self):
        try:
            with open(self.stations_file, 'r') as f:
                self.stations = json.load(f)
        except FileNotFoundError:
            print(f"No stations file found at {self.stations_file}", file=sys.stderr)
            self.stations = {}

    def set_audio_device(self, device_id):
        print(f"Debug: Client sending set-device command with ID: {device_id}", file=sys.stderr)
        response = self.send_command({"action": "set-device", "device_id": str(device_id)})
        print(f"Debug: Client received response: {response}", file=sys.stderr)
        if response["status"] == "ok":
            print(json.dumps({"status": "success", "message": response.get("message", "Device set")}))
            return 0
        else:
            print(json.dumps({"status": "error", "message": response.get("message", "Unknown error")}))
            return 1

    def send_command(self, command):
        try:
            print(f"Debug: Client attempting to send command: {command}", file=sys.stderr)
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sock.connect(self.socket_path)
            command_str = json.dumps(command)
            print(f"Debug: Sending command string: {command_str}", file=sys.stderr)
            sock.send(command_str.encode())
            response = sock.recv(1024).decode()
            print(f"Debug: Received raw response: {response}", file=sys.stderr)
            sock.close()
            return json.loads(response)
        except ConnectionRefusedError:
            print("Debug: Connection refused - daemon not running", file=sys.stderr)
            return {"status": "error", "message": "Daemon not running"}
        except Exception as e:
            print(f"Debug: Send command error: {str(e)}", file=sys.stderr)
            return {"status": "error", "message": f"Communication error: {e}"}

    def play(self, station_name):
        station_url = self.stations.get(station_name)
        if not station_url:
            for full_name, url in self.stations.items():
                if full_name.split('.')[1] == station_name:
                    station_url = url
                    break
        
        if not station_url:
            print(f"Station not found: {station_name}", file=sys.stderr)
            return 1
            
        response = self.send_command({"action": "play", "url": station_url})
        if response["status"] == "ok":
            print(f"Playing {station_name}")
            return 0
        else:
            print(f"Error: {response.get('message')}", file=sys.stderr)
            return 1

    def stop(self):
        response = self.send_command({"action": "stop"})
        if response["status"] == "ok":
            print("Playback stopped")
            return 0
        else:
            print(f"Error: {response.get('message')}", file=sys.stderr)
            return 1

    def set_volume(self, volume):
        try:
            volume = max(0, min(100, int(volume)))
            response = self.send_command({"action": "volume", "level": volume})
            if response["status"] == "ok":
                print(f"Volume set to {volume}%")
                return 0
            else:
                print(f"Error: {response.get('message')}", file=sys.stderr)
                return 1
        except ValueError:
            print("Volume must be a number between 0 and 100", file=sys.stderr)
            return 1

    def get_status(self):
        response = self.send_command({"action": "status"})
        if response["status"] == "ok":
            print(json.dumps(response, indent=2))
            return 0
        else:
            print(f"Error: {response.get('message')}", file=sys.stderr)
            return 1

    def list_stations(self):
        for station_name in sorted(self.stations.keys()):
            print(station_name)
        return 0

def start_daemon():
    """Start the radio daemon process"""
    print("Debug: Starting daemon...", file=sys.stderr)
    daemon_script = os.path.abspath(__file__)
    
    # Kill any existing daemon
    print("Debug: Cleaning up old daemon...", file=sys.stderr)
    subprocess.run(["pkill", "-f", f"{daemon_script} --daemon"])
    
    if os.path.exists("/tmp/radio.sock"):
        print("Debug: Removing old socket file...", file=sys.stderr)
        os.unlink("/tmp/radio.sock")
    
    print("Debug: Launching new daemon process...", file=sys.stderr)
    subprocess.Popen([sys.executable, daemon_script, '--daemon'])
    
    # Wait for daemon to start
    retries = 5
    while retries > 0:
        if os.path.exists("/tmp/radio.sock"):
            print("Debug: Daemon started successfully", file=sys.stderr)
            return True
        print(f"Debug: Waiting for daemon... ({retries} retries left)", file=sys.stderr)
        time.sleep(1)
        retries -= 1
    
    print("Debug: Failed to start daemon", file=sys.stderr)
    return False

def main():
    parser = argparse.ArgumentParser(description='Command-line radio player')
    parser.add_argument('--daemon', action='store_true', help=argparse.SUPPRESS)
    parser.add_argument('--stations-file', help='Path to stations JSON file')
    
    subparsers = parser.add_subparsers(dest='command', help='Command to execute')
    
    # Play command
    play_parser = subparsers.add_parser('play', help='Play a station')
    play_parser.add_argument('station', help='Station name to play')
    
    # Stop command
    subparsers.add_parser('stop', help='Stop playback')
    
    # Volume command
    volume_parser = subparsers.add_parser('volume', help='Set volume')
    volume_parser.add_argument('level', type=int, help='Volume level (0-100)')
    
    # List stations command
    subparsers.add_parser('list-stations', help='List available stations')
    
    # Status command
    subparsers.add_parser('status', help='Get player status')
    
    # Devices commands
    subparsers.add_parser('devices', help='List audio devices')
    device_parser = subparsers.add_parser('set-device', help='Set audio device')
    device_parser.add_argument('device_id', type=str, help='Device ID number')
    
    args = parser.parse_args()
    
    if args.daemon:
        # Run as daemon
        daemon = RadioDaemon()
        daemon.start()
        return 0
    
    # Check if daemon is running, start if not
    if not os.path.exists("/tmp/radio.sock"):
        start_daemon()
    
    # Handle client commands
    client = RadioClient(stations_file=args.stations_file)
    
    if args.command == 'play':
        return client.play(args.station)
    elif args.command == 'stop':
        return client.stop()
    elif args.command == 'volume':
        return client.set_volume(args.level)
    elif args.command == 'list-stations':
        return client.list_stations()
    elif args.command == 'status':
        return client.get_status()
    elif args.command == 'devices':
        # Create VLC instance to get devices
        instance = vlc.Instance('--quiet')
        player = instance.media_player_new()
        devices = {}
        
        # Get audio outputs
        mods = player.audio_output_device_enum()
        if mods:
            mod = mods
            index = 0
            while mod:
                mod = mod.contents
                if mod.device and mod.description:
                    device_id = mod.device.decode('utf-8')
                    device_name = mod.description.decode('utf-8')
                    devices[str(index)] = {
                        'name': device_name,
                        'device_id': device_id,
                        'use_this_id': device_id  # Make it clear which ID to use
                    }
                    index += 1
                
                if not mod.next:
                    break
                mod = mod.next
            
            vlc.libvlc_audio_output_device_list_release(mods)
        
        print(json.dumps({"status": "success", "devices": devices}))
        return 0
    elif args.command == 'set-device':
        print(f"Debug: Processing set-device command with arg: {args.device_id}", file=sys.stderr)
        
        # Make sure daemon is running
        if not os.path.exists("/tmp/radio.sock"):
            print("Debug: Daemon not running, attempting to start...", file=sys.stderr)
            if not start_daemon():
                print(json.dumps({"status": "error", "message": "Failed to start daemon"}))
                return 1
        
        # Get the devices first to validate the device_id
        instance = vlc.Instance('--quiet')
        player = instance.media_player_new()
        valid_devices = {}
        
        mods = player.audio_output_device_enum()
        if mods:
            mod = mods
            while mod:
                mod = mod.contents
                if mod.device and mod.description:
                    device_name = mod.description.decode('utf-8')
                    device_id_str = mod.device.decode('utf-8')
                    valid_devices[device_id_str] = device_name
                    print(f"Debug: Found device: {device_name} with ID: {device_id_str}", file=sys.stderr)
                if not mod.next:
                    break
                mod = mod.next
            vlc.libvlc_audio_output_device_list_release(mods)
        
        if not valid_devices:
            print(json.dumps({"status": "error", "message": "No audio devices found"}))
            return 1
        
        print(f"Debug: Valid device IDs: {list(valid_devices.keys())}", file=sys.stderr)
        print(f"Debug: Attempting to set device ID: {args.device_id}", file=sys.stderr)
        
        # Restart daemon to ensure clean state
        print("Debug: Restarting daemon for clean state...", file=sys.stderr)
        start_daemon()
        
        # Send the command to the daemon
        try:
            client = RadioClient()
            command = {
                "action": "set-device",
                "device_id": str(args.device_id)
            }
            print(f"Debug: Sending command to daemon: {command}", file=sys.stderr)
            response = client.send_command(command)
            print(f"Debug: Daemon response: {response}", file=sys.stderr)
            
            if response["status"] == "ok":
                device_name = valid_devices.get(args.device_id, "Unknown Device")
                print(json.dumps({
                    "status": "success",
                    "message": f"Audio device set to: {device_name}"
                }))
                return 0
            else:
                print(json.dumps(response))
                return 1
                
        except Exception as e:
            print(f"Debug: Error: {str(e)}", file=sys.stderr)
            print(json.dumps({
                "status": "error",
                "message": f"Failed to set device: {str(e)}"
            }))
            return 1
    else:
        parser.print_help()
        return 1

if __name__ == "__main__":
    sys.exit(main())