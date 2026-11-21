"""Serve the local StackDesk browser UI. The native C++ program owns all desk rules."""
import argparse
from datetime import date
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory
from threading import Lock
from urllib.parse import parse_qs, urlsplit

ROOT = Path(__file__).resolve().parent
BINARY = ROOT.parent / "main"
ASSETS = {
    "/": ("index.html", "text/html; charset=utf-8"),
    "/styles.css": ("styles.css", "text/css; charset=utf-8"),
    "/app.js": ("app.js", "text/javascript; charset=utf-8"),
}
ACTIONS = {
    "add-book": 4, "edit-book": 4, "set-topics": 2, "add-reader": 4, "edit-reader": 4,
    "borrow": 2, "return": 2, "renew": 2, "reserve": 2, "cancel-hold": 2, "advance": 1,
}
CSP = ("default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; "
       "img-src 'self' data:; object-src 'none'; base-uri 'none'; form-action 'self'; "
       "frame-ancestors 'self'")


class ApiError(Exception):
    def __init__(self, status, message):
        super().__init__(message)
        self.status = status


def desk_argument(value):
    """Encode one field in the existing batch grammar; a field cannot start a second command."""
    if not isinstance(value, str) or any(ord(c) < 32 or ord(c) == 127 for c in value):
        raise ApiError(HTTPStatus.BAD_REQUEST, "Fields must be single-line text.")
    try:
        length = len(value.encode("utf-8"))
    except UnicodeEncodeError as error:
        raise ApiError(HTTPStatus.BAD_REQUEST, "Invalid text encoding.") from error
    if length > 160:
        raise ApiError(HTTPStatus.BAD_REQUEST, "Fields must be at most 160 UTF-8 bytes.")
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


