# StackDesk System Testing Report

The implemented system passed **24 of 24** automated scenarios on Linux with GCC 14.2.0 and Python 3.12.14. The Makefile build and the direct `g++ -std=c++17 -pedantic-errors` build passed the same suite. A separate build with AddressSanitizer and UndefinedBehaviorSanitizer passed all 24 scenarios. Leak detection was disabled for that run.

## Method

The test runner launches the compiled executable in a fresh temporary directory for each scenario. Commands drive the same dispatcher used by the numbered menus. Batch tests compare standard output exactly and check the exit status. Tests expecting startup failures check the diagnostic on standard error too. The menu test uses ordered output checks so a prompt layout change cannot hide lost navigation or missing records.

The suite controls time through `--date` and `advance`. For date boundaries, the expected pickup and loan dates are calculated before running the program. For persistence, the test suite closes one process, starts a second with the same database file, and checks both displayed records and file contents. A hand-edited input file exercises loader validation. A directory in place of the backup file produces a real save error and lets the test verify that no draft reaches live state.

Run the checks and regenerate the detailed evidence from the repository root.

~~~bash
make test
python3 tests/system_tests.py ./main --evidence tests/TRANSCRIPTS.md
make demo
~~~

The [full transcripts](tests/TRANSCRIPTS.md) list exact command inputs, expected standard output, actual output, exit statuses and relevant disk checks for every case. All example contacts are fictional.

## Scenario results

| ID | Scenario | Expected behaviour checked | Result |
| --- | --- | --- | --- |
| T01 | Empty desk | Reports show zero totals and viewing does not create a database. | Pass |
| T02 | Book maintenance | Search ignores ASCII case, stock edits apply, invalid IDs and duplicate keys fail. | Pass |
| T03 | Reader maintenance | Profile edits persist and duplicate IDs and unknown tiers fail. | Pass |
| T04 | Policy selection | Student and faculty dates differ. Old loans keep their policy after a tier change. | Pass |
| T05 | Open loan limits | The fourth student loan and sixth faculty loan are blocked. Returning an item frees a slot. | Pass |
| T06 | Renewal limits | Student and faculty stop at their configured caps with the correct dates. | Pass |
| T07 | FIFO and pickup protection | A ready reader collects first. Walk-ins and waiters cannot take a held copy. | Pass |
| T08 | Pickup boundary | An offer remains valid through day three. On day four it goes to the next reader. | Pass |
| T09 | Stock and queue cancellation | Stock below protected copies is rejected. New stock and cancellations offer copies in order. | Pass |
| T10 | Simultaneous pickup expiries | Two offers expire together and one waiting reader receives a new window. | Pass |
| T11 | Time gap and restart | Multiple successive windows expire on their proper days even after a gap. | Pass |
| T12 | Overdue boundary | A loan due today is current. A late loan blocks borrowing or renewal until returned. | Pass |
| T13 | CSV and ordering | Oldest due dates sort first. Ties, commas, quotes and no-overwrite behaviour match expectations. | Pass |
| T14 | Stock and popularity | Counts reconcile with loans and returns. Equal checkout totals sort by ID. | Pass |
| T15 | Persistent mixed records | Quotes, backslashes, UTF-8, loan dates and a mixed queue survive a restart. | Pass |
| T16 | Incomplete database | A truncated primary is retained for diagnosis and its prior backup loads after restoration. | Pass |
| T17 | Missing primary | The program refuses to initialise an empty database while a backup exists. | Pass |
| T18 | Invalid commands | Malformed quoting, negative or overflowing numbers, oversized IDs and unknown commands are rejected. | Pass |
| T19 | Save rollback | A failed write preserves both the on-disk primary and the record displayed in the current process. | Pass |
| T20 | Invalid stored records | Unknown reader references and an impossible date block startup. | Pass |
| T21 | Calendar edges | Leap-year due dates work and a move backwards is rejected. | Pass |
| T22 | Numbered menu | Invalid selection, form cancellation, normal catalogue use and EOF work. | Pass |
| T23 | Concurrent sessions | A second process cannot open the same database until the first exits. | Pass |
| T24 | Empty export and file type | The CSV still has its header. A CSV filename is rejected as a database. | Pass |

## Boundary and persistence findings

Pickup expiry is exclusive. A copy offered on January 1 can be collected through January 3. Starting the desk on January 8 still grants the next offers on their original expiry dates. T08 and T11 detect an incorrect implementation that postpones a pickup window to the day the desk reopens. T12 and T21 verify month turnover, leap days and the difference between a due date and an overdue day.

T15 checks a real close-and-reopen cycle. T16 and T17 cover refusal to start with a damaged or missing primary. T19 exercises a disk write failure and compares the database bytes before and after. T23 starts two actual processes. Together these tests cover the expected single-workstation workflow. Abrupt power loss and the department's CS Academy server remain separate verification environments.

## Browser workspace verification

The terminal regression suite still passed **24 of 24** scenarios. The English browser workspace passed **8 of 8** API integration scenarios using the real C++ binary, loopback HTTP, and separate temporary databases. Run `make web-test` to reproduce these checks.

| ID | Input and expected result | Actual |
| --- | --- | --- |
| W01 | Request the English library page and JavaScript assets. The files and same-origin policy should be served. | Pass |
| W02 | Load the fictional demo. The snapshot should contain seven titles, six readers, the two policies, and the waiting queue. | Pass |
| W03 | Search `bk101`, `jun structures`, `pratical C++` and `pracitcal`. The corresponding titles should be ranked and the misspellings marked. An unrelated query should return no matches. | Pass |
| W04 | Attempt checkout as a waiting reader, then return the book as its borrower. The first request should fail and preserve disk bytes, and Taylor should receive a pickup through 23 November. | Pass |
| W05 | Advance to 24 November, reload and check out as Jamie. Taylor's hold should expire, Jamie's should be ready through 26 November, and the new loan should be due on 8 December. | Pass |
| W06 | Download the overdue report. The report should contain Database Design, Morgan Lam's ID, and nine days late on 24 November. | Pass |
| W07 | Send requests with a different origin or host, invalid fields, and an unsupported action. They should be rejected and no draft should reach disk. | Pass |
| W08 | Add and edit records through the browser API, change a reader tier, and restart against the same database. The existing loan should keep its original tier and both entry points should read the same records. | Pass |

A browser walkthrough also verified the visible close-match search result, the return dialog, both pickup deadlines, and the checkout that cleared the queue. The original terminal build remains the CS Academy submission entry point.
