"""Integration checks for the browser API, using the real C++ binary and temporary databases."""
from contextlib import contextmanager
import csv
from http.client import HTTPConnection
from io import StringIO
import json
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory
from threading import Thread
from urllib.parse import quote

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "web"))
from server import Desk, LocalServer  # noqa: E402

BINARY = Path(sys.argv[1] if len(sys.argv) > 1 else "./main").resolve()


@contextmanager
def running(database, demo=False):
    desk = Desk(database, binary=BINARY, demo=demo)
    if demo:
        desk.seed_demo()
    with LocalServer(("127.0.0.1", 0), desk) as server:
        thread = Thread(target=server.serve_forever, kwargs={"poll_interval": 0.02}, daemon=True)
        thread.start()
        try:
            yield server.server_port
        finally:
            server.shutdown()
            thread.join()


def request(port, path, *, method="GET", payload=None, status=200, headers=None):
    conn = HTTPConnection("127.0.0.1", port, timeout=4)
    parameters = {}
    if payload is not None:
        parameters["body"] = json.dumps(payload).encode("utf-8")
        parameters["headers"] = {"Content-Type": "application/json"}
    parameters.setdefault("headers", {}).update(headers or {})
    try:
        conn.request(method, path, **parameters)
        res = conn.getresponse()
        raw = res.read()
        assert res.status == status, (path, res.status, status, raw[:400])
        if "application/json" in res.getheader("Content-Type", ""):
            return json.loads(raw)
        return raw, dict(res.getheaders())
    finally:
        conn.close()


def action(port, name, *args, status=200, headers=None):
    return request(port, "/api/actions", method="POST", payload={"action": name, "args": list(args)},
                   status=status, headers=headers)


def book(state, ident):
    return next(item for item in state["books"] if item["id"] == ident)


def find(port, query):
    return request(port, "/api/search?q=" + quote(query))


def demo_flow(database):
    with running(database, demo=True) as port:
        html, headers = request(port, "/")
        assert b'<html lang="en">' in html
        assert b"<title>StackDesk Library</title>" in html
        assert "script-src 'self'" in headers["Content-Security-Policy"]
        assert b'"use strict"' in request(port, "/app.js")[0]
        print("PASS W01 English HTML, browser assets and response headers")

        initial = request(port, "/api/state")
        assert initial["demo"] and initial["day"] == "2026-11-21"
        assert len(initial["books"]) == 7 and len(initial["readers"]) == 6
        assert initial["policies"] == [
            {"tier":"STUDENT", "days":14, "maxLoans":3, "maxRenewals":1},
            {"tier":"FACULTY", "days":28, "maxLoans":5, "maxRenewals":2},
        ]
        assert book(initial,"BK101")["holds"] == [
            {"reader":"F01", "readyThrough":None}, {"reader":"S02", "readyThrough":None},
        ]
        print("PASS W02 C++ snapshot, demo seed and virtual loan policies")

        assert find(port, "bk101") == [{"id":"BK101","fuzzy":False}]
        assert find(port, "jun structures") == [{"id":"BK202","fuzzy":False}]
        assert find(port, "pratical C++") == [{"id":"BK101","fuzzy":True}]
        assert find(port, "pracitcal") == [{"id":"BK101","fuzzy":True}]
        assert find(port, "quantum astrophysics") == []
        print("PASS W03 exact ID, multi-token author/title and spelling correction")

        before = database.read_bytes()
        blocked = action(port, "borrow", "BK101", "S02", status=422)
        assert blocked["error"] == "reader is still waiting"
        assert database.read_bytes() == before
        returned = action(port, "return", "BK101", "S01")["state"]
        b = book(returned,"BK101")
        assert (b["available"],b["borrowed"],b["ready"]) == (0,0,1)
        assert b["holds"][0] == {"reader":"F01","readyThrough":"2026-11-23"}
        print("PASS W04 checkout rejection, actual return and first pickup deadline")

        moved = action(port, "advance", "2026-11-24")["state"]
        assert moved["day"] == "2026-11-24"
        assert book(moved,"BK101")["holds"] == [{"reader":"S02","readyThrough":"2026-11-26"}]
        assert request(port,"/api/state") == moved
        collected = action(port, "borrow", "BK101", "S02")["state"]
        assert not book(collected,"BK101")["holds"] and book(collected,"BK101")["borrowed"] == 1
        assert any(x["reader"] == "S02" and x["book"] == "BK101" and x["due"] == "2026-12-08"
                   for x in collected["loans"])
        print("PASS W05 expired hold handoff, reload and pickup checkout")

        csv_bytes, headers = request(port, "/api/overdue.csv")
        rows = list(csv.DictReader(StringIO(csv_bytes.decode("utf-8"))))
        assert headers["Content-Disposition"] == 'attachment; filename="stackdesk-overdue.csv"'
        assert [(r["book_id"],r["reader_id"],r["days_late"]) for r in rows] == [("BK532","S03","9")]
        print("PASS W06 C++ overdue calculation and CSV download")

        for options in ({"headers":{"Origin":"https://other.example"}},
                        {"headers":{"Host":"other.example"}}):
            request(port,"/api/state",status=403,**options)
        before = database.read_bytes()
        invalid = action(port,"add-book","B9","Two\nlines","Writer","1",status=400)
        assert "single-line" in invalid["error"]
        action(port,"not-an-action","B9",status=400)
        assert database.read_bytes() == before
        assert request(port,"/server.py",status=404)["error"] == "Page not found."
        print("PASS W07 local-only request validation and safe argument encoding")


def persistent_flow(database):
    with running(database) as port:
        state = request(port, "/api/state")
        assert not state["demo"] and not state["books"]
        action(port,"advance","2040-01-01",status=403)
        title = 'Chemistry, "Lab" \\ 系統'
        action(port,"add-book","B-1",title,"Ana Reed","2")
        action(port,"add-reader","R_1","Reader","r@example.invalid","FACULTY")
        r = action(port,"borrow","B-1","R_1")["state"]
        assert book(r,"B-1")["title"] == title and book(r,"B-1")["available"] == 1
        action(port,"edit-book","B-1",title,"New Author","2")
        r = action(port,"edit-reader","R_1","Updated Reader","new@example.invalid","STUDENT")["state"]
        assert r["loans"][0]["tier"] == "FACULTY" and r["readers"][0]["tier"] == "STUDENT"
    with running(database) as port:
        restored = request(port,"/api/state")
        assert restored["books"] == r["books"] and restored["readers"] == r["readers"]
        assert restored["loans"] == r["loans"]
    direct = subprocess.run([str(BINARY),"--db",str(database),"--batch"], input="snapshot\n",
                            capture_output=True,text=True,check=True)
    assert json.loads(direct.stdout)["loans"] == r["loans"]
    print("PASS W08 normal desk edits, restart and CLI compatibility with the same database")


if __name__ == "__main__":
    with TemporaryDirectory(prefix="stackdesk-web-test-") as paths:
        demo_flow(Path(paths) / "demo.db")
        persistent_flow(Path(paths) / "live.db")
    print("8 browser API scenarios passed.")
