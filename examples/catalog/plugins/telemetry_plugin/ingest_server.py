#!/usr/bin/env python3
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import parse_qs, urlparse
import sys


class Handler(BaseHTTPRequestHandler):
    log_path = "telemetry_requests.log"

    def do_GET(self):
        parsed = urlparse(self.path)
        query = parse_qs(parsed.query, keep_blank_values=True)
        # Flatten lists for easy human reading.
        flat = {k: (v[0] if v else "") for k, v in sorted(query.items())}
        line = f"path={parsed.path} query={flat}\n"
        with open(self.log_path, "a", encoding="utf-8") as f:
            f.write(line)
        self.send_response(204)
        self.end_headers()

    def log_message(self, _fmt, *_args):
        return


def main():
    log_path = sys.argv[1] if len(sys.argv) > 1 else "telemetry_requests.log"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 8787
    Handler.log_path = log_path
    server = HTTPServer(("127.0.0.1", port), Handler)
    print(f"Listening on http://127.0.0.1:{port}, logging to {log_path}")
    server.serve_forever()


if __name__ == "__main__":
    main()
