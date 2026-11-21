# StackDesk Library Management System

StackDesk is a C++ application for a small campus lending desk. It offers a numbered terminal menu and an English browser workspace. Staff can register readers, manage the book catalogue, issue and return copies, and run a fair pickup queue. The desk tracks copy availability and overdue loans across restarts.

## Run on CS Academy

From the repository directory, run:

~~~bash
make
./main
~~~

The Makefile builds with `g++ -std=c++17 -pedantic-errors`. The application uses the C++ standard library and requires no external packages. To build without Make, run `g++ -std=c++17 -pedantic-errors *.cpp -o main`. A successful change is saved automatically. The default database file is `stackdesk.db` in the current working directory.

For a repeatable tour that runs in a temporary directory, use `make demo`. Run `make test` with Python 3.8 or later to verify the compiled program. The automated tests and demonstration use fictional reader records.

## Run the English browser workspace

Run from the repository root on a machine with a C++17 toolchain and Python 3.8 or later.

~~~bash
make web-demo
~~~

Open the loopback address printed in the terminal, normally http://127.0.0.1:8765/. The demo starts at 21 November 2026 with fictional records in a temporary database. Return **Practical C++** in Circulation, then check Reservations. Taylor has through 23 November to collect it. Use **Advance date** to move three days forward. The reservation moves to Jamie, who can then collect it. Closing and restarting the demo starts fresh.

For everyday use, run `make web`. The browser and the numbered menu both use `stackdesk.db` by default. To open a specific database or another local port, build with `make` and run `python3 web/server.py --db example.db --port 8766`. The server listens only on the local machine. Stop it with Ctrl+C. The coursework build on CS Academy uses `make` and `./main`, with no Python requirement for the terminal program.

The workspace has an overview, editable catalog and reader records, circulation, reservation queues and reports with CSV download. **Smart Search** ranks title, author and ID matches in C++. It accepts multiple keywords, prefixes and one spelling edit or transposition in eligible words. For example, `pratical c++` finds *Practical C++*. Search runs entirely offline and can be filtered to currently available copies.

The browser sends actions to a small Python standard library server in `web/server.py`. It dispatches them to the same compiled C++ program, which validates actions and saves the database. C++ also supplies the JSON snapshot and ranked search results from `web_view.cpp`. Run `make web-test` for the browser API scenarios.

## Desk workflow

| Menu | What staff can do |
| --- | --- |
| Book catalog | Add a title, find by ID or keyword, list books, and edit the title, author or copy count. |
| Reader registry | Register readers, find by ID or name, edit profiles and see active loans, overdue loans and reservations. |
| Loans and returns | Check out a copy, collect a reservation, return or renew a loan, and see a reader's history. |
| Reservations | Join, view or cancel a title's waiting list. Returned and newly added copies go to the earliest waiting readers. |
| Reports | See stock, overdue counts and the three most borrowed titles. Open the overdue list or export it to a new CSV file. |

IDs are case sensitive and may contain 1–24 ASCII letters, digits, hyphens or underscores. A title must have between 1 and 9,999 copies. Text fields accept up to 160 bytes, including UTF-8 text, and search ignores ASCII letter case. In menu forms, `:back` cancels the action. Every form uses the same business rules as batch mode.

## Three features beyond basic lending

1. **Reader tiers.** A student can borrow three books for 14 days each with one renewal. Faculty can borrow five books for 28 days each with up to two renewals. Renewals extend the existing due date. Existing loans retain the tier assigned at checkout. A reader with an overdue loan must return it before borrowing or renewing. A reservation by another reader blocks renewal of that title.
2. **Pickup windows and automatic rollover.** The earliest waiting reader receives an offer when a copy becomes free. The offer is valid on the offer day and the next two calendar days. On the fourth day, the copy goes to the next reader. If the desk reopens after several days, it processes each missed expiry in order. The catalog reports separately on available copies and copies set aside for pickup.
3. **Prioritised overdue export.** Staff can see the oldest overdue loans first, then export those same rows with reader contact details to a new `.csv` file. The report lists a loan as overdue the day after its due date.

## Deterministic batch mode

