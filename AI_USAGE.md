# AI Usage Declaration and Log

**Tool and model.** ChatGPT Work Mode with OpenAI Codex (GPT-6.2 Astra and GPT-6.2 Sol) assisted with design, implementation, tests, learning notes, documentation, and the terminal-output demonstration. The implementation was generated for this project and checked against the attached COMP2113 project specification.

## Prompt history for this build

The first prompt included the group’s proposed task split. The attached PDF supplied the exact coursework requirements.

~~~text
here is the thing [11/2/26, 11:11:53 PM] ~ William Shi:
Library Management system

Parking Lot Management System

These all seem like viable options, the difficulty level are not too high
[11/3/26, 7:55:43 AM] ~ 🈵: Library management system sounds good
[11/3/26, 11:24:24 AM] ~ Eunice: Works for me
[11/3/26, 11:32:33 AM] Kevin: Library management system works for everyone then.
[11/3/26, 11:32:35 AM] Kevin: Should we divide up the initial requirements or system design tasks?
[11/3/26, 11:55:33 AM] ~ 🈵: Sure
[11/3/26, 3:32:34 PM] ~ William Shi: Our team of five can be divided as follows:

A: Adding, querying, and modifying books.

B: Registering, querying, and modifying reader information.

C: Borrowing and returning books.

D: Reservation queuing and simple statistics.

E: Saving and retrieving data, and helping to connect the different parts.

help me build a full system, that is exceeding the grading require and
~~~

~~~text
你直接从头做,然后你可以再加入2-3个speciall feature,
~~~

~~~text
how much do u finisih
~~~

## How AI was used in the workflow

| Step | AI assistance | Output used in the project |
| --- | --- | --- |
| Requirements and architecture | Read the rubric, mapped the five work areas and proposed three extra features. | Numbered menu and batch mode, the five modules, and the feature descriptions in the README. |
| Learning and implementation | Used an abstract policy interface for two real checkout policies and wrote notes on virtual calls and object ownership. | `policy.h`, `policy.cpp`, `circulation.cpp` and `LEARNING_MATERIALS.md`. |
| Data and reliability | Implemented snapshot validation, a prior backup and a draft-before-save workflow. | `storage.cpp` and snapshot error tests. |
| Testing | Drafted independent expected outputs, compiled the C++ program and compared expected with actual process results. | 24 passing scenarios with the commands and outputs in `tests/TRANSCRIPTS.md`. |
| Demonstration | Ran the batch walkthrough against a temporary database and used the captured output for a captioned video. | `demo/demo.sh` and the demonstration outline. |

## Verification and critical reflection

The automated checks compare real process output with expected output defined from the requirements. For example, in T11 the first ready hold expires on January 4, the second on January 7, and the third is still available for pickup on January 8. The test starts a new process on January 8. This catches an implementation that incorrectly gives the whole queue new deadlines on restart. In T19 a directory occupies the backup path. The save fails, a following read still shows the original record, and the primary file matches its earlier bytes.

The first report implementation printed “1 days”. The final version uses “1 day” for the singular case and checks it in T12 and T13. File export and the database also use distinct extensions so an export and a database cannot be assigned the same path. T24 checks this separation.

The group should append its actual follow-up prompts and its own review records here. For each review, record the reviewer, module, command or test ID, observed result, and any change made. Useful places to inspect are T04 for virtual dispatch, T11 for pickup timing, and T19 for rollback. Record the CS Academy result when the group has executed the build there.

## Browser workspace follow-up

The following project requests prompted the sample visualization and then the integrated English browser UI.

~~~text
这个系统可以visualize吗
我想要你把这个优化UI放到github的c++ code,拉出一个broswer, 这样可以吗,然后system要英语的,你觉得要加加别功能吗,比如ai搜索?
~~~

Codex used actual C++ command outputs to verify the four states in the first visualization. It then implemented an English workspace with a local HTTP adapter and added ranked keyword searches with single-edit spelling tolerance in C++. Browser actions reuse the same checkout, return, reservation and persistence code as the numbered menu. The browser searches run offline by comparing text and do not use a language-model service.

The verification used the 24 existing terminal cases, eight new browser API scenarios, and an actual browser walkthrough. The walkthrough returned the book, advanced the date and checked out the reserved copy as the next eligible reader. It also checked that searching for `pratical c++` highlighted a close match, and the API tests reopened a persistent database to confirm that browser edits still load in the terminal program.

## Book recommendations follow-up

~~~text
对,你就写这个;然后现在你觉得是不是可以加一个就是哎,我有这个人的借书历史记录,我们通过他个人的历史记录和同类人推荐的,给他的outlook推送
~~~

Codex added local C++ ranking for available alternatives and reader-specific recommendations. It used sets of distinct previously borrowed books to identify overlap between readers. At least two distinct readers must support a title before peer borrowing affects the result. The desk displays aggregate reasons and prepares an email draft for staff review. Automatic mail delivery is outside this version.

The new API cases check that an earlier database still loads, topic edits survive reopening, held titles stay out of results, and repeat borrowing by one peer cannot stand in for a second peer.
