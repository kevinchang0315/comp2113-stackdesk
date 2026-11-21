#!/usr/bin/env python3
"""Exercise the compiled CLI in separate directories and capture reproducible evidence."""
import csv
import io
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory
import textwrap
import time

BINARY = Path(sys.argv[1] if len(sys.argv) > 1 else "./main").resolve()
START = "2026-01-01"


def block(text):
    return textwrap.dedent(text).strip("\n") + "\n" if text else ""


class Case:
    def __init__(self, root):
        self.root = root
        self.db = root / "catalog.db"
        self.evidence = []

    def run(self, commands, expected="", *, day=START, status=0, err_contains="",
            interactive=False, contains=(), options=()):
        args = ["--db", "catalog.db"]
        if day is not None:
            args += ["--date", day]
        if not interactive:
            args += ["--batch"]
        args += list(options)
        data = block(commands)
        result = subprocess.run([str(BINARY), *args], input=data, cwd=self.root,
                                text=True, capture_output=True, timeout=8)
        stderr = result.stderr.replace(str(self.root), "<case>")
        if contains:
            cursor = 0
            for fragment in contains:
                location = result.stdout.find(fragment, cursor)
                assert location >= 0, (fragment, result.stdout)
                cursor = location + len(fragment)
            wanted = "In order:\n" + "\n".join(contains)
        else:
            wanted = block(expected)
            assert result.stdout == wanted, (
                "stdout mismatch", data, "expected", wanted, "actual", result.stdout, stderr)
        assert result.returncode == status, (data, result.returncode, status, stderr)
        if err_contains:
            assert err_contains in stderr, (err_contains, stderr)
        else:
            assert stderr == "", stderr
        self.evidence.append((
            shlex.join(["./main", *args]), data, wanted.replace(str(self.root), "<case>"),
            result.stdout.replace(str(self.root), "<case>"), status, result.returncode,
            err_contains, stderr
        ))

    def check(self, instruction, actual, expected):
        assert actual == expected, (instruction, actual, expected)
        self.evidence.append((instruction, actual, expected))

    def fixture(self, description, path, content):
        path.write_text(content, encoding="utf-8")
        self.evidence.append((description, path.name, content))


CASES = []


def scenario(title):
    def register(test):
        CASES.append((title, test))
        return test
    return register


@scenario("Empty registry and read-only start")
def empty(c):
    c.run("""
        # Empty operations on a new database.
        books

        readers
        overdue
        stats
        exit
        unknown-after-exit
    """, """
        No matching books.
        No matching readers.
        No overdue loans.
        Stock on 2026-01-01
        Titles=0 Readers=0
        Copies=0 Available=0 On loan=0 Ready=0 Waiting=0
        Overdue=0 Completed loans=0
        Top titles: none
    """)
    c.check("Database exists after read-only session", c.db.exists(), False)


@scenario("Catalog search edit and ID validation")
def catalog(c):
    c.run("""
        add-book C1 "Chemical Principles" "Peter Atkins" 3
        add-book B2 Networks Wilson 1
        find-book ATKINS
        add-book C1 Duplicate Somebody 1
        edit-book C1 "Physical Chemistry" "P. Atkins" 2
        book C1
        books
        add-book "bad id" Title Author 1
        add-book B3 Title Author 0
        book B3
    """, """
        Added book C1
        Added book B2
        C1 | Chemical Principles | Peter Atkins | copies=3 available=3 ready=0
        ERROR: book ID already exists
        Updated book C1
        C1 | Physical Chemistry | P. Atkins | copies=2 available=2 ready=0
        B2 | Networks | Wilson | copies=1 available=1 ready=0
        C1 | Physical Chemistry | P. Atkins | copies=2 available=2 ready=0
        ERROR: invalid character in ID
        ERROR: integer out of range
        ERROR: unknown book B3
    """, status=1)


