# System Test Transcripts

Each scenario ran in a fresh temporary directory with test records. Commands in one scenario share its database when shown as separate executions.
The printed command uses ./main for portability. The path marker <case> identifies the temporary run directory. Trailing prompt whitespace is trimmed for display.

## T01 Empty registry and read-only start

**PASS**

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
# Empty operations on a new database.
books

readers
overdue
stats
exit
unknown-after-exit

Expected stdout
No matching books.
No matching readers.
No overdue loans.
Stock on 2026-01-01
Titles=0 Readers=0
Copies=0 Available=0 On loan=0 Ready=0 Waiting=0
Overdue=0 Completed loans=0
Top titles: none

Actual stdout
No matching books.
No matching readers.
No overdue loans.
Stock on 2026-01-01
Titles=0 Readers=0
Copies=0 Available=0 On loan=0 Ready=0 Waiting=0
Overdue=0 Completed loans=0
Top titles: none

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Database exists after read-only session

~~~text
Expected: False
Actual:   False
~~~

## T02 Catalog search edit and ID validation

**PASS**

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 1. Actual exit: 1.

~~~text
Standard input
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

Expected stdout
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

Actual stdout
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

Expected stderr
(empty)

Actual stderr
(empty)
~~~

## T03 Reader registration contact edits and validation

**PASS**

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 1. Actual exit: 1.

~~~text
Standard input
add-reader S1 "Jo Lin" jo@example.invalid student
add-reader S1 Other other@example.invalid faculty
add-reader A1 Demo no-contact visitor
find-reader "jo l"
edit-reader S1 "Jo Lin" new@example.invalid FACULTY
reader S1
find-reader nobody

Expected stdout
Registered reader S1
ERROR: reader ID already exists
ERROR: tier must be STUDENT or FACULTY
S1 | Jo Lin | STUDENT | jo@example.invalid
Updated reader S1
S1 | Jo Lin | FACULTY | new@example.invalid
open=0 overdue=0 reservations=0
No matching readers.

Actual stdout
Registered reader S1
ERROR: reader ID already exists
ERROR: tier must be STUDENT or FACULTY
S1 | Jo Lin | STUDENT | jo@example.invalid
Updated reader S1
S1 | Jo Lin | FACULTY | new@example.invalid
open=0 overdue=0 reservations=0
No matching readers.

Expected stderr
(empty)

Actual stderr
(empty)
~~~

## T04 Student and faculty dynamic dispatch after a tier change

**PASS**

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
add-book A A-title Author 2
add-book B B-title Author 1
add-reader S S-name S@example.invalid student
add-reader F F-name F@example.invalid faculty

Expected stdout
Added book A
Added book B
Registered reader S
Registered reader F

Actual stdout
Added book A
Added book B
Registered reader S
Registered reader F

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
policy student
policy faculty
borrow A S
borrow A F
edit-reader S Student contact faculty
renew A S
borrow B S
history S

Expected stdout
STUDENT | days=14 limit=3 renewals=1
FACULTY | days=28 limit=5 renewals=2
Borrowed A by S due=2026-01-15
Borrowed A by F due=2026-01-29
Updated reader S
Renewed A for S due=2026-01-29
Borrowed B by S due=2026-01-29
A | issued=2026-01-01 due=2026-01-29 renewals=1 | OPEN
B | issued=2026-01-01 due=2026-01-29 renewals=0 | OPEN

Actual stdout
STUDENT | days=14 limit=3 renewals=1
FACULTY | days=28 limit=5 renewals=2
Borrowed A by S due=2026-01-15
Borrowed A by F due=2026-01-29
Updated reader S
Renewed A for S due=2026-01-29
Borrowed B by S due=2026-01-29
A | issued=2026-01-01 due=2026-01-29 renewals=1 | OPEN
B | issued=2026-01-01 due=2026-01-29 renewals=0 | OPEN

Expected stderr
(empty)

Actual stderr
(empty)
~~~

## T05 Loan limits for both tiers and duplicate borrowing

**PASS**

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
add-book B1 B1-title Author 2
add-book B2 B2-title Author 2
add-book B3 B3-title Author 2
add-book B4 B4-title Author 2
add-book B5 B5-title Author 2
add-book B6 B6-title Author 2
add-reader S S-name S@example.invalid student
add-reader F F-name F@example.invalid faculty

