"""Test ESP32 camera connection and endpoints"""
import requests
import sys

def test_esp_connection(base_url='http://192.168.4.1'):
    print(f"Testing ESP32 camera at {base_url}...")
    
    # Test basic connectivity
    try:
        r = requests.get(f"{base_url}/color.json", timeout=5)
        if r.status_code == 200:
            data = r.json()
            print(f"✓ Color endpoint working: RGB=({data['r']},{data['g']},{data['b']}) Color={data['name']}")
        else:
            print(f"✗ Color endpoint returned {r.status_code}")
    except Exception as e:
        print(f"✗ Cannot reach color endpoint: {e}")
        
    # Test frame endpoint (use GET instead of HEAD)
    try:
        r = requests.get(f"{base_url}/frame.ppm?ts=1", timeout=10, stream=True)
        if r.status_code == 200:
            content_type = r.headers.get('content-type', '')
            print(f"✓ Frame endpoint working (Content-Type: {content_type})")
            # Don't download the full image, just check headers
            r.close()
        else:
            print(f"✗ Frame endpoint returned {r.status_code}")
    except Exception as e:
        print(f"✗ Cannot reach frame endpoint: {e}")
        
    # Test settings endpoint
    try:
        r = requests.get(f"{base_url}/settings", timeout=5)
        if r.status_code == 200:
            print(f"✓ Settings endpoint working")
        else:
            print(f"✗ Settings endpoint returned {r.status_code}")
    except Exception as e:
        print(f"✗ Cannot reach settings endpoint: {e}")

if __name__ == '__main__':
    if len(sys.argv) > 1:
        test_esp_connection(sys.argv[1])
    else:
        test_esp_connection()