@scenario("Reader registration contact edits and validation")
def readers(c):
    c.run("""
        add-reader S1 "Jo Lin" jo@example.invalid student
        add-reader S1 Other other@example.invalid faculty
        add-reader A1 Demo no-contact visitor
        find-reader "jo l"
        edit-reader S1 "Jo Lin" new@example.invalid FACULTY
        reader S1
        find-reader nobody
    """, """
        Registered reader S1
        ERROR: reader ID already exists
        ERROR: tier must be STUDENT or FACULTY
        S1 | Jo Lin | STUDENT | jo@example.invalid
        Updated reader S1
        S1 | Jo Lin | FACULTY | new@example.invalid
        open=0 overdue=0 reservations=0
        No matching readers.
    """, status=1)


def registry(books, readers):
    return "".join(f"add-book {b} {b}-title Author {n}\n" for b, n in books) + "".join(
        f"add-reader {r} {r}-name {r}@example.invalid {t}\n" for r, t in readers)


def registered(books, readers):
    return "".join(f"Added book {b}\n" for b, _ in books) + "".join(
        f"Registered reader {r}\n" for r, _ in readers)


@scenario("Student and faculty dynamic dispatch after a tier change")
def policy_switch(c):
    books, readers = [("A", 2), ("B", 1)], [("S", "student"), ("F", "faculty")]
    c.run(registry(books, readers), registered(books, readers))
    c.run("""
        policy student
        policy faculty
        borrow A S
        borrow A F
        edit-reader S Student contact faculty
        renew A S
        borrow B S
        history S
    """, """
        STUDENT | days=14 limit=3 renewals=1
        FACULTY | days=28 limit=5 renewals=2
        Borrowed A by S due=2026-01-15
        Borrowed A by F due=2026-01-29
        Updated reader S
        Renewed A for S due=2026-01-29
        Borrowed B by S due=2026-01-29
        A | issued=2026-01-01 due=2026-01-29 renewals=1 | OPEN
        B | issued=2026-01-01 due=2026-01-29 renewals=0 | OPEN
    """)


@scenario("Loan limits for both tiers and duplicate borrowing")
def limits(c):
    books = [(f"B{i}", 2) for i in range(1, 7)]
    readers = [("S", "student"), ("F", "faculty")]
    c.run(registry(books, readers), registered(books, readers))
    c.run("""
        borrow B1 S
        borrow B1 S
        borrow B2 S
        borrow B3 S
        borrow B4 S
        borrow B1 F
        borrow B2 F
        borrow B3 F
        borrow B4 F
        borrow B5 F
        borrow B6 F
        return B1 S
        borrow B4 S
    """, """
        Borrowed B1 by S due=2026-01-15
        ERROR: reader already has this title
        Borrowed B2 by S due=2026-01-15
        Borrowed B3 by S due=2026-01-15
        ERROR: reader loan limit reached
        Borrowed B1 by F due=2026-01-29
        Borrowed B2 by F due=2026-01-29
        Borrowed B3 by F due=2026-01-29
        Borrowed B4 by F due=2026-01-29
        Borrowed B5 by F due=2026-01-29
        ERROR: reader loan limit reached
        Returned B1 by S
        Borrowed B4 by S due=2026-01-15
    """, status=1)


@scenario("Renewal counters and due date extensions")
def renewals(c):
    books, readers = [("B", 2)], [("S", "student"), ("F", "faculty")]
    c.run(registry(books, readers), registered(books, readers))
    c.run("""
        renew B S
        borrow B S
        renew B S
        renew B S
        borrow B F
        renew B F
        renew B F
        renew B F
    """, """
        ERROR: no open loan for this book and reader
        Borrowed B by S due=2026-01-15
        Renewed B for S due=2026-01-29
        ERROR: renewal limit reached
        Borrowed B by F due=2026-01-29
        Renewed B for F due=2026-02-26
        Renewed B for F due=2026-03-26
        ERROR: renewal limit reached
    """, status=1)


def one_copy(c, count=5, day=START):
    books, readers = [("B", 1)], [(f"R{i}", "student") for i in range(1, count + 1)]
    c.run(registry(books, readers), registered(books, readers), day=day)


