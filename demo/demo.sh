#!/bin/sh
# Run the walkthrough against a fresh database and print the real program output.
set -eu
if [ "$#" -eq 0 ]; then set -- ./main; fi
case "$1" in
    /*) binary=$1 ;;
    *) binary="$PWD/$1" ;;
esac
demo_dir=$(mktemp -d)
trap 'rm -rf "$demo_dir"' EXIT
cd "$demo_dir"

phase() {
    title=$1
    day=$2
    mode=$3
    cat > input.txt
    printf '\n## %s\n' "$title"
    printf '%s' '$ ./main --db demo.db'
    printf ' --date %s' "$day"
    if [ "$mode" = "batch" ]; then printf ' --batch'; fi
    printf '\n---- INPUT ----\n'
    cat input.txt
    printf '%s\n' '---- OUTPUT ----'
    if [ "$mode" = "batch" ]; then
        "$binary" --db demo.db --date "$day" --batch < input.txt
    else
        "$binary" --db demo.db --date "$day" < input.txt
    fi
}

phase 'Register titles and readers' 2026-11-21 batch <<'EOF'
add-book BK101 "Practical C++" "Ana Reed" 1
add-book BK202 "Data Structures" "Jun Wu" 2
add-reader S01 "Alex Chan" alex@example.invalid student
add-reader F01 "Taylor Lee" taylor@example.invalid faculty
add-reader S02 "Jamie Ho" jamie@example.invalid student
EOF

phase 'Browse the numbered menu' 2026-11-21 menu <<'EOF'
1
5
0
0
EOF

phase 'Apply each loan policy' 2026-11-21 batch <<'EOF'
policy student
policy faculty
borrow BK101 S01
borrow BK202 F01
renew BK202 F01
EOF

phase 'Pass an expired pickup to the next reader' 2026-11-21 batch <<'EOF'
reserve BK101 F01
reserve BK101 S02
return BK101 S01
queue BK101
advance 2026-11-24
queue BK101
EOF

phase 'Open a fresh process and collect the saved hold' 2026-11-24 batch <<'EOF'
queue BK101
borrow BK101 S02
history S01
stats
EOF

phase 'List overdue loans and export the CSV' 2026-11-24 batch <<'EOF'
advance 2026-12-12
overdue
export-overdue overdue.csv
EOF
printf '\n%s\n' '---- CSV ----'
cat overdue.csv