class Desk:
    def __init__(self, database, binary=BINARY, demo=False):
        self.database = Path(database).resolve()
        self.binary = Path(binary).resolve()
        self.demo = demo
        self.demo_day = "2026-11-01"
        self.lock = Lock()

    def _run(self, commands):
        argv = [str(self.binary), "--db", str(self.database), "--batch"]
        if self.demo:
            argv += ["--date", self.demo_day]
        try:
            result = subprocess.run(argv, input=commands, encoding="utf-8", errors="replace",
                                    capture_output=True, timeout=12, check=False)
        except (OSError, subprocess.TimeoutExpired) as error:
            print(f"StackDesk failed to run: {error}", file=sys.stderr)
            raise ApiError(HTTPStatus.SERVICE_UNAVAILABLE, "The C++ program could not be started.") from error
        if result.returncode == 1:
            diagnostic = next((line[7:] for line in result.stdout.splitlines() if line.startswith("ERROR: ")),
                              "The library program rejected the action.")
            raise ApiError(HTTPStatus.UNPROCESSABLE_ENTITY, diagnostic)
        if result.returncode != 0:
            print(result.stderr.rstrip(), file=sys.stderr)
            raise ApiError(HTTPStatus.SERVICE_UNAVAILABLE,
                           "Could not open the library database. Check the terminal for details.")
        return result.stdout.rstrip("\r\n")

    def _snapshot(self, output):
        state = json.loads(output)
        state["demo"] = self.demo
        if self.demo:
            self.demo_day = date.fromisoformat(state["day"]).isoformat()
        return state

    def state(self):
        with self.lock:
            return self._snapshot(self._run("snapshot\n"))

    def search(self, query):
        field = desk_argument(query.strip(" "))
        with self.lock:
            return json.loads(self._run(f"catalog-search {field}\n"))

    def recommendations(self, kind, item_id):
        command = "recommendations-reader" if kind == "reader" else "recommendations-book"
        field = desk_argument(item_id)
        with self.lock:
            return json.loads(self._run(f"{command} {field}\n"))

    def act(self, name, arguments):
        if not isinstance(name, str) or name not in ACTIONS:
            raise ApiError(HTTPStatus.BAD_REQUEST, "Unknown desk action.")
        if not isinstance(arguments, list) or len(arguments) != ACTIONS[name]:
            raise ApiError(HTTPStatus.BAD_REQUEST, "The action has an incorrect number of fields.")
        if name == "advance" and not self.demo:
            raise ApiError(HTTPStatus.FORBIDDEN, "The date can be changed only in a demo session.")
        fields = " ".join(desk_argument(a) for a in arguments)
        with self.lock:
            output = self._run(f"{name} {fields}\nsnapshot\n")
            message, snapshot = output.rsplit("\n", 1)
            return {"message": message, "state": self._snapshot(snapshot)}

    def overdue_csv(self):
        # The export path is allocated by the server. The browser sends no path.
        with self.lock, TemporaryDirectory(prefix="stackdesk-csv-") as directory:
            path = Path(directory) / "overdue.csv"
            escaped_path = desk_argument(str(path))
            self._run(f"export-overdue {escaped_path}\n")
            return path.read_bytes()

    def seed_demo(self):
        if not self.demo or self.database.exists():
            raise RuntimeError("demo seeding needs a new demo database")
        self._run('''add-book BK101 "Practical C++" "Ana Reed" 1
add-book BK202 "Data Structures" "Jun Wu" 3
add-book BK314 "Computer Systems" "Maya Patel" 2
add-book BK410 "Algorithms in Practice" "Lena Torres" 3
add-book BK532 "Database Design" "Nora Bennett" 2
add-book BK608 "The Pragmatic Programmer" "Andrew Hunt & David Thomas" 2
add-book BK719 "Introduction to Linear Algebra" "Gilbert Strang" 2
set-topics BK101 "C++, programming, beginner"
set-topics BK202 "data structures, algorithms, programming"
set-topics BK314 "computer architecture, systems"
set-topics BK410 "algorithms, problem solving, programming"
set-topics BK532 "databases, SQL"
set-topics BK608 "programming, software craft"
set-topics BK719 "mathematics, linear algebra"
add-reader S01 "Alex Chan" alex@example.invalid student
add-reader F01 "Taylor Lee" taylor@example.invalid faculty
add-reader S02 "Jamie Ho" jamie@example.invalid student
add-reader S03 "Morgan Lam" morgan@example.invalid student
add-reader F02 "Riley Wong" riley@example.invalid faculty
add-reader S04 "Sam Park" sam@example.invalid student
borrow BK532 S03
borrow BK608 S04
borrow BK101 F01
return BK101 F01
borrow BK608 F01
return BK608 F01
borrow BK101 F02
return BK101 F02
borrow BK608 F02
return BK608 F02
advance 2026-11-21
return BK608 S04
borrow BK101 S01
borrow BK202 F01
borrow BK314 S02
borrow BK410 F02
reserve BK101 F01
reserve BK101 S02
''')
        self.demo_day = "2026-11-21"


class LocalServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address, desk):
        self.desk = desk
        super().__init__(address, Handler)


