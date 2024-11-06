
import sys
import json
import warnings
import urllib3

# Updated warning suppression
warnings.filterwarnings('ignore', category=Warning) # More general warning suppression
warnings.filterwarnings('ignore', category=DeprecationWarning)

# Import radio player after warning suppression
try:
    import radio_player
except Exception as e:
    print(json.dumps({"status": "error", "message": f"Failed to import radio_player: {str(e)}"}))
    sys.exit(1)

def init_radio():
    try:
        radio = radio_player.MultiChannelRadio()
        print(json.dumps({"status": "success"}))
    except Exception as e:
        print(json.dumps({"status": "error", "message": str(e)}))

def get_devices():
    try:
        radio = radio_player.MultiChannelRadio()
        devices = radio.get_audio_devices()
        # Ensure we have at least one device
        if not devices:
            print(json.dumps({"status": "error", "message": "No audio devices found"}))
            return
        # Convert to JSON and ensure proper encoding
        print(json.dumps({"status": "success", "devices": devices}, ensure_ascii=False))
    except Exception as e:
        print(json.dumps({"status": "error", "message": f"Error getting devices: {str(e)}"}))

def play_station(station, channel, device_id=None):
    try:
        print(json.dumps({"status": "debug", "message": f"Initializing radio for station: {station}, channel: {channel}, device: {device_id}"}))
        
        radio = radio_player.MultiChannelRadio()
        
        print(json.dumps({"status": "debug", "message": "Loading stations"}))
        # Load stations file
        stations_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'stations.json')
        if not radio.load_stations(stations_path):
            print(json.dumps({"status": "error", "message": "Failed to load stations file"}))
            return
            
        print(json.dumps({"status": "debug", "message": "Setting audio device"}))
        if device_id is not None:
            if not radio.set_audio_device(device_id):
                print(json.dumps({"status": "error", "message": f"Failed to set audio device: {device_id}"}))
                return
                
        print(json.dumps({"status": "debug", "message": "Playing station"}))
        success = radio.play(station, channel)
        
        if success:
            print(json.dumps({"status": "success", "message": "Station playing successfully"}))
        else:
            print(json.dumps({"status": "error", "message": "Failed to play station"}))
            
    except Exception as e:
        print(json.dumps({"status": "error", "message": str(e)}))

def stop_channel(channel):
    try:
        radio = radio_player.MultiChannelRadio()
        success = radio.stop(channel)
        print(json.dumps({"status": "success" if success else "error"}))
    except Exception as e:
        print(json.dumps({"status": "error", "message": str(e)}))

def stop_all():
    try:
        radio = radio_player.MultiChannelRadio()
        success = radio.stop_all()
        print(json.dumps({"status": "success" if success else "error"}))
    except Exception as e:
        print(json.dumps({"status": "error", "message": str(e)}))

def set_volume(channel, volume):
    try:
        radio = radio_player.MultiChannelRadio()
        success = radio.set_volume(channel, volume)
        print(json.dumps({"status": "success" if success else "error"}))
    except Exception as e:
        print(json.dumps({"status": "error", "message": str(e)}))

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(json.dumps({"status": "error", "message": "No command specified"}))
        sys.exit(1)

    command = sys.argv[1]
    if command == "init":
        init_radio()
    elif command == "devices":
        get_devices()
    elif command == "play" and len(sys.argv) >= 4:
        device_id = int(sys.argv[4]) if len(sys.argv) > 4 else None
        play_station(sys.argv[2], sys.argv[3], device_id)
    elif command == "stop" and len(sys.argv) >= 3:
        stop_channel(sys.argv[2])
    elif command == "stopall":
        stop_all()
    elif command == "volume" and len(sys.argv) >= 4:
        set_volume(sys.argv[2], float(sys.argv[3]))
    else:
        print(json.dumps({"status": "error", "message": "Invalid command"}))
