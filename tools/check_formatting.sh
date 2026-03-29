#!/usr/bin/env bash
set -euo pipefail

function check_formatting {

    if ! command -v clang-format >/dev/null 2>&1; then
        echo "Error: clang-format is not installed or not on PATH." >&2
        exit 1
    fi

    mapfile -t files < <(git ls-files '*.c' '*.h')

    if [ "${#files[@]}" -eq 0 ]; then
        echo "Error: No C source or header files found." >&2
        exit 0
    fi

    status=0
    for file in "${files[@]}"; do
        # --dry-run/--Werror make clang-format exit non-zero when reformatting is needed
        if ! clang-format --dry-run --Werror -style=file "$file"; then
            echo "Needs formatting: $file"
            status=1
        fi
    done

    if [ "$status" -eq 0 ]; then
        echo "Formatting looks good."
    else
        echo "Formatting issues detected. Run clang-format -i on the files above." >&2
    fi

    exit "$status"

}

function main {
    if ! [ -f Makefile ] 
    then
        echo "call from project root directory"
        exit 1
    fi

    check_formatting
}

main "$@"