Expected stdout
Added book B1
Added book B2
Added book B3
Added book B4
Added book B5
Added book B6
Registered reader S
Registered reader F

Actual stdout
Added book B1
Added book B2
Added book B3
Added book B4
Added book B5
Added book B6
Registered reader S
Registered reader F

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 1. Actual exit: 1.

~~~text
Standard input
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

Expected stdout
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

Actual stdout
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

Expected stderr
(empty)

Actual stderr
(empty)
~~~

## T06 Renewal counters and due date extensions

**PASS**

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
add-book B B-title Author 2
add-reader S S-name S@example.invalid student
add-reader F F-name F@example.invalid faculty

Expected stdout
Added book B
Registered reader S
Registered reader F

Actual stdout
Added book B
Registered reader S
Registered reader F

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 1. Actual exit: 1.

~~~text
Standard input
renew B S
borrow B S
renew B S
renew B S
borrow B F
renew B F
renew B F
renew B F

Expected stdout
ERROR: no open loan for this book and reader
Borrowed B by S due=2026-01-15
Renewed B for S due=2026-01-29
ERROR: renewal limit reached
Borrowed B by F due=2026-01-29
Renewed B for F due=2026-02-26
Renewed B for F due=2026-03-26
ERROR: renewal limit reached

Actual stdout
ERROR: no open loan for this book and reader
Borrowed B by S due=2026-01-15
Renewed B for S due=2026-01-29
ERROR: renewal limit reached
Borrowed B by F due=2026-01-29
Renewed B for F due=2026-02-26
Renewed B for F due=2026-03-26
ERROR: renewal limit reached

Expected stderr
(empty)

Actual stderr
(empty)
~~~

## T07 FIFO pickup and walk-in exclusion

**PASS**

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
add-book B B-title Author 1
add-reader R1 R1-name R1@example.invalid student
add-reader R2 R2-name R2@example.invalid student
add-reader R3 R3-name R3@example.invalid student
add-reader R4 R4-name R4@example.invalid student
add-reader R5 R5-name R5@example.invalid student

Expected stdout
Added book B
Registered reader R1
Registered reader R2
Registered reader R3
Registered reader R4
Registered reader R5

Actual stdout
Added book B
Registered reader R1
Registered reader R2
Registered reader R3
Registered reader R4
Registered reader R5

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 1. Actual exit: 1.

~~~text
Standard input
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

Expected stdout
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

Actual stdout
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

Expected stderr
(empty)

Actual stderr
(empty)
~~~

## T08 Last valid pickup day and exact expiry boundary

**PASS**

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
add-book B B-title Author 1
add-reader R1 R1-name R1@example.invalid student
add-reader R2 R2-name R2@example.invalid student
add-reader R3 R3-name R3@example.invalid student

Expected stdout
Added book B
Registered reader R1
Registered reader R2
Registered reader R3

Actual stdout
Added book B
Registered reader R1
Registered reader R2
Registered reader R3

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 1. Actual exit: 1.

~~~text
Standard input
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

Expected stdout
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

Actual stdout
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

Expected stderr
(empty)

Actual stderr
(empty)
~~~

## T09 Stock changes respect pickup promises and fill waiting places

**PASS**

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
add-book B B-title Author 1
add-reader R1 R1-name R1@example.invalid student
add-reader R2 R2-name R2@example.invalid student
add-reader R3 R3-name R3@example.invalid student
add-reader R4 R4-name R4@example.invalid student
add-reader R5 R5-name R5@example.invalid student

Expected stdout
Added book B
Registered reader R1
Registered reader R2
Registered reader R3
Registered reader R4
Registered reader R5

Actual stdout
Added book B
Registered reader R1
Registered reader R2
Registered reader R3
Registered reader R4
Registered reader R5

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 1. Actual exit: 1.

~~~text
Standard input
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

Expected stdout
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

Actual stdout
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

Expected stderr
(empty)

Actual stderr
(empty)
~~~

## T10 Two simultaneous expiries and staggered pickups