~~~bash
./main --db sample.db --date 2026-11-21 --batch <<'EOF'
add-book B1 "Introduction to C++" "Ana Reed" 1
add-reader S1 "Alex Chan" alex@example.invalid student
add-reader F1 "Taylor Lee" taylor@example.invalid faculty
borrow B1 S1
reserve B1 F1
return B1 S1
queue B1
EOF

./main --db sample.db --date 2026-11-21 --batch <<'EOF'
queue B1
borrow B1 F1
history F1
stats
EOF
~~~

This example starts with a new `sample.db`. Use a fresh filename to repeat it. Batch mode reads one command per line. `help` lists all commands, `exit` exits, and blank lines or lines beginning with `#` are ignored. Double-quote a field with spaces and use `\"` or `\\` inside quoted fields. Exit status is 0 on success, 1 if a batch command is rejected, and 2 if startup or input fails. Batch errors print a message and processing continues with subsequent commands.

`--date` fixes the business day for a test. In this mode, `advance YYYY-MM-DD` moves time forward. Use a separate database for simulations. In ordinary mode, the business date follows the local calendar and respects the last date saved in the database.

## How the code meets the technical requirements

| Requirement | Implementation |
| --- | --- |
| Self-learned C++ feature | `LoanPolicy` is an abstract class. `StudentPolicy` and `FacultyPolicy` override lending rules, and circulation calls them at run time through a base reference. See `policy.h`, `policy.cpp` and `circulation.cpp`. |
| Dynamic allocation and deallocation | Two `std::unique_ptr<LoanPolicy>` members own heap-allocated policy objects. A virtual destructor allows deletion through the base type. Ownership and deallocation follow RAII. |
| Structured data | Maps index books and readers by stable ID. A vector keeps loan history. Each book's deque preserves FIFO reservation order and supports cancellation and pickup. |
| Persistent file I/O | File streams read and write a versioned snapshot of catalog, readers, loans, queue entries and date. The program validates loaded records and keeps the previous successful snapshot as a backup. |
| Multiple implementation files | `main.cpp` handles startup and UI. Public interfaces in `desk.h`, `model.h`, `storage.h`, `policy.h` and `date.h` separate declarations from implementations. |
| Modular functions and operation modes | Catalog, readers, circulation, reservations, reports and storage are separate modules. Menus and the batch runner share the same dispatcher. |
| External header-only libraries | None. |

The storage module stages a complete validated snapshot before replacing the database. A second desk process using the same database must wait until the first exits. If a primary snapshot fails validation, save a copy for inspection, verify the prior snapshot in `FILE.db.bak`, and copy that backup to `FILE.db` while the desk is closed. The backup can be one successful change behind. After a forced process termination, confirm that no desk process is using the database before removing a leftover `FILE.db.lock` directory. Snapshots and CSV exports contain the reader contact information entered by staff. Keep them in the desk's working directory with appropriate access.

## Team roster and work areas

Complete the names and student IDs from the registered five-person team. The work areas below follow the group's proposed split. Record actual owners and contributions after each member reviews their module.

| Team member | Student ID |
| --- | --- |
| Kevin | To fill |
| William Shi | To fill |
| Eunice | To fill |
| Team member 4 | Name and ID to fill |
| Team member 5 | Name and ID to fill |

| Role | Module and integration responsibility |
| --- | --- |
| A | Catalog add, search and edit in `catalog.cpp`. Verify that stock changes honour loans and pickups. |
| B | Reader registration, query and editing in `readers.cpp`. Verify profile edits and borrowing policy changes. |
| C | Borrowing, renewals, returns and history in `circulation.cpp` with the policies in `policy.cpp`. |
| D | FIFO reservations and pickup expiry in `reservations.cpp`. Stock and overdue reports in `reports.cpp`. |
| E | Versioned persistence in `storage.cpp` and integration in `main.cpp` and `Makefile`. Verify execution on CS Academy. |

## Project evidence

- [Learning materials](LEARNING_MATERIALS.md)
- [Testing report](TESTING_REPORT.md) and [expected versus actual system outputs](tests/TRANSCRIPTS.md)
- [AI usage declaration and log](AI_USAGE.md)
- [Demonstration outline](demo/VIDEO_GUIDE.md). The course requires an accessible hosted video URL here and in Moodle.

Moodle takes the group's private GitHub repository link and the online video link. The project specification names `engg1340comp2113` as the teaching team account that needs access to the private repository.
