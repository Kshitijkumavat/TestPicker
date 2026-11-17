import http.server
import socketserver
import subprocess
import urllib.parse
import json
import os
import csv

PORT = 8080

class TestPickerHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        # Enable CORS and disable aggressive caching for local dev
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
        super().end_headers()

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        query = urllib.parse.parse_qs(parsed.query)

        # API 1: Project Metadata & File List from C++ Engine
        if path == '/api/project':
            try:
                proc = subprocess.run(['./testpicker.exe', '--json-summary'],
                                      capture_output=True, text=True, check=True)
                data = json.loads(proc.stdout.strip())
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps(data).encode('utf-8'))
            except Exception as e:
                self.send_response(500)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps({'error': str(e)}).encode('utf-8'))
            return

        # API 2: Analyze Commit via C++ Executable
        if path == '/api/analyze':
            budget = query.get('budget', ['60'])[0]
            files_str = query.get('files', ['4,14,24'])[0]
            file_ids = [f.strip() for f in files_str.split(',') if f.strip().isdigit()]

            cmd = ['./testpicker.exe', '--json-analyze', budget] + file_ids
            try:
                proc = subprocess.run(cmd, capture_output=True, text=True, check=True)
                data = json.loads(proc.stdout.strip())
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps(data).encode('utf-8'))
            except Exception as e:
                self.send_response(500)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps({'error': str(e)}).encode('utf-8'))
            return

        # API 3: Simulate Execution Against Planted Bug in C++
        if path == '/api/simulate':
            strat = query.get('strat', ['3'])[0]
            budget = query.get('budget', ['60'])[0]
            files_str = query.get('files', ['4,14,24'])[0]
            file_ids = [f.strip() for f in files_str.split(',') if f.strip().isdigit()]

            cmd = ['./testpicker.exe', '--json-simulate', strat, budget] + file_ids
            try:
                proc = subprocess.run(cmd, capture_output=True, text=True, check=True)
                data = json.loads(proc.stdout.strip())
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps(data).encode('utf-8'))
            except Exception as e:
                self.send_response(500)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps({'error': str(e)}).encode('utf-8'))
            return

        # API 4: Benchmark Summary from results/summary.csv
        if path == '/api/benchmark':
            summary_path = 'results/summary.csv'
            rows = []
            if os.path.exists(summary_path):
                with open(summary_path, 'r', encoding='utf-8') as f:
                    reader = csv.DictReader(f)
                    for r in reader:
                        rows.append(r)
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps({'rows': rows}).encode('utf-8'))
            return

        # Default: Serve static HTML, CSS, JS
        return super().do_GET()

if __name__ == '__main__':
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("", PORT), TestPickerHandler) as httpd:
        print(f"TestPicker C++ Backend Server listening on port {PORT}...")
        httpd.serve_forever()