@scenario("FIFO pickup and walk-in exclusion")
def fifo(c):
    one_copy(c)
    c.run("""
        reserve B R2
        borrow B R1
        reserve B R1
        reserve B R2
        reserve B R3
        reserve B R2
        renew B R1
        return B R1
        queue B
        borrow B R4
        borrow B R3
        borrow B R2
        queue B
        return B R2
        queue B
        return B R2
    """, """
        ERROR: copy available; borrow it at the desk
        Borrowed B by R1 due=2026-01-15
        ERROR: reader already has this title
        Reserved B for R2 position=1
        Reserved B for R3 position=2
        ERROR: reader already in queue
        ERROR: another reader has reserved this title
        Returned B by R1
        1 R2 READY through 2026-01-03
        2 R3 WAITING
        ERROR: no unreserved copy available
        ERROR: reader is still waiting
        Borrowed B by R2 due=2026-01-15
        1 R3 WAITING
        Returned B by R2
        1 R3 READY through 2026-01-03
        ERROR: no open loan for this book and reader
    """, status=1)


@scenario("Last valid pickup day and exact expiry boundary")
def cutoff(c):
    one_copy(c, 3)
    c.run("""
        borrow B R1
        reserve B R2
        reserve B R3
        return B R1
        advance 2026-01-03
        queue B
        advance 2026-01-04
        queue B
        borrow B R2
        advance 2026-01-06
        borrow B R3
    """, """
        Borrowed B by R1 due=2026-01-15
        Reserved B for R2 position=1
        Reserved B for R3 position=2
        Returned B by R1
        Date 2026-01-03 | expired pickups=0
        1 R2 READY through 2026-01-03
        2 R3 WAITING
        Date 2026-01-04 | expired pickups=1
        1 R3 READY through 2026-01-06
        ERROR: no unreserved copy available
        Date 2026-01-06 | expired pickups=0
        Borrowed B by R3 due=2026-01-20
    """, status=1)


@scenario("Stock changes respect pickup promises and fill waiting places")
def stock(c):
    one_copy(c)
    c.run("""
        borrow B R1
        reserve B R2
        reserve B R3
        edit-book B Revised Editor 2
        queue B
        edit-book B Revised Editor 1
        book B
        cancel-hold B R2
        queue B
        cancel-hold B R1
        return B R1
        cancel-hold B R3
        book B
    """, """
        Borrowed B by R1 due=2026-01-15
        Reserved B for R2 position=1
        Reserved B for R3 position=2
        Updated book B
        1 R2 READY through 2026-01-03
        2 R3 WAITING
        ERROR: copy count would break an active loan or pickup promise
        B | Revised | Editor | copies=2 available=0 ready=1
        Cancelled reservation B R2
        1 R3 READY through 2026-01-03
        ERROR: no reservation for this reader
        Returned B by R1
        Cancelled reservation B R3
        B | Revised | Editor | copies=2 available=2 ready=0
    """, status=1)


@scenario("Two simultaneous expiries and staggered pickups")
def simultaneous(c):
    one_copy(c)
    c.run("""
        edit-book B Title Author 2
        borrow B R1
        borrow B R2
        reserve B R3
        reserve B R4
        reserve B R5
        return B R1
        return B R2
        queue B
        advance 2026-01-04
        queue B
        book B
    """, """
        Updated book B
        Borrowed B by R1 due=2026-01-15
        Borrowed B by R2 due=2026-01-15
        Reserved B for R3 position=1
        Reserved B for R4 position=2
        Reserved B for R5 position=3
        Returned B by R1
        Returned B by R2
        1 R3 READY through 2026-01-03
        2 R4 READY through 2026-01-03
        3 R5 WAITING
        Date 2026-01-04 | expired pickups=2
        1 R5 READY through 2026-01-06
        B | Title | Author | copies=2 available=1 ready=1
    """)


