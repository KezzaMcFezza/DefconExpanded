#!/usr/bin/env python3
"""
Simple HTTP server for serving Defcon WebAssembly build with proper headers
for SharedArrayBuffer/threading support.

Usage: python serve-defcon.py [port]
Default port: 8000
"""

import http.server
import socketserver
import sys
import os

class DefconHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        # Required headers for SharedArrayBuffer/threading support
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        # Additional security headers
        self.send_header('Cache-Control', 'no-cache')
        super().end_headers()

def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    
    # Change to the build output directory
    build_dir = os.path.join(os.getcwd(), 'build', 'wasm-release', 'result', 'Release')
    if os.path.exists(build_dir):
        os.chdir(build_dir)
        print(f"Serving from: {build_dir}")
    else:
        print(f"Warning: Build directory not found at {build_dir}")
        print("Serving from current directory")
    
    with socketserver.TCPServer(("", port), DefconHTTPRequestHandler) as httpd:
        print(f"Server running at http://localhost:{port}/")
        print("Open http://localhost:{port}/defcon.html to play")
        print("Press Ctrl+C to stop")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nServer stopped")

if __name__ == "__main__":
    main() 