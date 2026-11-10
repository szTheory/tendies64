#!/bin/sh
# ci-review.sh: measured critical-path review of one completed CI run (VER-04).
# POSIX sh + awk; the GitHub Actions REST API is read through "gh api" only.
#
# Usage:
#   ci-review.sh RUN_ID [--repo OWNER/NAME] [--append FILE]
#                       [--finding "id|finding|taken/declined|reason"]...
#                       [--defer "reason"]
#
#   RUN_ID     numeric workflow run id. The run must be completed.
#   --repo     repository, default szTheory/tendies64.
#   --append   add one row at the end of the "## Runs" table of FILE.
#   --finding  add one row at the end of the "## Findings" table of FILE
#              (repeatable). The row's run-id column is RUN_ID. The verdict
#              must be "taken" or "declined". Needs --append FILE or --log FILE.
#   --log      FILE to receive findings when no row is appended.
#   --defer    with --append: append a dated deferral row stating the reason
#              instead of measured values (for a run whose data cannot be read).
#
# Measured per job from .../actions/runs/<id>/jobs:
#   queue = started_at - created_at, run = completed_at - started_at
# Derived: wall (run created_at to updated_at), critical path (the maximum over
# jobs of queue + run), slowest step (longest step across all jobs).
# Existing rows are never edited or removed; a run id or finding id that is
# already present is refused.
# Exit: 0 ok, 1 refused (run not completed, duplicate row, bad finding),
#       2 usage or tool error.
set -eu
LC_ALL=C
export LC_ALL

REPO=szTheory/tendies64
RUN=""
APPEND=""
LOG=""
DEFER=""
FINDINGS=""
NL='
'

die() {
  echo "ci-review: $*" >&2
  exit 2
}

refuse() {
  echo "ci-review: $*" >&2
  exit 1
}

usage() {
  echo 'usage: ci-review.sh RUN_ID [--repo OWNER/NAME] [--append FILE] [--finding "id|finding|taken/declined|reason"] [--defer "reason"]' >&2
}

while [ $# -gt 0 ]; do
  case $1 in
    --repo)
      [ $# -ge 2 ] || { usage; exit 2; }
      REPO=$2
      shift 2
      ;;
    --append)
      [ $# -ge 2 ] || { usage; exit 2; }
      APPEND=$2
      shift 2
      ;;
    --log)
      [ $# -ge 2 ] || { usage; exit 2; }
      LOG=$2
      shift 2
      ;;
    --finding)
      [ $# -ge 2 ] || { usage; exit 2; }
      FINDINGS="${FINDINGS}$2${NL}"
      shift 2
      ;;
    --defer)
      [ $# -ge 2 ] || { usage; exit 2; }
      DEFER=$2
      shift 2
      ;;
    -h | --help)
      usage
      exit 0
      ;;
    -*)
      usage
      exit 2
      ;;
    *)
      [ -z "$RUN" ] || { usage; exit 2; }
      RUN=$1
      shift
      ;;
  esac
done

[ -n "$RUN" ] || { usage; exit 2; }
case $RUN in
  *[!0-9]*) die "RUN_ID must be numeric" ;;