**PASS**

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
add-book B B-title Author 1
add-reader R1 R1-name R1@example.invalid student
add-reader R2 R2-name R2@example.invalid student
add-reader R3 R3-name R3@example.invalid student
add-reader R4 R4-name R4@example.invalid student
add-reader R5 R5-name R5@example.invalid student

Expected stdout
Added book B
Registered reader R1
Registered reader R2
Registered reader R3
Registered reader R4
Registered reader R5

Actual stdout
Added book B
Registered reader R1
Registered reader R2
Registered reader R3
Registered reader R4
Registered reader R5

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
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

Expected stdout
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

Actual stdout
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

Expected stderr
(empty)

Actual stderr
(empty)
~~~

## T11 Restart after a multi-day gap gives each reader the right pickup window

**PASS**

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
add-book B B-title Author 1
add-reader R1 R1-name R1@example.invalid student
add-reader R2 R2-name R2@example.invalid student
add-reader R3 R3-name R3@example.invalid student
add-reader R4 R4-name R4@example.invalid student
add-reader R5 R5-name R5@example.invalid student

Expected stdout
Added book B
Registered reader R1
Registered reader R2
Registered reader R3
Registered reader R4
Registered reader R5

Actual stdout
Added book B
Registered reader R1
Registered reader R2
Registered reader R3
Registered reader R4
Registered reader R5

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
borrow B R1
reserve B R2
reserve B R3
reserve B R4
reserve B R5
return B R1
queue B

Expected stdout
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

Actual stdout
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

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Command: ./main --db catalog.db --date 2026-01-08 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
date
queue B

Expected stdout
2026-01-08
1 R4 READY through 2026-01-09
2 R5 WAITING

Actual stdout
2026-01-08
1 R4 READY through 2026-01-09
2 R5 WAITING

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Command: ./main --db catalog.db --date 2026-01-08 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
advance 2026-01-13
queue B
book B

Expected stdout
Date 2026-01-13 | expired pickups=2
Queue empty.
B | B-title | Author | copies=1 available=1 ready=0

Actual stdout
Date 2026-01-13 | expired pickups=2
Queue empty.
B | B-title | Author | copies=1 available=1 ready=0

Expected stderr
(empty)

Actual stderr
(empty)
~~~

## T12 Due today is current and overdue borrowing clears after return

**PASS**

Command: ./main --db catalog.db --date 2026-01-31 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
add-book A A-title Author 1
add-book B B-title Author 1
add-reader S S-name S@example.invalid student

Expected stdout
Added book A
Added book B
Registered reader S

Actual stdout
Added book A
Added book B
Registered reader S

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Command: ./main --db catalog.db --date 2026-01-31 --batch

Expected exit: 1. Actual exit: 1.

~~~text
Standard input
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

Expected stdout
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

Actual stdout
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

Expected stderr
(empty)

Actual stderr
(empty)
~~~

## T13 CSV priority sorting escaping and overwrite prevention

**PASS**

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 1. Actual exit: 1.

~~~text
Standard input
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

Expected stdout
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

Actual stdout
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

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Exact CSV including CRLF endings

~~~text
Expected: 'days_late,book_id,title,reader_id,reader_name,contact,due_date\r\n2,"A","\'  =demo","S","Lee, ""J""","\'=example",2026-01-15\r\n2,"Z","Chem, ""A""","S","Lee, ""J""","\'=example",2026-01-15\r\n1,"C","Control","S","Lee, ""J""","\'=example",2026-01-16\r\n'
Actual:   'days_late,book_id,title,reader_id,reader_name,contact,due_date\r\n2,"A","\'  =demo","S","Lee, ""J""","\'=example",2026-01-15\r\n2,"Z","Chem, ""A""","S","Lee, ""J""","\'=example",2026-01-15\r\n1,"C","Control","S","Lee, ""J""","\'=example",2026-01-16\r\n'
~~~

Parsed comma and quotation marks in title

~~~text
Expected: 'Chem, "A"'
Actual:   'Chem, "A"'
~~~

Command: ./main --db catalog.db --date 2026-01-17 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
date

Expected stdout
2026-01-17

Actual stdout
2026-01-17

Expected stderr
(empty)

Actual stderr
(empty)
~~~

## T14 Inventory and ranked checkout statistics