@scenario("Restart after a multi-day gap gives each reader the right pickup window")
def day_gap(c):
    one_copy(c)
    c.run("""
        borrow B R1
        reserve B R2
        reserve B R3
        reserve B R4
        reserve B R5
        return B R1
        queue B
    """, """
        Borrowed B by R1 due=2026-01-15
        Reserved B for R2 position=1
        Reserved B for R3 position=2
        Reserved B for R4 position=3
        Reserved B for R5 position=4
        Returned B by R1
        1 R2 READY through 2026-01-03
        2 R3 WAITING
        3 R4 WAITING
        4 R5 WAITING
    """)
    c.run("date\nqueue B\n", "2026-01-08\n1 R4 READY through 2026-01-09\n2 R5 WAITING", day="2026-01-08")
    c.run("""
        advance 2026-01-13
        queue B
        book B
    """, """
        Date 2026-01-13 | expired pickups=2
        Queue empty.
        B | B-title | Author | copies=1 available=1 ready=0
    """, day="2026-01-08")


@scenario("Due today is current and overdue borrowing clears after return")
def overdue(c):
    books, readers = [("A", 1), ("B", 1)], [("S", "student")]
    c.run(registry(books, readers), registered(books, readers), day="2026-01-31")
    c.run("""
        borrow A S
        advance 2026-02-14
        overdue
        advance 2026-02-15
        overdue
        borrow B S
        renew A S
        return A S
        borrow B S
        history S
    """, """
        Borrowed A by S due=2026-02-14
        Date 2026-02-14 | expired pickups=0
        No overdue loans.
        Date 2026-02-15 | expired pickups=0
        Overdue on 2026-02-15
        1 day | A A-title | S S-name | due=2026-02-14
        ERROR: return overdue loans first
        ERROR: return overdue loans first
        Returned A by S
        Borrowed B by S due=2026-03-01
        A | issued=2026-01-31 due=2026-02-14 renewals=0 | RETURNED 2026-02-15
        B | issued=2026-02-15 due=2026-03-01 renewals=0 | OPEN
    """, day="2026-01-31", status=1)


@scenario("CSV priority sorting escaping and overwrite prevention")
def csv_export(c):
    c.run(r'''
        add-book Z "Chem, \"A\"" Writer 1
        add-book A "  =demo" Writer 1
        add-book C Control Writer 1
        add-reader S "Lee, \"J\"" "=example" student
        borrow Z S
        borrow A S
        advance 2026-01-02
        borrow C S
        advance 2026-01-17
        overdue
        export-overdue overdue.csv
        export-overdue overdue.csv
        export-overdue catalog.db
    ''', '''
        Added book Z
        Added book A
        Added book C
        Registered reader S
        Borrowed Z by S due=2026-01-15
        Borrowed A by S due=2026-01-15
        Date 2026-01-02 | expired pickups=0
        Borrowed C by S due=2026-01-16
        Date 2026-01-17 | expired pickups=0
        Overdue on 2026-01-17
        2 days | A   =demo | S Lee, "J" | due=2026-01-15
        2 days | Z Chem, "A" | S Lee, "J" | due=2026-01-15
        1 day | C Control | S Lee, "J" | due=2026-01-16
        Exported overdue report: overdue.csv
        ERROR: export file already exists
        ERROR: export filename must end in .csv
    ''', status=1)
    raw = (c.root / "overdue.csv").read_bytes().decode("utf-8")
    header = "days_late,book_id,title,reader_id,reader_name,contact,due_date"
    wanted = "\r\n".join([header,
        '''2,"A","'  =demo","S","Lee, ""J""","'=example",2026-01-15''',
        '''2,"Z","Chem, ""A""","S","Lee, ""J""","'=example",2026-01-15''',
        '''1,"C","Control","S","Lee, ""J""","'=example",2026-01-16''', ""])
    c.check("Exact CSV including CRLF endings", raw, wanted)
    rows = list(csv.reader(io.StringIO(raw)))
    c.check("Parsed comma and quotation marks in title", rows[2][2], 'Chem, "A"')
    c.run("date", "2026-01-17", day="2026-01-17")


