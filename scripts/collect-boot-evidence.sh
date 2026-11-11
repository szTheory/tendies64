#!/bin/sh
# Emit or scan the one sanitized guest-evidence JSON used by the two CI jobs.
set -eu
LC_ALL=C
export LC_ALL

ROOT=$(cd "$(dirname "$0")/.." && pwd -P)
TMP=""
PII_STAGE=""
KEEP_PII=0

cleanup() {
  [ -z "$TMP" ] || rm -rf "$TMP"
  if [ "$KEEP_PII" -eq 0 ] && [ -n "$PII_STAGE" ]; then
    rm -rf "$PII_STAGE"
  fi
}
trap cleanup EXIT HUP INT TERM

die() {
  echo "collect-boot-evidence: $*" >&2
  exit 1
}

emit() {
  case ${GITHUB_SHA:-} in
    *[!0-9a-f]*|'') die "GITHUB_SHA must be a 40-character lowercase commit id" ;;
  esac
  [ "${#GITHUB_SHA}" -eq 40 ] || die "GITHUB_SHA must be a 40-character lowercase commit id"
  case ${GITHUB_RUN_ID:-} in
    *[!0-9]*|'') die "GITHUB_RUN_ID must be an unsigned decimal string" ;;
  esac
  case ${GITHUB_RUN_ATTEMPT:-} in
    *[!0-9]*|'') die "GITHUB_RUN_ATTEMPT must be an unsigned decimal string" ;;
  esac
  case ${GITHUB_JOB:-} in
    gate-linux|gate-macos) ;;
    *) die "GITHUB_JOB must be gate-linux or gate-macos" ;;
  esac

  RESULT_CHECK=$ROOT/build/gate/runner/t64-result-check
  SUPERVISOR=$ROOT/build/gate/runner/t64-supervisor
  FIXTURE_DIR=$ROOT/build/fixtures
  [ -x "$RESULT_CHECK" ] || die "result checker is not built"
  [ -x "$SUPERVISOR" ] || die "real supervisor is not built"
  [ -f "$FIXTURE_DIR/hello.z64" ] && [ -f "$FIXTURE_DIR/hello.input.json" ] ||
    die "qualified hello fixture files are not present"

  mkdir -p "$ROOT/build/evidence"
  TMP=$(mktemp -d "$ROOT/build/evidence/.emit.XXXXXX") || die "cannot create private evidence work directory"
  mkdir "$TMP/work"
  if ! "$SUPERVISOR" --real \
      --result "$TMP/result.json" \
      --meta "$TMP/meta.json" \
      --work-dir "$TMP/work" \
      --wall-ms 30000 \
      --fixture "$FIXTURE_DIR/hello.z64" \
      --input "$FIXTURE_DIR/hello.input.json" \
      --capture-out "$TMP/capture.bin" >"$TMP/supervisor.stdout" 2>"$TMP/supervisor.stderr"; then
    die "real hello supervisor run failed"
  fi
  "$RESULT_CHECK" result "$TMP/result.json" >/dev/null ||
    die "real hello result failed strict validation"
  printf '%s\n' 'Tendies64 hello via open IPL3' > "$TMP/expected.log"
  cmp -s "$TMP/expected.log" "$TMP/capture.bin" ||
    die "real hello capture does not equal the authored transcript"
  jq -e --arg sha "$GITHUB_SHA" '
    .schema == "t64.result" and .schema_version == 2 and
    .source_revision == $sha and .outcome == "pass" and
    .detail == "positive-match" and .fixture_id == "hello-openipl3" and
    .guest_stop.kind == "authored-completion"
  ' "$TMP/result.json" >/dev/null ||
    die "real hello result identity, outcome, or completion does not match this run"

  jq -cnS --arg commit "$GITHUB_SHA" \
    --arg run "$GITHUB_RUN_ID" --arg attempt "$GITHUB_RUN_ATTEMPT" \
    --arg job "$GITHUB_JOB" --slurpfile result "$TMP/result.json" \
    '{schema:"t64.boot-evidence",schema_version:1,commit_sha:$commit,run_id:$run,run_attempt:$attempt,job_name:$job,result:$result[0]}' \
    > "$TMP/evidence.json"
  jq -e '
    (keys == ["commit_sha","job_name","result","run_attempt","run_id","schema","schema_version"]) and
    .schema == "t64.boot-evidence" and .schema_version == 1 and
    (.result.schema == "t64.result") and (.result.schema_version == 2)
  ' "$TMP/evidence.json" >/dev/null ||
    die "evidence wrapper does not match the closed seven-key contract"

  OUT=$ROOT/build/evidence/$GITHUB_JOB.json
  mv "$TMP/evidence.json" "$OUT" ||
    die "cannot atomically publish $GITHUB_JOB evidence"

  PII_STAGE=$ROOT/.t64-evidence-pii
  mkdir "$PII_STAGE" || die "stale PII scan staging directory already exists"
  cp "$OUT" "$PII_STAGE/record.json" ||
    die "cannot stage evidence for the PII scan"
  KEEP_PII=1
  echo "collect-boot-evidence: emitted sanitized $GITHUB_JOB guest record"
}

scan_pii() {
  PII_STAGE=$ROOT/.t64-evidence-pii
  [ -d "$PII_STAGE" ] && [ ! -L "$PII_STAGE" ] ||
    die "PII scan staging directory is absent or unsafe"
  [ -f "$PII_STAGE/record.json" ] && [ ! -L "$PII_STAGE/record.json" ] ||
    die "PII scan staging record is absent or unsafe"
  [ "$(find "$PII_STAGE" -type f | wc -l | tr -d ' ')" = 1 ] ||
    die "PII scan staging directory has unexpected files"
  sh "$ROOT/scripts/pii-scan.sh" --tree
  rm -rf "$PII_STAGE"
  PII_STAGE=""
  KEEP_PII=0
  echo "collect-boot-evidence: sanitized guest record passed the PII scan"
}

case ${1:-} in
  --emit)
    [ "$#" -eq 1 ] || die "--emit takes no extra arguments"
    emit
    ;;
  --scan-pii)
    [ "$#" -eq 1 ] || die "--scan-pii takes no extra arguments"
    scan_pii
    ;;
  *)
    echo "usage: collect-boot-evidence.sh --emit | --scan-pii" >&2
    exit 2
    ;;
esac