**PASS**

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
add-book A A-title Author 2
add-book B B-title Author 2
add-book C C-title Author 2
add-book D D-title Author 2
add-reader K K-name K@example.invalid student
add-reader L L-name L@example.invalid faculty
add-reader V V-name V@example.invalid student

Expected stdout
Added book A
Added book B
Added book C
Added book D
Registered reader K
Registered reader L
Registered reader V

Actual stdout
Added book A
Added book B
Added book C
Added book D
Registered reader K
Registered reader L
Registered reader V

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
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

Expected stdout
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

Actual stdout
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

Expected stderr
(empty)

Actual stderr
(empty)
~~~

## T15 Quoted UTF-8 records and mixed queue survive separate processes

**PASS**

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
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

Expected stdout
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

Actual stdout
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

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
book A
reader F
history R1
history F
queue A

Expected stdout
A | 化學 "入門" \ Lab | A Writer | copies=2 available=0 ready=1
F | Faculty | FACULTY | desk \ west
open=1 overdue=0 reservations=0
A | issued=2026-01-01 due=2026-01-15 renewals=0 | RETURNED 2026-01-01
A | issued=2026-01-01 due=2026-02-26 renewals=1 | OPEN
1 R2 READY through 2026-01-03
2 R3 WAITING

Actual stdout
A | 化學 "入門" \ Lab | A Writer | copies=2 available=0 ready=1
F | Faculty | FACULTY | desk \ west
open=1 overdue=0 reservations=0
A | issued=2026-01-01 due=2026-01-15 renewals=0 | RETURNED 2026-01-01
A | issued=2026-01-01 due=2026-02-26 renewals=1 | OPEN
1 R2 READY through 2026-01-03
2 R3 WAITING

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Read-only restart preserves every database byte

~~~text
Expected: True
Actual:   True
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
borrow A R2
queue A

Expected stdout
Borrowed A by R2 due=2026-01-15
1 R3 WAITING

Actual stdout
Borrowed A by R2 due=2026-01-15
1 R3 WAITING

Expected stderr
(empty)

Actual stderr
(empty)
~~~

## T16 Truncated snapshot is rejected and prior backup can be recovered

**PASS**

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
add-book A Title Author 1
edit-book A Changed Author 2

Expected stdout
Added book A
Updated book A

Actual stdout
Added book A
Updated book A

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Previous good snapshot

~~~text
Expected: 'STACKDESK 1\nDATE 2026-01-01\nBOOK "A" "Title" "Author" 1\nEND\n'
Actual:   'STACKDESK 1\nDATE 2026-01-01\nBOOK "A" "Title" "Author" 1\nEND\n'
~~~

Simulate an incomplete primary snapshot

~~~text
Expected: 'STACKDESK 1\nDATE 2026-01-01\nBOOK "A" "Changed" "Author" 2\n'
Actual:   'catalog.db'
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 2. Actual exit: 2.

~~~text
Standard input
books

Expected stdout
(empty)

Actual stdout
(empty)

Expected stderr
Contains snapshot is incomplete or unreadable

Actual stderr
FATAL: database <case>/catalog.db line 3: snapshot is incomplete or unreadable
~~~

Invalid file retained for inspection

~~~text
Expected: True
Actual:   True
~~~

Backup retained after failed load

~~~text
Expected: True
Actual:   True
~~~

Copy catalog.db.bak to catalog.db

~~~text
Expected: 'STACKDESK 1\nDATE 2026-01-01\nBOOK "A" "Title" "Author" 1\nEND\n'
Actual:   'STACKDESK 1\nDATE 2026-01-01\nBOOK "A" "Title" "Author" 1\nEND\n'
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
book A

Expected stdout
A | Title | Author | copies=1 available=1 ready=0

Actual stdout
A | Title | Author | copies=1 available=1 ready=0

Expected stderr
(empty)

Actual stderr
(empty)
~~~

## T17 Missing primary with an existing backup is not initialized as empty

**PASS**

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
add-book A Title Author 1
edit-book A Update Author 2

Expected stdout
Added book A
Updated book A

Actual stdout
Added book A
Updated book A

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Move catalog.db aside to simulate a missing primary

~~~text
Expected: False
Actual:   False
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 2. Actual exit: 2.