@scenario("Inventory and ranked checkout statistics")
def stats(c):
    b = [(title, 2) for title in ["A", "B", "C", "D"]]
    m = [("K", "student"), ("L", "faculty"), ("V", "student")]
    c.run(registry(b, m), registered(b, m))
    c.run("""
        borrow D K
        return D K
        borrow D K
        borrow B K
        borrow A L
        borrow A V
        reserve A K
        return A L
        borrow C L
        advance 2026-01-20
        stats
    """, """
        Borrowed D by K due=2026-01-15
        Returned D by K
        Borrowed D by K due=2026-01-15
        Borrowed B by K due=2026-01-15
        Borrowed A by L due=2026-01-29
        Borrowed A by V due=2026-01-15
        Reserved A for K position=1
        Returned A by L
        Borrowed C by L due=2026-01-29
        Date 2026-01-20 | expired pickups=1
        Stock on 2026-01-20
        Titles=4 Readers=3
        Copies=8 Available=4 On loan=4 Ready=0 Waiting=0
        Overdue=3 Completed loans=2
        Top titles | A=2 | D=2 | B=1
    """)


@scenario("Quoted UTF-8 records and mixed queue survive separate processes")
def persistence(c):
    c.run(r'''
        add-book A "化學 \"入門\" \\ Lab" "A Writer" 2
        add-reader F Faculty "desk \\ west" faculty
        add-reader R1 One one student
        add-reader R2 Two two student
        add-reader R3 Three three student
        borrow A F
        renew A F
        borrow A R1
        reserve A R2
        reserve A R3
        return A R1
    ''', '''
        Added book A
        Registered reader F
        Registered reader R1
        Registered reader R2
        Registered reader R3
        Borrowed A by F due=2026-01-29
        Renewed A for F due=2026-02-26
        Borrowed A by R1 due=2026-01-15
        Reserved A for R2 position=1
        Reserved A for R3 position=2
        Returned A by R1
    ''')
    saved = c.db.read_bytes()
    c.run(r'''
        book A
        reader F
        history R1
        history F
        queue A
    ''', r'''
        A | 化學 "入門" \ Lab | A Writer | copies=2 available=0 ready=1
        F | Faculty | FACULTY | desk \ west
        open=1 overdue=0 reservations=0
        A | issued=2026-01-01 due=2026-01-15 renewals=0 | RETURNED 2026-01-01
        A | issued=2026-01-01 due=2026-02-26 renewals=1 | OPEN
        1 R2 READY through 2026-01-03
        2 R3 WAITING
    ''')
    c.check("Read-only restart preserves every database byte", c.db.read_bytes() == saved, True)
    c.run("borrow A R2\nqueue A", "Borrowed A by R2 due=2026-01-15\n1 R3 WAITING")


@scenario("Truncated snapshot is rejected and prior backup can be recovered")
def recovery(c):
    c.run("""
        add-book A Title Author 1
        edit-book A Changed Author 2
    """, "Added book A\nUpdated book A")
    backup = c.db.with_suffix(".db.bak").read_text()
    expected_backup = 'STACKDESK 1\nDATE 2026-01-01\nBOOK "A" "Title" "Author" 1\nEND\n'
    c.check("Previous good snapshot", backup, expected_backup)
    content = c.db.read_text().rsplit("END\n", 1)[0]
    c.fixture("Simulate an incomplete primary snapshot", c.db, content)
    c.run("books", status=2, err_contains="snapshot is incomplete or unreadable")
    c.check("Invalid file retained for inspection", c.db.read_text() == content, True)
    c.check("Backup retained after failed load", c.db.with_suffix(".db.bak").read_text() == backup, True)
    shutil.copyfile(c.db.with_suffix(".db.bak"), c.db)
    c.check("Copy catalog.db.bak to catalog.db", c.db.read_text(), backup)
    c.run("book A", "A | Title | Author | copies=1 available=1 ready=0")


