"""Local Flask server that proxies the ESP32 camera endpoints and provides a browser UI.

Usage:
  pip install -r tools/requirements.txt
  python tools/server_app.py --esp http://192.168.4.1 --host 0.0.0.0 --port 5000

The server fetches /frame.ppm and streams it to the browser, and forwards settings and color queries.
"""
from flask import Flask, Response, request, render_template_string, redirect, url_for
import requests
import argparse

app = Flask(__name__)

INDEX_HTML = '''
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <title>ESP Camera Local Proxy</title>
  <style>body{font-family:Arial;margin:16px} img{max-width:100%;border:1px solid #ccc}</style>
</head>
<body>
  <h2>ESP Camera (proxied)</h2>
  <div>
    <img id="frame" src="/frame" alt="frame"/>
  </div>
  <p>
    <button onclick="fetch('/settings?brightness=0').then(()=>alert('sent'))">Reset Brightness</button>
    <button onclick="fetch('/color').then(r=>r.json()).then(j=>alert(JSON.stringify(j)))">Get Center Color</button>
  </p>
  <script>
    function refresh(){
      const img = document.getElementById('frame');
      img.src = '/frame?ts=' + Date.now();
    }
    setInterval(refresh, 1000);
  </script>
</body>
</html>
'''

@app.route('/')
def index():
    return render_template_string(INDEX_HTML)

@app.route('/frame')
def frame():
    esp = app.config['ESP_BASE']
    params = dict(request.args)
    params['ts'] = params.get('ts', '')
    try:
        r = requests.get(f"{esp}/frame.ppm", params=params, stream=True, timeout=10)
    except Exception as e:
        return Response(f"Error fetching frame: {e}", status=502)
    # stream content back
    return Response(r.iter_content(chunk_size=4096), content_type=r.headers.get('Content-Type','application/octet-stream'))

@app.route('/color')
def color():
    esp = app.config['ESP_BASE']
    try:
        r = requests.get(f"{esp}/color.json", timeout=5)
        r.raise_for_status()
        return Response(r.content, content_type='application/json')
    except Exception as e:
        return Response(f"Error: {e}", status=502)

@app.route('/settings')
def settings():
    esp = app.config['ESP_BASE']
    params = dict(request.args)
    try:
        r = requests.get(f"{esp}/settings", params=params, timeout=5)
        return Response(r.content, status=r.status_code, content_type=r.headers.get('Content-Type','text/plain'))
    except Exception as e:
        return Response(f"Error: {e}", status=502)

if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--esp', default='http://192.168.4.1', help='Base URL of ESP device')
    p.add_argument('--host', default='127.0.0.1')
    p.add_argument('--port', type=int, default=5000)
    args = p.parse_args()
    app.config['ESP_BASE'] = args.esp.rstrip('/')
    print('Starting proxy server. ESP base =', app.config['ESP_BASE'])
    app.run(host=args.host, port=args.port, debug=False)