~~~text
Standard input
add-book B Some Author 1

Expected stdout
(empty)

Actual stdout
(empty)

Expected stderr
Contains database missing but a backup exists

Actual stderr
FATAL: database missing but a backup exists; restore it before continuing
~~~

Primary still absent

~~~text
Expected: False
Actual:   False
~~~

## T18 Invalid numeric syntax malformed quotes and unknown commands

**PASS**

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
add-book B Title Author 1

Expected stdout
Added book B

Actual stdout
Added book B

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 1. Actual exit: 1.

~~~text
Standard input
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

Expected stdout
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

Actual stdout
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

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Rejected commands preserve snapshot bytes

~~~text
Expected: True
Actual:   True
~~~

## T19 Failed disk save rolls back both memory and the primary snapshot

**PASS**

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
add-book B Title Author 1

Expected stdout
Added book B

Actual stdout
Added book B

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Create a directory at catalog.db.bak to force a save failure

~~~text
Expected: True
Actual:   True
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 1. Actual exit: 1.

~~~text
Standard input
edit-book B CannotSave Author 3
book B

Expected stdout
ERROR: expected a regular file at <case>/catalog.db.bak
B | Title | Author | copies=1 available=1 ready=0

Actual stdout
ERROR: expected a regular file at <case>/catalog.db.bak
B | Title | Author | copies=1 available=1 ready=0

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Primary snapshot remains byte for byte intact

~~~text
Expected: True
Actual:   True
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
edit-book B Recovered Author 2
book B

Expected stdout
Updated book B
B | Recovered | Author | copies=2 available=2 ready=0

Actual stdout
Updated book B
B | Recovered | Author | copies=2 available=2 ready=0

Expected stderr
(empty)

Actual stderr
(empty)
~~~

## T20 Database loader validates foreign keys and calendar fields

**PASS**

Place a loan with a dangling reader ID in the database

~~~text
Expected: 'STACKDESK 1\nDATE 2026-01-01\nBOOK "B" "Title" "Author" 1\nLOAN "B" "UNKNOWN" STUDENT 2026-01-01 2026-01-15 0 -\nEND\n'
Actual:   'catalog.db'
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 2. Actual exit: 2.

~~~text
Standard input
books

Expected stdout
(empty)

Actual stdout
(empty)

Expected stderr
Contains loan references unknown record

Actual stderr
FATAL: database <case>/catalog.db line 5: loan references unknown record
~~~

Bytes of rejected database remain intact

~~~text
Expected: True
Actual:   True
~~~

Try to load an impossible calendar date

~~~text
Expected: 'STACKDESK 1\nDATE 2026-01-01\nBOOK "B" "Title" "Author" 1\nLOAN "B" "UNKNOWN" STUDENT 2026-01-01 2026-02-29 0 -\nEND\n'
Actual:   'catalog.db'
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 2. Actual exit: 2.

~~~text
Standard input
books

Expected stdout
(empty)

Actual stdout
(empty)

Expected stderr
Contains invalid calendar date

Actual stderr
FATAL: database <case>/catalog.db line 4: invalid calendar date (years 1900-9999)
~~~

## T21 Leap years invalid dates and monotonic business date

**PASS**

Command: ./main --db catalog.db --date 2028-02-20 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
add-book B B-title Author 1
add-reader S S-name S@example.invalid student

Expected stdout
Added book B
Registered reader S

Actual stdout
Added book B
Registered reader S

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Command: ./main --db catalog.db --date 2028-02-20 --batch

Expected exit: 1. Actual exit: 1.

~~~text
Standard input
borrow B S
advance 2028-02-29
date
advance 2100-02-29
advance 2028-02-28
date

Expected stdout
Borrowed B by S due=2028-03-05
Date 2028-02-29 | expired pickups=0
2028-02-29
ERROR: invalid calendar date (years 1900-9999)
ERROR: business date cannot move backwards
2028-02-29

Actual stdout
Borrowed B by S due=2028-03-05
Date 2028-02-29 | expired pickups=0
2028-02-29
ERROR: invalid calendar date (years 1900-9999)
ERROR: business date cannot move backwards
2028-02-29

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Command: ./main --db catalog.db --date 2028-02-28 --batch