@scenario("Missing primary with an existing backup is not initialized as empty")
def missing(c):
    c.run("add-book A Title Author 1\nedit-book A Update Author 2", "Added book A\nUpdated book A")
    c.db.rename(c.root / "removed-primary")
    c.check("Move catalog.db aside to simulate a missing primary", c.db.exists(), False)
    c.run("add-book B Some Author 1", status=2, err_contains="database missing but a backup exists")
    c.check("Primary still absent", c.db.exists(), False)


@scenario("Invalid numeric syntax malformed quotes and unknown commands")
def inputs(c):
    c.run("add-book B Title Author 1", "Added book B")
    saved = c.db.read_bytes()
    c.run(r'''
        add-book
        add-book C "bad quote Author 1
        add-book C "bad\q" Author 1
        add-book C "" Author 1
        add-book ABCDEFGHIJKLMNOPQRSTUVWXY Title Author 1
        edit-book B Title Author 999999999999999
        add-book C Title Author -1
        missing
        reader Nobody
        exit now
        book B
    ''', '''
        ERROR: add-book expects 4 argument(s)
        ERROR: unterminated quoted field
        ERROR: invalid quoted escape
        ERROR: text field must contain 1-160 bytes and a non-space character
        ERROR: ID must be 1-24 ASCII letters, digits, hyphens or underscores
        ERROR: integer out of range
        ERROR: expected a nonnegative integer
        ERROR: unknown command missing; use help
        ERROR: unknown reader Nobody
        ERROR: exit expects no arguments
        B | Title | Author | copies=1 available=1 ready=0
    ''', status=1)
    c.check("Rejected commands preserve snapshot bytes", c.db.read_bytes() == saved, True)


@scenario("Failed disk save rolls back both memory and the primary snapshot")
def save_failure(c):
    c.run("add-book B Title Author 1", "Added book B")
    saved = c.db.read_bytes()
    backup = c.db.with_suffix(".db.bak")
    backup.mkdir()
    c.check("Create a directory at catalog.db.bak to force a save failure", backup.is_dir(), True)
    c.run("""
        edit-book B CannotSave Author 3
        book B
    """, """
        ERROR: expected a regular file at <case>/catalog.db.bak
        B | Title | Author | copies=1 available=1 ready=0
    """.replace("<case>", str(c.root)), status=1)
    c.check("Primary snapshot remains byte for byte intact", c.db.read_bytes() == saved, True)
    backup.rmdir()
    c.run("edit-book B Recovered Author 2\nbook B",
          "Updated book B\nB | Recovered | Author | copies=2 available=2 ready=0")


@scenario("Database loader validates foreign keys and calendar fields")
def invalid_storage(c):
    broken = ('STACKDESK 1\nDATE 2026-01-01\nBOOK "B" "Title" "Author" 1\n'
              'LOAN "B" "UNKNOWN" STUDENT 2026-01-01 2026-01-15 0 -\nEND\n')
    c.fixture("Place a loan with a dangling reader ID in the database", c.db, broken)
    c.run("books", status=2, err_contains="loan references unknown record")
    c.check("Bytes of rejected database remain intact", c.db.read_text() == broken, True)
    broken = broken.replace("2026-01-15", "2026-02-29")
    c.fixture("Try to load an impossible calendar date", c.db, broken)
    c.run("books", status=2, err_contains="invalid calendar date")


@scenario("Leap years invalid dates and monotonic business date")
def dates(c):
    b, m = [("B", 1)], [("S", "student")]
    c.run(registry(b, m), registered(b, m), day="2028-02-20")
    c.run("""
        borrow B S
        advance 2028-02-29
        date
        advance 2100-02-29
        advance 2028-02-28
        date
    """, """
        Borrowed B by S due=2028-03-05
        Date 2028-02-29 | expired pickups=0
        2028-02-29
        ERROR: invalid calendar date (years 1900-9999)
        ERROR: business date cannot move backwards
        2028-02-29
    """, status=1, day="2028-02-20")
    c.run("date", day="2028-02-28", status=2, err_contains="earlier than the saved business date")
    c.run("date", "2028-02-29", day="2028-02-29")