class Handler(BaseHTTPRequestHandler):
    def send_data(self, status, data, content_type, download=None):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Security-Policy", CSP)
        self.send_header("Referrer-Policy", "no-referrer")
        if download:
            self.send_header("Content-Disposition", f'attachment; filename="{download}"')
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(data)

    def send_json(self, status, value):
        self.send_data(status, json.dumps(value, ensure_ascii=False).encode("utf-8"), "application/json; charset=utf-8")

    def local_request(self):
        authority = self.headers.get("Host", "")
        try:
            hostname = urlsplit("//" + authority).hostname
        except ValueError:
            hostname = None
        if hostname not in {"localhost", "127.0.0.1", "::1"}:
            raise ApiError(HTTPStatus.FORBIDDEN, "Use the local StackDesk address.")
        origin = self.headers.get("Origin")
        if origin and origin not in {"http://" + authority, "https://" + authority}:
            raise ApiError(HTTPStatus.FORBIDDEN, "Cross-origin requests are disabled.")
        if self.headers.get("Sec-Fetch-Site") in {"cross-site", "same-site"} and self.path.startswith("/api/"):
            raise ApiError(HTTPStatus.FORBIDDEN, "Open the StackDesk workspace first.")

    def execute(self, write=False):
        try:
            self.local_request()
            desk = self.server.desk
            url = urlsplit(self.path)
            if write:
                if url.path != "/api/actions":
                    raise ApiError(HTTPStatus.NOT_FOUND, "Page not found.")
                if self.headers.get_content_type() != "application/json":
                    raise ApiError(HTTPStatus.UNSUPPORTED_MEDIA_TYPE, "Send the action as JSON.")
                try:
                    size = int(self.headers.get("Content-Length", "0"))
                    if not 0 < size <= 8192:
                        raise ValueError("size outside limit")
                    payload = json.loads(self.rfile.read(size).decode("utf-8"))
                    if not isinstance(payload, dict):
                        raise ValueError("expected an object")
                except (ValueError, UnicodeError) as error:
                    raise ApiError(HTTPStatus.BAD_REQUEST, "Invalid action payload.") from error
                response = desk.act(payload.get("action"), payload.get("args"))
                self.send_json(HTTPStatus.OK, response)
            elif url.path in ASSETS:
                name, content_type = ASSETS[url.path]
                self.send_data(HTTPStatus.OK, (ROOT / name).read_bytes(), content_type)
            elif url.path == "/api/state":
                self.send_json(HTTPStatus.OK, desk.state())
            elif url.path == "/api/search":
                term = parse_qs(url.query, keep_blank_values=True).get("q", [""])[0]
                self.send_json(HTTPStatus.OK, desk.search(term))
            elif url.path == "/api/recommendations":
                params = parse_qs(url.query, keep_blank_values=True)
                if len(params) != 1:
                    raise ApiError(HTTPStatus.BAD_REQUEST, "Choose a book or a reader.")
                kind, values = next(iter(params.items()))
                if kind not in {"book", "reader"} or len(values) != 1 or not values[0]:
                    raise ApiError(HTTPStatus.BAD_REQUEST, "Choose a valid book or reader ID.")
                self.send_json(HTTPStatus.OK, desk.recommendations(kind, values[0]))
            elif url.path == "/api/overdue.csv":
                self.send_data(HTTPStatus.OK, desk.overdue_csv(), "text/csv; charset=utf-8", "stackdesk-overdue.csv")
            else:
                raise ApiError(HTTPStatus.NOT_FOUND, "Page not found.")
        except ApiError as error:
            self.send_json(error.status, {"error": str(error)})
        except (ValueError, OSError) as error:
            if isinstance(error, (BrokenPipeError, ConnectionResetError)):
                return
            print(f"StackDesk request failed: {error}", file=sys.stderr)
            self.send_json(HTTPStatus.INTERNAL_SERVER_ERROR, {"error": "Could not read the requested data."})

    def do_GET(self):
        self.execute()

    def do_HEAD(self):
        self.execute()

    def do_POST(self):
        self.execute(write=True)


def main():
    parser = argparse.ArgumentParser(description="Open the StackDesk browser workspace locally.")
    parser.add_argument("--db", type=Path, default=None, help="Session database (default: stackdesk.db)")
    parser.add_argument("--demo", action="store_true", help="Use fictional records in a temporary database")
    parser.add_argument("--host", choices=("127.0.0.1", "localhost"), default="127.0.0.1")
    parser.add_argument("--port", type=int, default=os.environ.get("PORT", "8765"))
    # Fixed-port startup already fails on a port conflict.
    parser.add_argument("--strictPort", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.demo and args.db is not None:
        parser.error("--demo uses a separate temporary database")
    if not args.demo and args.db is None:
        args.db = Path("stackdesk.db")

    def serve(desk):
        desk.state()
        with LocalServer((args.host, args.port), desk) as server:
            print(f"StackDesk Browser UI: http://127.0.0.1:{server.server_port}/", flush=True)
            try:
                server.serve_forever()
            except KeyboardInterrupt:
                pass

    try:
        if args.demo:
            with TemporaryDirectory(prefix="stackdesk-web-demo-") as folder:
                desk = Desk(Path(folder) / "demo.db", demo=True)
                desk.seed_demo()
                serve(desk)
        else:
            serve(Desk(args.db))
    except (ApiError, OSError) as error:
        print(f"StackDesk: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
