import http.server
import socketserver
import gzip

class GzipHandler(http.server.SimpleHTTPRequestHandler):
    def do_HEAD(self):
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
        self.send_header('Content-Encoding', 'gzip')
        self.end_headers()

    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
        self.send_header('Content-Encoding', 'gzip')
        
        # We are going to serve chunked or with Content-Length
        # Let's serve with Content-Length to make it simple
        content = b"Hello, this is a gzipped response!\n" * 10
        compressed_content = gzip.compress(content)
        
        self.send_header('Content-Length', str(len(compressed_content)))
        self.end_headers()
        self.wfile.write(compressed_content)

if __name__ == "__main__":
    PORT = 8000
    with socketserver.TCPServer(("", PORT), GzipHandler) as httpd:
        print(f"Serving at port {PORT}")
        httpd.serve_forever()
