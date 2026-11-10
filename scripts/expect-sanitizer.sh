#!/bin/sh
set -u

if [ "$#" -ne 1 ]; then
  echo "usage: expect-sanitizer.sh FAULTY_CHILD" >&2
  exit 2
fi

temporary_dir=$(mktemp -d "${TMPDIR:-/tmp}/t64-sanitizer.XXXXXX") || exit 2
trap 'rm -rf "$temporary_dir"' EXIT HUP INT TERM

run_and_require_report() {
  output_file=$1
  shift
  ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$@" \
    >"$output_file" 2>&1
  child_status=$?
  [ "$child_status" -ne 0 ] || return 1
  grep -Eq 'ERROR: AddressSanitizer: .*buffer-overflow' "$output_file"
}

make_exit_control() {
  control_path=$1
  exit_status=$2
  {
    echo '#!/bin/sh'
    echo "exit $exit_status"
  } >"$control_path" || return 1
  chmod +x "$control_path"
}

if ! run_and_require_report "$temporary_dir/fault-report" "$1"; then
  echo "sanitizer control failed: faulty child did not produce a matching report and nonzero exit" >&2
  exit 1
fi

make_exit_control "$temporary_dir/no-report" 7 || exit 2
if run_and_require_report "$temporary_dir/no-report-output" \
  "$temporary_dir/no-report"; then
  echo "sanitizer control failed: non-sanitizer failure was accepted" >&2
  exit 1
fi

make_exit_control "$temporary_dir/zero-exit" 0 || exit 2
if run_and_require_report "$temporary_dir/zero-exit-output" \
  "$temporary_dir/zero-exit"; then
  echo "sanitizer control failed: zero exit was accepted" >&2
  exit 1
fi

echo "sanitizer negative control: expected report and nonzero child exit observed; no-report and zero-exit controls rejected"