@scenario("Interactive menu submission cancellation and EOF")
def menu(c):
    c.run("""
        bad
        1
        1
        :back
        1
        A
        The Study Book
        Ana Reed
        2
        2
        A
    """, interactive=True, contains=[
        "StackDesk  |  2026-01-01", "Choose a number from 0 to 5.", "Book catalog",
        "Type :back to cancel this action.", "Book ID:", "Book catalog", "Added book A",
        "A | The Study Book | Ana Reed | copies=2 available=2 ready=0", "Desk closed."
    ])
    c.run("books", "A | The Study Book | Ana Reed | copies=2 available=2 ready=0")


@scenario("Existing session prevents a simultaneous writer")
def lock(c):
    argv = [str(BINARY), "--db", "catalog.db", "--date", START, "--batch"]
    process = subprocess.Popen(argv, cwd=c.root, stdin=subprocess.PIPE,
                               stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    try:
        lock_dir = c.root / "catalog.db.lock"
        deadline = time.monotonic() + 3
        while not lock_dir.exists() and process.poll() is None and time.monotonic() < deadline:
            time.sleep(0.01)
        c.check("First batch process holds the session lock with stdin open", lock_dir.is_dir(), True)
        c.run("add-book B Title Author 1", status=2, err_contains="path is locked")
    finally:
        process.communicate(b"exit\n", timeout=5)
    c.check("Lock directory released when the first session exits", lock_dir.exists(), False)
    c.run("add-book B Title Author 1", "Added book B")


@scenario("Validate database extension and empty CSV export")
def separate_paths(c):
    c.run("export-overdue catalog.csv", "Exported overdue report: catalog.csv")
    c.check("Empty export still has a CSV header",
            (c.root / "catalog.csv").read_bytes(),
            b"days_late,book_id,title,reader_id,reader_name,contact,due_date\r\n")
    c.run("stats", status=2, err_contains="database filename must end in .db",
          options=("--db", "catalog.csv"))
    c.check("Separate export never creates a database", c.db.exists(), False)


def evidence(path, all_runs):
    sections = [
        "# System Test Transcripts",
        "",
        "Each scenario ran in a fresh temporary directory with test records. Commands in one scenario share its database when shown as separate executions.",
        "The printed command uses ./main for portability. The path marker <case> identifies the temporary run directory.",
    ]
    for index, (title, entries) in enumerate(all_runs, 1):
        sections += ["", f"## T{index:02} {title}", "", "**PASS**"]
        for entry in entries:
            if len(entry) == 3:
                label, actual, expected = entry
                sections += ["", label, "", "~~~text", f"Expected: {expected!r}",
                             f"Actual:   {actual!r}", "~~~"]
            else:
                cmd, data, want, stdout, expected_exit, actual_exit, err_contains, stderr = entry
                sections += ["", f"Command: {cmd}", "",
                             f"Expected exit: {expected_exit}. Actual exit: {actual_exit}.",
                             "", "~~~text", "Standard input", data.rstrip() or "(empty)",
                             "", "Expected stdout", want.rstrip() or "(empty)",
                             "", "Actual stdout", stdout.rstrip() or "(empty)",
                             "", "Expected stderr", ("Contains " + err_contains) if err_contains else "(empty)",
                             "", "Actual stderr", stderr.rstrip() or "(empty)", "~~~"]
    path.write_text("\n".join(sections)+"\n", encoding="utf-8")


def main():
    runs = []
    for index, (title, test) in enumerate(CASES, 1):
        with TemporaryDirectory(prefix="stackdesk-test-") as folder:
            case = Case(Path(folder))
            try:
                test(case)
            except Exception:
                print(f"FAIL T{index:02} {title}", file=sys.stderr)
                raise
            print(f"PASS T{index:02} {title}")
            runs.append((title, case.evidence))
    print(f"{len(runs)}/{len(CASES)} scenarios passed.")
    if len(sys.argv) == 4 and sys.argv[2] == "--evidence":
        evidence(Path(sys.argv[3]), runs)


if __name__ == "__main__":
    main()