Expected exit: 2. Actual exit: 2.

~~~text
Standard input
date

Expected stdout
(empty)

Actual stdout
(empty)

Expected stderr
Contains earlier than the saved business date

Actual stderr
FATAL: --date is earlier than the saved business date
~~~

Command: ./main --db catalog.db --date 2028-02-29 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
date

Expected stdout
2028-02-29

Actual stdout
2028-02-29

Expected stderr
(empty)

Actual stderr
(empty)
~~~

## T22 Interactive menu submission cancellation and EOF

**PASS**

Command: ./main --db catalog.db --date 2026-01-01

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
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

Expected stdout
In order:
StackDesk  |  2026-01-01
Choose a number from 0 to 5.
Book catalog
Type :back to cancel this action.
Book ID:
Book catalog
Added book A
A | The Study Book | Ana Reed | copies=2 available=2 ready=0
Desk closed.

Actual stdout

StackDesk  |  2026-01-01
  1  Book catalog
  2  Reader registry
  3  Loans and returns
  4  Reservations
  5  Reports
  0  Exit
Select: Choose a number from 0 to 5.

StackDesk  |  2026-01-01
  1  Book catalog
  2  Reader registry
  3  Loans and returns
  4  Reservations
  5  Reports
  0  Exit
Select:
Book catalog
  1  Add a book
  2  View a book by ID
  3  Search titles, authors or IDs
  4  Edit book details and stock
  5  List books
  0  Back
Select: Type :back to cancel this action.
Book ID:
Book catalog
  1  Add a book
  2  View a book by ID
  3  Search titles, authors or IDs
  4  Edit book details and stock
  5  List books
  0  Back
Select: Type :back to cancel this action.
Book ID: Title: Author: Copies:
Added book A

Book catalog
  1  Add a book
  2  View a book by ID
  3  Search titles, authors or IDs
  4  Edit book details and stock
  5  List books
  0  Back
Select: Type :back to cancel this action.
Book ID:
A | The Study Book | Ana Reed | copies=2 available=2 ready=0

Book catalog
  1  Add a book
  2  View a book by ID
  3  Search titles, authors or IDs
  4  Edit book details and stock
  5  List books
  0  Back
Select: Desk closed.

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
books

Expected stdout
A | The Study Book | Ana Reed | copies=2 available=2 ready=0

Actual stdout
A | The Study Book | Ana Reed | copies=2 available=2 ready=0

Expected stderr
(empty)

Actual stderr
(empty)
~~~

## T23 Existing session prevents a simultaneous writer

**PASS**

First batch process holds the session lock with stdin open

~~~text
Expected: True
Actual:   True
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 2. Actual exit: 2.

~~~text
Standard input
add-book B Title Author 1

Expected stdout
(empty)

Actual stdout
(empty)

Expected stderr
Contains path is locked

Actual stderr
FATAL: path is locked: <case>/catalog.db.lock
~~~

Lock directory released when the first session exits

~~~text
Expected: False
Actual:   False
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
add-book B Title Author 1

Expected stdout
Added book B

Actual stdout
Added book B

Expected stderr
(empty)

Actual stderr
(empty)
~~~

## T24 Validate database extension and empty CSV export

**PASS**

Command: ./main --db catalog.db --date 2026-01-01 --batch

Expected exit: 0. Actual exit: 0.

~~~text
Standard input
export-overdue catalog.csv

Expected stdout
Exported overdue report: catalog.csv

Actual stdout
Exported overdue report: catalog.csv

Expected stderr
(empty)

Actual stderr
(empty)
~~~

Empty export still has a CSV header

~~~text
Expected: b'days_late,book_id,title,reader_id,reader_name,contact,due_date\r\n'
Actual:   b'days_late,book_id,title,reader_id,reader_name,contact,due_date\r\n'
~~~

Command: ./main --db catalog.db --date 2026-01-01 --batch --db catalog.csv

Expected exit: 2. Actual exit: 2.

~~~text
Standard input
stats

Expected stdout
(empty)

Actual stdout
(empty)

Expected stderr
Contains database filename must end in .db

Actual stderr
FATAL: database filename must end in .db
~~~

Separate export never creates a database

~~~text
Expected: False
Actual:   False
~~~