esac
case $REPO in
  */*) : ;;
  *) die "--repo must be OWNER/NAME" ;;
esac
case $REPO in
  *[!A-Za-z0-9._/-]*) die "--repo has unexpected characters" ;;
esac
[ -z "$DEFER" ] || [ -n "$APPEND" ] || die "--defer needs --append FILE"
if [ -n "$FINDINGS" ] && [ -z "$APPEND" ] && [ -z "$LOG" ]; then
  die "--finding needs --append FILE or --log FILE"
fi
[ -n "$LOG" ] || LOG=$APPEND
if [ -n "$LOG" ]; then
  [ -f "$LOG" ] || die "$LOG not found"
fi

command -v gh >/dev/null 2>&1 || die "gh is required"
gh auth status >/dev/null 2>&1 || die "gh is not authenticated"

# insert_row FILE SECTION ROW: add ROW directly under the last table row of
# "## SECTION" (after the section text when it has no table rows yet). Nothing
# else in FILE changes. ROW travels in the environment so awk never interprets
# escape sequences in it.
insert_row() {
  _f=$1
  _sec=$2
  _tmp=$(mktemp "${TMPDIR:-/tmp}/ci-review.XXXXXX") || die "mktemp failed"
  if ! SEC="## $_sec" ROW=$3 awk '
    { line[NR] = $0 }
    END {
      sec = ENVIRON["SEC"]; row = ENVIRON["ROW"]
      s = 0
      for (i = 1; i <= NR; i++) if (line[i] == sec) { s = i; break }
      if (!s) exit 3
      e = NR
      for (i = s + 1; i <= NR; i++) if (line[i] ~ /^## /) { e = i - 1; break }
      at = 0
      for (i = s + 1; i <= e; i++) if (line[i] ~ /^\|/) at = i
      if (!at) { at = e; while (at > s && line[at] == "") at-- }
      for (i = 1; i <= at; i++) print line[i]
      print row
      for (i = at + 1; i <= NR; i++) print line[i]
    }
  ' "$_f" > "$_tmp"; then
    rm -f "$_tmp"
    die "section \"## $_sec\" not found in $_f"
  fi
  cat "$_tmp" > "$_f"
  rm -f "$_tmp"
}

# Findings are validated before any API call or write.
FINDING_ROWS=""
if [ -n "$FINDINGS" ]; then
  _ifs=$IFS
  IFS=$NL
  for _fd in $FINDINGS; do
    IFS=$_ifs
    [ -n "$_fd" ] || continue
    _id=${_fd%%|*}
    _rest=${_fd#*|}
    [ "$_rest" != "$_fd" ] || refuse "finding needs four '|'-separated fields: $_fd"
    _text=${_rest%%|*}
    _rest2=${_rest#*|}
    [ "$_rest2" != "$_rest" ] || refuse "finding needs four '|'-separated fields: $_id"
    _verdict=${_rest2%%|*}
    _reason=${_rest2#*|}
    [ "$_reason" != "$_rest2" ] || refuse "finding needs four '|'-separated fields: $_id"
    case $_id in
      F-[0-9][0-9][0-9]) : ;;
      *) refuse "finding id must look like F-NNN: $_id" ;;
    esac
    case $_verdict in
      taken | declined) : ;;
      *) refuse "finding $_id verdict must be taken or declined" ;;
    esac
    case $_reason in
      *'|'*) refuse "finding $_id reason must not contain '|'" ;;
    esac
    [ -n "$_text" ] && [ -n "$_reason" ] || refuse "finding $_id has an empty field"
    if [ -n "$LOG" ] && grep -q "^| $_id |" "$LOG"; then
      refuse "finding id $_id already present in $LOG"
    fi
    FINDING_ROWS="${FINDING_ROWS}| ${_id} | ${RUN} | ${_text} | ${_verdict} | ${_reason} |${NL}"
    IFS=$NL
  done
  IFS=$_ifs
fi

if [ -n "$APPEND" ] && grep -qE "^\| [0-9]{4}-[0-9]{2}-[0-9]{2} \| $RUN \|" "$APPEND"; then
  if [ -n "$FINDINGS" ]; then
    echo "ci-review: run $RUN already has a Runs row in $APPEND; adding findings only" >&2
    APPEND_DONE=1
  else
    refuse "run $RUN already has a Runs row in $APPEND"
  fi
else
  APPEND_DONE=0
fi

# Deferral rows need no run data.
if [ -n "$DEFER" ] && [ "$APPEND_DONE" = 0 ]; then
  case $DEFER in
    *'|'*) refuse "--defer reason must not contain '|'" ;;
  esac
  _d=$(date -u +%Y-%m-%d)
  insert_row "$APPEND" Runs "| ${_d} | ${RUN} | - | - | deferred | - | - | - | - | deferred: ${DEFER} |"
  echo "ci-review: appended deferral row for run $RUN"
  APPEND_DONE=1
  DEFER_ONLY=1
else
  DEFER_ONLY=0
fi

if [ "$DEFER_ONLY" = 0 ]; then
  # Run fields. gh api prints one TSV line.
  _run=$(gh api "repos/$REPO/actions/runs/$RUN" --jq '
    def ts: sub("\\.[0-9]+Z$"; "Z") | fromdate;
    [.status, .head_sha, .event, (.conclusion // "-"), .created_at, .updated_at, ((.updated_at | ts) - (.created_at | ts))] | @tsv') \
    || die "cannot read run $RUN of $REPO"
  _status=$(printf '%s' "$_run" | cut -f1)
  _sha=$(printf '%s' "$_run" | cut -f2)
  _event=$(printf '%s' "$_run" | cut -f3)
  _concl=$(printf '%s' "$_run" | cut -f4)
  _created=$(printf '%s' "$_run" | cut -f5)
  _updated=$(printf '%s' "$_run" | cut -f6)
  _wall=$(printf '%s' "$_run" | cut -f7)
  if [ "$_status" != completed ]; then
    refuse "run $RUN is $_status, not completed"
  fi

  # Per job: name, conclusion, queue s, run s. Timestamps may carry a
  # fractional part, which fromdate rejects, so it is stripped first.
  _jobs=$(gh api --paginate "repos/$REPO/actions/runs/$RUN/jobs?per_page=100" --jq '
    def ts: sub("\\.[0-9]+Z$"; "Z") | fromdate;
    .jobs[]
    | select(.started_at != null and .completed_at != null and .created_at != null)
    | [.name, (.conclusion // "-"), ((.started_at | ts) - (.created_at | ts)), ((.completed_at | ts) - (.started_at | ts))]
    | @tsv') || die "cannot read jobs of run $RUN"
  [ -n "$_jobs" ] || die "run $RUN has no readable jobs"

  _steps=$(gh api --paginate "repos/$REPO/actions/runs/$RUN/jobs?per_page=100" --jq '
    def ts: sub("\\.[0-9]+Z$"; "Z") | fromdate;
    .jobs[] | .name as $j | .steps[]?
    | select(.started_at != null and .completed_at != null)
    | [$j, .name, ((.completed_at | ts) - (.started_at | ts))]
    | @tsv') || die "cannot read steps of run $RUN"

  # Derived numbers and the table rows come from one awk pass over the jobs.
  _derived=$(printf '%s\n' "$_jobs" | awk -F '\t' '
    {
      q = $3 + 0; r = $4 + 0; t = q + r
      if (t > cp) { cp = t; cpjob = $1 }
      printf "JOB\t%s\t%s\t%d\t%d\n", $1, $2, q, r
    }
    END { printf "CP\t%d\t%s\n", cp, cpjob }')
  _cp=$(printf '%s\n' "$_derived" | awk -F '\t' '$1 == "CP" { print $2 }')
  _cpjob=$(printf '%s\n' "$_derived" | awk -F '\t' '$1 == "CP" { print $3 }')
  _slow=$(printf '%s\n' "$_steps" | awk -F '\t' '
    NF >= 3 && ($3 + 0) >= best { best = $3 + 0; name = $1 "/" $2 }
    END { if (name != "") printf "%s %ds", name, best; else print "-" }')
  _lin=$(printf '%s\n' "$_derived" | awk -F '\t' '$1 == "JOB" && $2 == "gate-linux" { printf "%d/%d", $4, $5; f = 1 } END { if (!f) printf "-/-" }')
  _mac=$(printf '%s\n' "$_derived" | awk -F '\t' '$1 == "JOB" && $2 == "gate-macos" { printf "%d/%d", $4, $5; f = 1 } END { if (!f) printf "-/-" }')
  _short=$(printf '%s' "$_sha" | cut -c1-12)
  _date=$(printf '%s' "$_created" | cut -c1-10)

  echo "run $RUN  repo $REPO  sha $_short  event $_event  conclusion $_concl"
  echo "created $_created  updated $_updated"
  echo "job                      conclusion   queue s   run s"
  printf '%s\n' "$_derived" | awk -F '\t' '$1 == "JOB" { printf "%-24s %-12s %7d %7d\n", $2, $3, $4, $5 }'
  echo "wall ${_wall}s  critical path ${_cp}s (${_cpjob})  slowest step: ${_slow}"

  if [ -n "$APPEND" ] && [ "$APPEND_DONE" = 0 ]; then
    insert_row "$APPEND" Runs "| ${_date} | ${RUN} | ${_short} | ${_event} | ${_concl} | ${_wall} | ${_cp} | ${_lin} | ${_mac} | ${_slow} |"
    echo "ci-review: appended Runs row for run $RUN to $APPEND"
  fi
fi

if [ -n "$FINDING_ROWS" ]; then
  _ifs=$IFS
  IFS=$NL
  for _r in $FINDING_ROWS; do
    IFS=$_ifs
    [ -n "$_r" ] || continue
    insert_row "$LOG" Findings "$_r"
    IFS=$NL
  done
  IFS=$_ifs
  echo "ci-review: appended finding row(s) to $LOG"
fi
