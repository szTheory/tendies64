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
  COMBINED=$ROOT/build/evidence/combined/$GITHUB_JOB
  mkdir -p "$COMBINED" || die "cannot create the combined evidence upload directory"
  cp "$OUT" "$COMBINED/$GITHUB_JOB.json" ||
    die "cannot stage $GITHUB_JOB boot evidence in the combined upload directory"

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

valid_sha() {
  case $1 in *[!0-9a-f]*|'') return 1 ;; esac
  [ "${#1}" -eq 40 ]
}

valid_decimal() {
  case $1 in *[!0-9]*|'') return 1 ;; esac
  [ "$1" != 0 ] && [ "${1#0}" = "$1" ]
}

collect() {
  repo=$1
  sha=$2
  run=$3
  attempt=$4
  out_arg=$5
  [ "$repo" = "szTheory/tendies64" ] || die "repo must be szTheory/tendies64"
  valid_sha "$sha" || die "--sha must be 40 lowercase hexadecimal characters"
  valid_decimal "$run" || die "--run must be a canonical positive decimal string"
  valid_decimal "$attempt" || die "--attempt must be a canonical positive decimal string"
  [ -n "$out_arg" ] || die "--out must name a fresh output directory"
  command -v gh >/dev/null 2>&1 || die "GitHub CLI is required for exact-run collection"
  command -v jq >/dev/null 2>&1 || die "jq is required for exact-run collection"
  command -v shasum >/dev/null 2>&1 || die "shasum is required to record sanitized record digests"

  case $out_arg in
    /*) out=$out_arg ;;
    *) out=$ROOT/$out_arg ;;
  esac
  parent=$(dirname "$out")
  mkdir -p "$parent" || die "cannot create output parent directory"
  [ ! -e "$out" ] && [ ! -L "$out" ] || die "output path already exists; refusing stale or local evidence"
  TMP=$(mktemp -d "$parent/.boot-evidence-collect.XXXXXX") || die "cannot create private collection directory"
  chmod 700 "$TMP" || die "cannot protect private collection directory"

  run_file=$TMP/run.json
  attempt_file=$TMP/attempt.json
  jobs_pages=$TMP/jobs.pages
  artifacts_pages=$TMP/artifacts.pages
  jobs_file=$TMP/jobs.json
  artifacts_file=$TMP/artifacts.json
  gh api "repos/$repo/actions/runs/$run" > "$run_file" || die "cannot read selected workflow run"
  gh api "repos/$repo/actions/runs/$run/attempts/$attempt" > "$attempt_file" || die "cannot read selected workflow run attempt"
  jq -e --arg repo "$repo" --arg sha "$sha" --arg run "$run" --arg attempt "$attempt" '
    (.id | tostring) == $run and .repository.full_name == $repo and
    .path == ".github/workflows/ci.yml" and .event == "push" and
    .head_sha == $sha and (.run_attempt | tostring) == $attempt and
    .status == "completed" and .conclusion == "success"
  ' "$run_file" >/dev/null || die "selected run is not a successful ci.yml push for this repository and SHA"
  jq -e --arg sha "$sha" --arg run "$run" --arg attempt "$attempt" '
    (.id | tostring) == $run and (.run_attempt | tostring) == $attempt and
    .path == ".github/workflows/ci.yml" and .head_sha == $sha and
    .status == "completed" and .conclusion == "success"
  ' "$attempt_file" >/dev/null || die "selected attempt is not completed and successful for this repository and SHA"

  : > "$jobs_pages"
  page=1
  while :; do
    response=$TMP/jobs-$page.json
    gh api "repos/$repo/actions/runs/$run/attempts/$attempt/jobs?per_page=100&page=$page" > "$response" || die "cannot enumerate selected attempt jobs"
    jq -e '(.jobs | type) == "array" and (.total_count | type) == "number"' "$response" >/dev/null || die "job API response is malformed"
    cat "$response" >> "$jobs_pages"
    printf '\n' >> "$jobs_pages"
    count=$(jq '.jobs | length' "$response")
    [ "$count" -lt 100 ] && break
    page=$((page + 1))
    [ "$page" -le 100 ] || die "job pagination exceeded the safety bound"
  done
  jq -s '{total_count:.[0].total_count,jobs:[.[].jobs[]]}' "$jobs_pages" > "$jobs_file" || die "cannot combine job pages"
  jq -e --arg sha "$sha" --arg run "$run" --arg attempt "$attempt" '
    .total_count == 2 and (.jobs | length) == 2 and
    ([.jobs[] | select(.name == "gate-linux")] | length) == 1 and
    ([.jobs[] | select(.name == "gate-macos")] | length) == 1 and
    all(.jobs[];
      (.run_id | tostring) == $run and
      (.run_attempt | tostring) == $attempt and .head_sha == $sha and
      .status == "completed" and .conclusion == "success")
  ' "$jobs_file" >/dev/null || die "attempt must contain exactly one successful exact-SHA gate-linux and gate-macos job"

  : > "$artifacts_pages"
  page=1
  while :; do
    response=$TMP/artifacts-$page.json
    gh api "repos/$repo/actions/runs/$run/artifacts?per_page=100&page=$page" > "$response" || die "cannot enumerate selected run artifacts"
    jq -e '(.artifacts | type) == "array" and (.total_count | type) == "number"' "$response" >/dev/null || die "artifact API response is malformed"
    cat "$response" >> "$artifacts_pages"
    printf '\n' >> "$artifacts_pages"
    count=$(jq '.artifacts | length' "$response")
    [ "$count" -lt 100 ] && break
    page=$((page + 1))
    [ "$page" -le 100 ] || die "artifact pagination exceeded the safety bound"
  done
  jq -s '{total_count:.[0].total_count,artifacts:[.[].artifacts[]]}' "$artifacts_pages" > "$artifacts_file" || die "cannot combine artifact pages"
  linux_name=t64-authored-evidence-$sha-$run-$attempt-gate-linux
  macos_name=t64-authored-evidence-$sha-$run-$attempt-gate-macos
  jq -e --arg sha "$sha" --arg run "$run" --arg attempt "$attempt" --arg linux "$linux_name" --arg macos "$macos_name" '
    def prior_attempt($candidate; $selected):
      (($candidate | length) < ($selected | length)) or
      ((($candidate | length) == ($selected | length)) and ($candidate < $selected));
    (.total_count == (.artifacts | length)) and
    (
      ([.artifacts[] | select((.name // "") | startswith("t64-authored-evidence-"))]) as $proof |
      ([$proof[] | select(.name == $linux or .name == $macos)]) as $current |
      ($proof | length) >= 2 and
      ($current | length) == 2 and
      ([$current[].name] | sort) == ([$linux,$macos] | sort) and
      ([$proof[].name] | unique | length) == ($proof | length) and
      ([$proof[].id | tostring] | unique | length) == ($proof | length) and
      all($proof[];
        . as $artifact |
        ($artifact.name | split("-")) as $parts |
        $parts[5] as $artifact_attempt |
        (($parts | length) == 8 and $parts[0] == "t64" and
         $parts[1] == "authored" and $parts[2] == "evidence" and
         $parts[3] == $sha and $parts[4] == $run and
         ($artifact_attempt | test("^[1-9][0-9]*$")) and
         $parts[6] == "gate" and ($parts[7] == "linux" or $parts[7] == "macos") and
         (($artifact_attempt == $attempt) or prior_attempt($artifact_attempt; $attempt)) and
         (($artifact.id | type) == "number") and $artifact.id > 0 and
         (($artifact.expired | type) == "boolean") and
         (($artifact.size_in_bytes | type) == "number") and
         $artifact.size_in_bytes > 0 and $artifact.size_in_bytes <= 1048576 and
         (($artifact.digest | type) == "string") and
         ($artifact.digest | test("^sha256:[0-9a-f]{64}$")) and
         (($artifact.workflow_run.id | tostring) == $run) and
         ($artifact.workflow_run.head_sha == $sha))
      ) and
      all($current[]; .expired == false)
    )
  ' "$artifacts_file" >/dev/null || die "run must have exactly two unexpired current proof artifacts with only correctly identified older attempts retained"

  mkdir "$TMP/gate-linux" "$TMP/gate-macos" "$TMP/publish" || die "cannot create private artifact directories"
  mkdir -p "$ROOT/build/gate/07-qualification"
  downloads=$(mktemp -d "$ROOT/build/gate/07-qualification/hosted-download.XXXXXX") ||
    die "cannot retain original artifact archives"
  chmod 700 "$downloads"
  cp "$run_file" "$attempt_file" "$jobs_file" "$artifacts_file" "$downloads/" ||
    die "cannot retain original artifact API records"
  echo "collect-boot-evidence: original download records retained at $downloads"
  for job in gate-linux gate-macos; do
    artifact_name=$linux_name
    [ "$job" = gate-linux ] || artifact_name=$macos_name
    artifact_id=$(jq -er --arg name "$artifact_name" '.artifacts[] | select(.name == $name) | .id | tostring' "$artifacts_file") || die "cannot read $job artifact ID"
    artifact_digest=$(jq -er --arg name "$artifact_name" '.artifacts[] | select(.name == $name) | .digest' "$artifacts_file") || die "cannot read $job artifact digest"
    artifact_size=$(jq -er --arg name "$artifact_name" '.artifacts[] | select(.name == $name) | .size_in_bytes' "$artifacts_file") || die "cannot read $job artifact size"
    archive=$downloads/$job.zip
    download_exit=$downloads/$job-download-exit.txt
    # Read one byte beyond the allowed archive size to distinguish an exact
    # limit from an oversized response. Keep both the sink and producer status:
    # a truncated/failed API request must never look like a successful download.
    if ! (
      set +e
      gh api "repos/$repo/actions/artifacts/$artifact_id/zip"
      download_code=$?
      printf '%s\n' "$download_code" > "$download_exit"
      exit "$download_code"
    ) | head -c 1048577 > "$archive"; then
      die "cannot retain bounded $job artifact archive bytes"
    fi
    downloaded_size=$(wc -c < "$archive" | tr -d ' ')
    [ "$downloaded_size" -le 1048576 ] && [ "$downloaded_size" = "$artifact_size" ] ||
      die "$job downloaded archive length differs from its bounded API identity"
    [ -f "$download_exit" ] && [ "$(cat "$download_exit")" = 0 ] ||
      die "cannot download exact $job artifact archive by ID"
    downloaded_digest=sha256:$(shasum -a 256 "$archive" | awk '{print $1}')
    [ "$downloaded_digest" = "$artifact_digest" ] ||
      die "$job downloaded archive digest differs from its API identity"
    jq -cnS --arg id "$artifact_id" --arg sha "$sha" --arg run "$run" \
      --arg attempt "$attempt" --arg job "$job" --arg digest "$downloaded_digest" \
      --argjson bytes "$downloaded_size" \
      '{artifact_id:$id,commit_sha:$sha,run_id:$run,run_attempt:$attempt,job_name:$job,archive_digest:$digest,archive_bytes:$bytes}' \
      > "$downloads/$job-binding.json"
    slot=$TMP/$job
    expected=$slot/$job.json
    python3 -B "$ROOT/scripts/qualify-rdp-vi.py" --extract-upload "$archive" \
      --job "$job" --out "$slot" >/dev/null ||
      die "$job archive failed validation before extraction"
    [ -d "$slot" ] && [ ! -L "$slot" ] || die "$job artifact directory is missing or unsafe"
    python3 -B "$ROOT/scripts/qualify-rdp-vi.py" --verify-upload "$slot" \
      --sha "$sha" --run "$run" --attempt "$attempt" --job "$job" >/dev/null ||
      die "$job combined artifact failed its exact boot/raw evidence member checks"
    cp "$expected" "$TMP/publish/$job.json" || die "cannot stage validated $job record"
    mkdir -p "$TMP/publish/rdp-vi/$job" || die "cannot stage $job raw evidence"
    cp "$slot/rdp-vi/$job/manifest.json" "$slot/rdp-vi/$job/inventory.json" \
      "$slot/rdp-vi/$job/observation.json" "$slot/rdp-vi/$job/raw-export.json" \
      "$TMP/publish/rdp-vi/$job/" ||
      die "cannot retain validated $job raw evidence"
  done
  linux_artifact_id=$(jq -er --arg name "$linux_name" '.artifacts[] | select(.name == $name) | .id | tostring' "$artifacts_file") || die "cannot read gate-linux artifact ID"
  linux_artifact_digest=$(jq -er --arg name "$linux_name" '.artifacts[] | select(.name == $name) | .digest' "$artifacts_file") || die "cannot read gate-linux artifact digest"
  macos_artifact_id=$(jq -er --arg name "$macos_name" '.artifacts[] | select(.name == $name) | .id | tostring' "$artifacts_file") || die "cannot read gate-macos artifact ID"
  macos_artifact_digest=$(jq -er --arg name "$macos_name" '.artifacts[] | select(.name == $name) | .digest' "$artifacts_file") || die "cannot read gate-macos artifact digest"
  linux_record_digest=$(shasum -a 256 "$TMP/publish/gate-linux.json" | awk '{print $1}') || die "cannot hash gate-linux record"
  macos_record_digest=$(shasum -a 256 "$TMP/publish/gate-macos.json" | awk '{print $1}') || die "cannot hash gate-macos record"
  [ ! -e "$out" ] && [ ! -L "$out" ] || die "output path appeared during collection; refusing overwrite"
  mv "$TMP/publish" "$out" || die "cannot atomically publish the validated evidence pair"
  echo "collect-boot-evidence: gate-linux artifact_id=$linux_artifact_id artifact_digest=$linux_artifact_digest record_sha256=$linux_record_digest"
  echo "collect-boot-evidence: gate-macos artifact_id=$macos_artifact_id artifact_digest=$macos_artifact_digest record_sha256=$macos_record_digest"
  echo "collect-boot-evidence: original archive and API records retained at $downloads"
  echo "collect-boot-evidence: collected exact successful run $run attempt $attempt for $sha"
}

selftest() {
  CHECKER=$ROOT/build/gate/runner/t64-result-check
  SUPERVISOR=$ROOT/build/gate/runner/t64-supervisor
  FIXTURES=$ROOT/build/fixtures
  [ -x "$CHECKER" ] && [ -x "$SUPERVISOR" ] || die "build the gate preset before collector selftest"
  [ -f "$FIXTURES/hello.z64" ] && [ -f "$FIXTURES/hello.input.json" ] || die "qualified fixture files are required for collector selftest"
  TMP=$(mktemp -d "${TMPDIR:-/tmp}/t64-boot-collect-selftest.XXXXXX") || die "cannot create collector selftest directory"
  chmod 700 "$TMP" || die "cannot protect collector selftest directory"
  mkdir -p "$TMP/work" "$TMP/mockbin" "$TMP/download"
  mkdir -p "$ROOT/build/gate/07-qualification"
  LAYOUT_CONTROL_DIR=$(mktemp -d "$ROOT/build/gate/07-qualification/upload-layout-control.XXXXXX") ||
    die "cannot create private upload-layout evidence directory"
  chmod 700 "$LAYOUT_CONTROL_DIR"
  sha=$(git -C "$ROOT" rev-parse HEAD) || die "cannot determine selftest source revision"
  "$SUPERVISOR" --real --result "$TMP/result.json" --meta "$TMP/meta.json" \
    --work-dir "$TMP/work" --wall-ms 30000 --fixture "$FIXTURES/hello.z64" \
    --input "$FIXTURES/hello.input.json" --capture-out "$TMP/capture.bin" \
    >"$TMP/supervisor.stdout" 2>"$TMP/supervisor.stderr" || die "real local supervisor fixture failed in collector selftest"
  printf '%s\n' 'Tendies64 hello via open IPL3' > "$TMP/expected.log"
  cmp -s "$TMP/expected.log" "$TMP/capture.bin" || die "local selftest result lacks the authored transcript"
  jq -cnS --arg commit "$sha" --arg run 123 --arg attempt 1 --arg job gate-linux \
    --slurpfile result "$TMP/result.json" \
    '{schema:"t64.boot-evidence",schema_version:1,commit_sha:$commit,run_id:$run,run_attempt:$attempt,job_name:$job,result:$result[0]}' \
    > "$TMP/linux.json"
  jq -cnS --arg commit "$sha" --arg run 123 --arg attempt 1 --arg job gate-macos \
    --slurpfile result "$TMP/result.json" \
    '{schema:"t64.boot-evidence",schema_version:1,commit_sha:$commit,run_id:$run,run_attempt:$attempt,job_name:$job,result:$result[0]}' \
    > "$TMP/macos.json"
  "$CHECKER" boot-evidence --sha "$sha" --run 123 --attempt 1 --job gate-linux "$TMP/linux.json" >/dev/null || die "selftest Linux wrapper failed strict C validation"
  "$CHECKER" boot-evidence --sha "$sha" --run 123 --attempt 1 --job gate-macos "$TMP/macos.json" >/dev/null || die "selftest macOS wrapper failed strict C validation"
  linux_name=t64-authored-evidence-$sha-123-1-gate-linux
  macos_name=t64-authored-evidence-$sha-123-1-gate-macos
  for job in gate-linux gate-macos; do
    raw_dir=$TMP/raw/$job
    python3 -B "$ROOT/scripts/qualify-rdp-vi.py" --emit-layout-selftest "$raw_dir" \
      --sha "$sha" --job "$job" --run 123 --attempt 1 >/dev/null ||
      die "actual raw producer failed the $job upload-layout selftest"
    layout=$TMP/layout/$job
    mkdir -p "$layout/rdp-vi/$job"
    boot_record=$TMP/linux.json
    [ "$job" = gate-linux ] || boot_record=$TMP/macos.json
    cp "$boot_record" "$layout/$job.json"
    cp "$raw_dir"/* "$layout/rdp-vi/$job/"
    archive_name=$linux_name
    [ "$job" = gate-linux ] || archive_name=$macos_name
    python3 -B "$ROOT/scripts/qualify-rdp-vi.py" --package-upload "$layout" \
      --sha "$sha" --job "$job" --run 123 --attempt 1 --out "$TMP/$archive_name.zip" \
      >/dev/null || die "cannot package actual $job files with the upload action root layout"
  done
  python3 -B "$ROOT/scripts/qualify-rdp-vi.py" --scan-pii "$TMP/layout" \
    >"$TMP/upload-pii.stdout" 2>"$TMP/upload-pii.stderr" ||
    die "actual combined upload files failed the PII scan"
  for job in gate-linux gate-macos; do
    sh "$ROOT/scripts/rights-scan.sh" --staging "$TMP/layout/$job" \
      >"$TMP/upload-rights-$job.stdout" 2>"$TMP/upload-rights-$job.stderr" ||
      die "actual $job combined upload files failed the rights/provenance scan"
  done
  linux_attempt2_name=t64-authored-evidence-$sha-123-2-gate-linux
  macos_attempt2_name=t64-authored-evidence-$sha-123-2-gate-macos
  jq -cnS --arg commit "$sha" --arg run 123 --arg attempt 2 --arg job gate-linux \
    --slurpfile result "$TMP/result.json" \
    '{schema:"t64.boot-evidence",schema_version:1,commit_sha:$commit,run_id:$run,run_attempt:$attempt,job_name:$job,result:$result[0]}' \
    > "$TMP/linux-attempt2.json"
  jq -cnS --arg commit "$sha" --arg run 123 --arg attempt 2 --arg job gate-macos \
    --slurpfile result "$TMP/result.json" \
    '{schema:"t64.boot-evidence",schema_version:1,commit_sha:$commit,run_id:$run,run_attempt:$attempt,job_name:$job,result:$result[0]}' \
    > "$TMP/macos-attempt2.json"
  for job in gate-linux gate-macos; do
    raw_dir=$TMP/raw-attempt2/$job
    python3 -B "$ROOT/scripts/qualify-rdp-vi.py" --emit-layout-selftest "$raw_dir" \
      --sha "$sha" --job "$job" --run 123 --attempt 2 >/dev/null ||
      die "actual raw producer failed the $job attempt-2 upload-layout selftest"
    layout=$TMP/layout-attempt2/$job
    mkdir -p "$layout/rdp-vi/$job"
    boot_record=$TMP/linux-attempt2.json
    [ "$job" = gate-linux ] || boot_record=$TMP/macos-attempt2.json
    cp "$boot_record" "$layout/$job.json"
    cp "$raw_dir"/* "$layout/rdp-vi/$job/"
    archive_name=$linux_attempt2_name
    [ "$job" = gate-linux ] || archive_name=$macos_attempt2_name
    python3 -B "$ROOT/scripts/qualify-rdp-vi.py" --package-upload "$layout" \
      --sha "$sha" --job "$job" --run 123 --attempt 2 --out "$TMP/$archive_name.zip" \
      >/dev/null || die "cannot package actual $job attempt-2 files with the upload action root layout"
  done
  for mutation in extra duplicate traversal raw-data raw-resealed; do
    python3 -B "$ROOT/scripts/qualify-rdp-vi.py" --mutate-upload-archive "$TMP/$linux_name.zip" \
      --job gate-linux --mutation "$mutation" --out "$TMP/control-$mutation.zip" >/dev/null ||
      die "cannot create the $mutation combined-artifact rejection control"
  done
  python3 -B "$ROOT/scripts/qualify-rdp-vi.py" --mutate-upload-archive "$TMP/$macos_name.zip" \
    --job gate-macos --mutation raw-resealed --out "$TMP/control-raw-resealed-macos.zip" >/dev/null ||
    die "cannot create the matching macOS resealed raw control"
  # Mutate the actual upload ZIPs, then advertise their new archive identity.
  # This reaches the production pre-extraction checks instead of replacing an
  # already-extracted filesystem tree inside the mock transport.
  python3 -B - "$TMP" "$linux_name" <<'PY'
import json
from pathlib import Path
import stat
import sys
import zipfile

root = Path(sys.argv[1])
with zipfile.ZipFile(root / (sys.argv[2] + ".zip")) as source:
    entries = [(info, source.read(info)) for info in source.infolist()]
changed = bytearray((root / (sys.argv[2] + ".zip")).read_bytes())
(root / "control-download-failed.zip").write_bytes(changed)
changed[10] ^= 1
(root / "control-digest-mismatch.zip").write_bytes(changed)
(root / "control-download-overflow.zip").write_bytes(b"x" * 2097152)
for case in ("malformed-record", "unknown-record-field", "wrong-record-job",
             "extra-file", "symlink", "expanded-size", "special-mode"):
    with zipfile.ZipFile(root / ("control-" + case + ".zip"), "w",
                         compression=zipfile.ZIP_DEFLATED) as archive:
        for info, original in entries:
            data = original
            if info.filename == "gate-linux.json":
                if case == "malformed-record":
                    data = b"{not-json"
                elif case == "unknown-record-field":
                    value = json.loads(data)
                    value["unexpected"] = True
                    data = json.dumps(value).encode()
                elif case == "wrong-record-job":
                    data = (root / "macos.json").read_bytes()
                elif case == "expanded-size":
                    data = b"x" * 1048577
                elif case == "special-mode":
                    info = zipfile.ZipInfo(info.filename)
                    info.create_system = 3
                    info.external_attr = (stat.S_IFIFO | 0o600) << 16
            archive.writestr(info, data)
        if case == "extra-file":
            archive.writestr("unexpected.txt", b"unexpected")
        elif case == "symlink":
            link = zipfile.ZipInfo("unexpected-link")
            link.create_system = 3
            link.external_attr = (stat.S_IFLNK | 0o777) << 16
            archive.writestr(link, b"gate-linux.json")
PY
  for mutation in extra duplicate traversal raw-data raw-resealed raw-resealed-macos malformed-record unknown-record-field wrong-record-job extra-file symlink expanded-size special-mode; do
    mutation_archive=$TMP/control-$mutation.zip
    mutation_bytes=$(wc -c < "$mutation_archive" | tr -d ' ')
    mutation_digest=$(shasum -a 256 "$mutation_archive" | awk '{print $1}')
    jq -cn --argjson size "$mutation_bytes" --arg digest "sha256:$mutation_digest" \
      '{size_in_bytes:$size,digest:$digest}' > "$TMP/control-$mutation-identity.json"
  done
  linux_size=$(wc -c < "$TMP/$linux_name.zip" | tr -d ' ')
  macos_size=$(wc -c < "$TMP/$macos_name.zip" | tr -d ' ')
  linux_attempt2_size=$(wc -c < "$TMP/$linux_attempt2_name.zip" | tr -d ' ')
  macos_attempt2_size=$(wc -c < "$TMP/$macos_attempt2_name.zip" | tr -d ' ')
  linux_digest=$(shasum -a 256 "$TMP/$linux_name.zip" | awk '{print $1}')
  macos_digest=$(shasum -a 256 "$TMP/$macos_name.zip" | awk '{print $1}')
  linux_attempt2_digest=$(shasum -a 256 "$TMP/$linux_attempt2_name.zip" | awk '{print $1}')
  macos_attempt2_digest=$(shasum -a 256 "$TMP/$macos_attempt2_name.zip" | awk '{print $1}')
  jq -cn --arg sha "$sha" '{id:123,run_attempt:1,path:".github/workflows/ci.yml",event:"push",head_sha:$sha,status:"completed",conclusion:"success",repository:{full_name:"szTheory/tendies64"}}' > "$TMP/run.json"
  jq -cn --arg sha "$sha" '{id:123,run_attempt:2,path:".github/workflows/ci.yml",event:"push",head_sha:$sha,status:"completed",conclusion:"success",repository:{full_name:"szTheory/tendies64"}}' > "$TMP/run-attempt2.json"
  jq -cn --arg sha "$sha" '{id:123,run_attempt:1,path:".github/workflows/ci.yml",head_sha:$sha,status:"completed",conclusion:"success"}' > "$TMP/attempt.json"
  jq -cn --arg sha "$sha" '{id:123,run_attempt:2,path:".github/workflows/ci.yml",head_sha:$sha,status:"completed",conclusion:"success"}' > "$TMP/attempt2.json"
  jq -cn --arg sha "$sha" '[{id:301,run_id:123,run_attempt:1,head_sha:$sha,name:"gate-linux",status:"completed",conclusion:"success"},{id:302,run_id:123,run_attempt:1,head_sha:$sha,name:"gate-macos",status:"completed",conclusion:"success"}] | {total_count:length,jobs:.}' > "$TMP/jobs.json"
  jq -cn --arg sha "$sha" '[{id:303,run_id:123,run_attempt:2,head_sha:$sha,name:"gate-linux",status:"completed",conclusion:"success"},{id:304,run_id:123,run_attempt:2,head_sha:$sha,name:"gate-macos",status:"completed",conclusion:"success"}] | {total_count:length,jobs:.}' > "$TMP/jobs-attempt2.json"
  jq -cn --arg sha "$sha" --arg linux "$linux_name" --arg macos "$macos_name" --arg ld "$linux_digest" --arg md "$macos_digest" --argjson linux_size "$linux_size" --argjson macos_size "$macos_size" '[{id:401,name:$linux,size_in_bytes:$linux_size,expired:false,digest:("sha256:"+$ld),workflow_run:{id:123,head_sha:$sha}},{id:402,name:$macos,size_in_bytes:$macos_size,expired:false,digest:("sha256:"+$md),workflow_run:{id:123,head_sha:$sha}}] | {total_count:length,artifacts:.}' > "$TMP/artifacts.json"
  jq -cn --arg sha "$sha" --arg linux "$linux_attempt2_name" --arg macos "$macos_attempt2_name" --arg ld "$linux_digest" --arg md "$macos_digest" --arg ld2 "$linux_attempt2_digest" --arg md2 "$macos_attempt2_digest" --argjson old_size "$linux_size" --argjson linux_size "$linux_attempt2_size" --argjson macos_size "$macos_attempt2_size" '[{id:403,name:("t64-authored-evidence-"+$sha+"-123-1-gate-linux"),size_in_bytes:$old_size,expired:true,digest:("sha256:"+$ld),workflow_run:{id:123,head_sha:$sha}},{id:404,name:$linux,size_in_bytes:$linux_size,expired:false,digest:("sha256:"+$ld2),workflow_run:{id:123,head_sha:$sha}},{id:405,name:$macos,size_in_bytes:$macos_size,expired:false,digest:("sha256:"+$md2),workflow_run:{id:123,head_sha:$sha}}] | {total_count:length,artifacts:.}' > "$TMP/artifacts-attempt2.json"
  printf '%s\n' \
    '#!/bin/sh' \
    'set -eu' \
    'root=${T64_EVIDENCE_MOCK_ROOT:?}' \
    'if [ "${1:-}" = api ]; then' \
    '  endpoint=${2:-}' \
    '  mutation=${T64_MOCK_CASE:-}; case $mutation in archive-*) mutation=${mutation#archive-} ;; esac' \
    '  if [ "$endpoint" = "repos/szTheory/tendies64/actions/runs/123/artifacts?per_page=100&page=1" ] && [ -f "$root/control-$mutation-identity.json" ]; then' \
    '    if [ "$mutation" = raw-resealed ]; then jq --slurpfile linux "$root/control-raw-resealed-identity.json" --slurpfile macos "$root/control-raw-resealed-macos-identity.json" ".artifacts[0] += \$linux[0] | .artifacts[1] += \$macos[0]" "$root/artifacts.json"; exit 0; fi' \
    '    jq --slurpfile identity "$root/control-$mutation-identity.json" ".artifacts[0] += \$identity[0]" "$root/artifacts.json"; exit 0' \
    '  fi' \
    '  case "$endpoint" in' \
    '    repos/szTheory/tendies64/actions/artifacts/401/zip|repos/szTheory/tendies64/actions/artifacts/402/zip|repos/szTheory/tendies64/actions/artifacts/404/zip|repos/szTheory/tendies64/actions/artifacts/405/zip)' \
    '      artifact_id=${endpoint%/zip}; artifact_id=${artifact_id##*/}; metadata=$root/artifacts.json' \
    '      case $artifact_id in 404|405) metadata=$root/artifacts-attempt2.json ;; esac' \
    '      name=$(jq -er --argjson id "$artifact_id" ".artifacts[] | select(.id == \$id) | .name" "$metadata")' \
    '      archive=$root/$name.zip' \
    '      if [ "$artifact_id" = 401 ] && [ -f "$root/control-$mutation.zip" ]; then archive=$root/control-$mutation.zip; fi' \
    '      if [ "$artifact_id" = 402 ] && [ "$mutation" = raw-resealed ]; then archive=$root/control-raw-resealed-macos.zip; fi' \
    '      cat "$archive"' \
    '      if [ "${T64_MOCK_CASE:-}" = download-failed ]; then exit 1; fi ;;' \
    '    repos/szTheory/tendies64/actions/runs/123) case ${T64_MOCK_CASE:-} in retained-old-attempt|wrong-current-attempt) cat "$root/run-attempt2.json" ;; wrong-run-sha) jq ".head_sha=\"0000000000000000000000000000000000000000\"" "$root/run.json" ;; wrong-run-id) jq ".id=999" "$root/run.json" ;; wrong-run-attempt) jq ".run_attempt=2" "$root/run.json" ;; *) cat "$root/run.json" ;; esac ;;' \
    '    repos/szTheory/tendies64/actions/runs/123/attempts/1) case ${T64_MOCK_CASE:-} in wrong-attempt) jq ".run_attempt=2" "$root/attempt.json" ;; *) cat "$root/attempt.json" ;; esac ;;' \
    '    repos/szTheory/tendies64/actions/runs/123/attempts/1/jobs?per_page=100\&page=1) case ${T64_MOCK_CASE:-} in failed-job) jq ".jobs[1].conclusion=\"failure\"" "$root/jobs.json" ;; incomplete-job) jq ".jobs[1].status=\"in_progress\"" "$root/jobs.json" ;; duplicate-job) jq ".jobs += [.jobs[0]] | .total_count=.jobs|length" "$root/jobs.json" ;; missing-job) jq ".jobs=[.jobs[0]] | .total_count=1" "$root/jobs.json" ;; wrong-job-sha) jq ".jobs[1].head_sha=\"0000000000000000000000000000000000000000\"" "$root/jobs.json" ;; *) cat "$root/jobs.json" ;; esac ;;' \
    '    repos/szTheory/tendies64/actions/runs/123/attempts/2) cat "$root/attempt2.json" ;;' \
    '    repos/szTheory/tendies64/actions/runs/123/attempts/2/jobs?per_page=100\&page=1) cat "$root/jobs-attempt2.json" ;;' \
    '    repos/szTheory/tendies64/actions/runs/123/artifacts?per_page=100\&page=1) case ${T64_MOCK_CASE:-} in retained-old-attempt) cat "$root/artifacts-attempt2.json" ;; wrong-current-attempt) jq ".artifacts[2].name |= sub(\"-2-gate-macos$\";\"-1-gate-macos\")" "$root/artifacts-attempt2.json" ;; missing-artifact) jq ".artifacts=[.artifacts[0]] | .total_count=1" "$root/artifacts.json" ;; duplicate-artifact) jq ".artifacts += [.artifacts[0] | .id=499] | .total_count=.artifacts|length" "$root/artifacts.json" ;; wrong-artifact-attempt) jq ".artifacts |= map(.name |= sub(\"-1-gate-\";\"-2-gate-\"))" "$root/artifacts.json" ;; wrong-artifact-run) jq ".artifacts[0].workflow_run.id=999" "$root/artifacts.json" ;; wrong-artifact-source) jq ".artifacts[0].workflow_run.head_sha=\"0000000000000000000000000000000000000000\"" "$root/artifacts.json" ;; expired-artifact) jq ".artifacts[0].expired=true" "$root/artifacts.json" ;; large-artifact) jq ".artifacts[0].size_in_bytes=1048577" "$root/artifacts.json" ;; wrong-artifact-sha) jq ".artifacts[0].name |= sub(\"[0-9a-f]{40}\";\"0000000000000000000000000000000000000000\")" "$root/artifacts.json" ;; *) cat "$root/artifacts.json" ;; esac ;;' \
    '    *) echo "unexpected mock API endpoint: $endpoint" >&2; exit 2 ;;' \
    '  esac' \
    '  exit 0' \
    'fi' \
    'echo "unexpected mock command; raw artifact API download is required" >&2; exit 2' \
    > "$TMP/mockbin/gh"
  chmod 700 "$TMP/mockbin/gh"
  T64_QUALIFIER=$ROOT/scripts/qualify-rdp-vi.py
  export T64_QUALIFIER
  if ! PATH="$TMP/mockbin:$PATH" T64_EVIDENCE_MOCK_ROOT="$TMP" \
    sh "$ROOT/scripts/collect-boot-evidence.sh" --collect --repo szTheory/tendies64 \
    --sha "$sha" --run 123 --attempt 1 --out "$TMP/collected" \
    >"$TMP/valid-collection.stdout" 2>"$TMP/valid-collection.stderr"; then
    die "mocked exact-run collection failed for a valid combined artifact pair"
  fi
  [ -f "$TMP/collected/gate-linux.json" ] && [ ! -L "$TMP/collected/gate-linux.json" ] || die "mocked collector did not publish Linux record"
  [ -f "$TMP/collected/gate-macos.json" ] && [ ! -L "$TMP/collected/gate-macos.json" ] || die "mocked collector did not publish macOS record"
  [ "$(find "$TMP/collected" -type f | wc -l | tr -d ' ')" = 10 ] || die "mocked collector did not publish exactly two boot records and eight text evidence files"
  cmp -s "$TMP/linux.json" "$TMP/collected/gate-linux.json" || die "mocked Linux record changed during collection"
  cmp -s "$TMP/macos.json" "$TMP/collected/gate-macos.json" || die "mocked macOS record changed during collection"
  for job in gate-linux gate-macos; do
    for member in manifest.json inventory.json observation.json raw-export.json; do
      cmp -s "$TMP/layout/$job/rdp-vi/$job/$member" "$TMP/collected/rdp-vi/$job/$member" ||
        die "mocked collector changed $job raw member $member"
    done
  done
  if ! PATH="$TMP/mockbin:$PATH" T64_EVIDENCE_MOCK_ROOT="$TMP" \
    T64_QUALIFIER="$ROOT/scripts/qualify-rdp-vi.py" \
    python3 -B "$ROOT/scripts/qualify-rdp-vi.py" --collect-hosted \
      --sha "$sha" --run 123 --attempt 1 --out "$TMP/raw-collected" \
      >"$TMP/raw-collector.stdout" 2>"$TMP/raw-collector.stderr"; then
    cat "$TMP/raw-collector.stdout" >&2
    cat "$TMP/raw-collector.stderr" >&2
    die "new raw collector failed the exact mocked combined upload pair"
  fi
  [ "$(find "$TMP/raw-collected" -type f | wc -l | tr -d ' ')" = 10 ] ||
    die "new raw collector did not retain both boot records and the exact text evidence files"
  for job in gate-linux gate-macos; do
    for member in manifest.json inventory.json observation.json raw-export.json; do
      cmp -s "$TMP/layout/$job/rdp-vi/$job/$member" "$TMP/raw-collected/rdp-vi/$job/$member" ||
        die "new raw collector changed $job member $member"
    done
  done
  PATH="$TMP/mockbin:$PATH" T64_EVIDENCE_MOCK_ROOT="$TMP" T64_MOCK_CASE=retained-old-attempt \
    sh "$ROOT/scripts/collect-boot-evidence.sh" --collect --repo szTheory/tendies64 \
    --sha "$sha" --run 123 --attempt 2 --out "$TMP/collected-attempt2" >/dev/null ||
    die "mocked exact-run collection rejected a valid attempt-2 pair with a retained attempt-1 artifact"
  [ "$(find "$TMP/collected-attempt2" -type f | wc -l | tr -d ' ')" = 10 ] || die "attempt-2 collection did not publish exactly two current boot records and eight text evidence files"
  cmp -s "$TMP/linux-attempt2.json" "$TMP/collected-attempt2/gate-linux.json" || die "attempt-2 Linux record changed during collection"
  cmp -s "$TMP/macos-attempt2.json" "$TMP/collected-attempt2/gate-macos.json" || die "attempt-2 macOS record changed during collection"
  for job in gate-linux gate-macos; do
    for member in manifest.json inventory.json observation.json raw-export.json; do
      cmp -s "$TMP/layout-attempt2/$job/rdp-vi/$job/$member" "$TMP/collected-attempt2/rdp-vi/$job/$member" ||
        die "attempt-2 mocked collector changed $job raw member $member"
    done
  done
  rejects=0
  expect_reject() {
    case_name=$1
    shift
    rm -rf "$TMP/rejected"
    if PATH="$TMP/mockbin:$PATH" T64_EVIDENCE_MOCK_ROOT="$TMP" T64_MOCK_CASE="$case_name" \
      sh "$ROOT/scripts/collect-boot-evidence.sh" --collect --repo szTheory/tendies64 \
      --sha "$sha" --run 123 --attempt 1 --out "$TMP/rejected" >/dev/null 2>&1; then
      die "collector selftest accepted $*"
    fi
    [ ! -e "$TMP/rejected" ] && [ ! -L "$TMP/rejected" ] || die "collector selftest partially published after rejecting $*"
    rejects=$((rejects + 1))
  }
  expect_attempt2_reject() {
    case_name=$1
    description=$2
    rm -rf "$TMP/rejected-attempt2"
    if PATH="$TMP/mockbin:$PATH" T64_EVIDENCE_MOCK_ROOT="$TMP" T64_MOCK_CASE="$case_name" \
      sh "$ROOT/scripts/collect-boot-evidence.sh" --collect --repo szTheory/tendies64 \
      --sha "$sha" --run 123 --attempt 2 --out "$TMP/rejected-attempt2" >/dev/null 2>&1; then
      die "collector selftest accepted $description"
    fi
    [ ! -e "$TMP/rejected-attempt2" ] && [ ! -L "$TMP/rejected-attempt2" ] || die "collector selftest partially published after rejecting $description"
    rejects=$((rejects + 1))
  }
  expect_archive_reject() {
    case_name=$1
    mutation=$2
    expected_error=$3
    if PATH="$TMP/mockbin:$PATH" T64_EVIDENCE_MOCK_ROOT="$TMP" T64_MOCK_CASE="$case_name" \
      sh "$ROOT/scripts/collect-boot-evidence.sh" --collect --repo szTheory/tendies64 \
      --sha "$sha" --run 123 --attempt 1 --out "$TMP/rejected-$mutation" \
      >"$TMP/$mutation.stdout" 2>"$TMP/$mutation.stderr"; then
      die "collector selftest accepted an archive with a $mutation member"
    fi
    [ ! -e "$TMP/rejected-$mutation" ] && [ ! -L "$TMP/rejected-$mutation" ] ||
      die "collector selftest partially published after rejecting the $mutation member"
    grep -F "$expected_error" "$TMP/$mutation.stderr" >/dev/null ||
      { cat "$TMP/$mutation.stdout" >&2; cat "$TMP/$mutation.stderr" >&2; die "archive $mutation control failed before its intended member check"; }
    rejects=$((rejects + 1))
  }
  expect_attempt2_reject wrong-current-attempt "a current macOS artifact mislabeled as retained attempt 1"
  expect_reject wrong-run-sha "a wrong run SHA"
  expect_reject wrong-run-id "a wrong run id"
  expect_reject wrong-run-attempt "a run object for another attempt"
  expect_reject wrong-attempt "a wrong attempt"
  expect_reject failed-job "a failed job"
  expect_reject incomplete-job "an incomplete job"
  expect_reject duplicate-job "a duplicate job"
  expect_reject missing-job "a missing job"
  expect_reject wrong-job-sha "a wrong-SHA job"
  expect_reject missing-artifact "a missing artifact"
  expect_reject duplicate-artifact "a duplicate artifact"
  expect_reject wrong-artifact-attempt "an artifact for another attempt"
  expect_reject wrong-artifact-sha "an artifact for another SHA"
  expect_reject wrong-artifact-run "an artifact associated with another run"
  expect_reject wrong-artifact-source "an artifact associated with another source SHA"
  expect_reject expired-artifact "an expired artifact"
  expect_reject large-artifact "an artifact exceeding its size bound"
  expect_archive_reject digest-mismatch digest-mismatch "downloaded archive digest differs from its API identity"
  expect_archive_reject download-overflow download-overflow "downloaded archive length differs from its bounded API identity"
  overflow_records=$(sed -n 's/^collect-boot-evidence: original download records retained at //p' "$TMP/download-overflow.stdout")
  [ -d "$overflow_records" ] && [ ! -L "$overflow_records" ] || die "overflow control lost its original download records"
  [ "$(wc -c < "$overflow_records/gate-linux.zip" | tr -d ' ')" = 1048577 ] ||
    die "overflow control did not cap the actual download at one byte above the limit"
  [ -f "$overflow_records/gate-linux-download-exit.txt" ] ||
    die "overflow control returned without the downloader exit record"
  expect_archive_reject download-failed download-failed "cannot download exact gate-linux artifact archive by ID"
  expect_reject malformed-record "malformed downloaded JSON"
  expect_reject unknown-record-field "a record with an unknown wrapper field"
  expect_reject wrong-record-job "a replayed record for the other job"
  expect_reject extra-file "an artifact with an extra file"
  expect_reject symlink "an artifact containing a symlink"
  expect_archive_reject archive-extra extra "unexpected or special file"
  expect_archive_reject archive-duplicate duplicate "duplicate or empty members"
  expect_archive_reject archive-traversal traversal "traversal or empty path component"
  expect_archive_reject archive-raw-data raw-data "public raw serialization bytes differ from their manifest binding"
  expect_archive_reject archive-raw-resealed raw-resealed "public raw bytes differ from the independently authored output contract"
  expect_archive_reject expanded-size expanded-size "exceeds the unpacked byte bound"
  expect_archive_reject special-mode special-mode "unexpected or special file"
  mkdir "$TMP/rejected"
  printf '%s\n' 'stale local bytes' > "$TMP/rejected/local.json"
  if PATH="$TMP/mockbin:$PATH" T64_EVIDENCE_MOCK_ROOT="$TMP" \
    sh "$ROOT/scripts/collect-boot-evidence.sh" --collect --repo szTheory/tendies64 \
    --sha "$sha" --run 123 --attempt 1 --out "$TMP/rejected" >/dev/null 2>&1; then
    die "collector selftest accepted pre-existing local output"
  fi
  [ -f "$TMP/rejected/local.json" ] || die "collector selftest altered pre-existing output"
  rejects=$((rejects + 1))
  for job in gate-linux gate-macos; do
    cp -R "$TMP/layout/$job" "$LAYOUT_CONTROL_DIR/$job"
  done
  cp "$TMP/$linux_name.zip" "$LAYOUT_CONTROL_DIR/$linux_name.zip"
  cp "$TMP/$macos_name.zip" "$LAYOUT_CONTROL_DIR/$macos_name.zip"
  cp "$TMP/control-extra.zip" "$LAYOUT_CONTROL_DIR/control-extra.zip"
  cp "$TMP/control-duplicate.zip" "$LAYOUT_CONTROL_DIR/control-duplicate.zip"
  cp "$TMP/control-traversal.zip" "$LAYOUT_CONTROL_DIR/control-traversal.zip"
  cp "$TMP/control-raw-data.zip" "$LAYOUT_CONTROL_DIR/control-raw-data.zip"
  for mutation in digest-mismatch download-overflow download-failed expanded-size special-mode raw-resealed; do
    cp "$TMP/control-$mutation.zip" "$LAYOUT_CONTROL_DIR/control-$mutation.zip"
    cp "$TMP/$mutation.stderr" "$LAYOUT_CONTROL_DIR/$mutation.stderr"
  done
  cp "$TMP/control-raw-resealed-macos.zip" "$LAYOUT_CONTROL_DIR/control-raw-resealed-macos.zip"
  cp "$TMP/valid-collection.stdout" "$LAYOUT_CONTROL_DIR/valid-collection.stdout"
  cp "$TMP/valid-collection.stderr" "$LAYOUT_CONTROL_DIR/valid-collection.stderr"
  cp "$TMP/extra.stderr" "$LAYOUT_CONTROL_DIR/extra.stderr"
  cp "$TMP/duplicate.stderr" "$LAYOUT_CONTROL_DIR/duplicate.stderr"
  cp "$TMP/traversal.stderr" "$LAYOUT_CONTROL_DIR/traversal.stderr"
  cp "$TMP/raw-data.stderr" "$LAYOUT_CONTROL_DIR/raw-data.stderr"
  cp -R "$TMP/collected" "$LAYOUT_CONTROL_DIR/collected"
  cp -R "$TMP/raw-collected" "$LAYOUT_CONTROL_DIR/raw-collected"
  cp "$TMP/raw-collector.stdout" "$LAYOUT_CONTROL_DIR/raw-collector.stdout"
  cp "$TMP/raw-collector.stderr" "$LAYOUT_CONTROL_DIR/raw-collector.stderr"
  cp "$TMP/upload-pii.stdout" "$LAYOUT_CONTROL_DIR/upload-pii.stdout"
  cp "$TMP/upload-pii.stderr" "$LAYOUT_CONTROL_DIR/upload-pii.stderr"
  for job in gate-linux gate-macos; do
    cp "$TMP/upload-rights-$job.stdout" "$LAYOUT_CONTROL_DIR/upload-rights-$job.stdout"
    cp "$TMP/upload-rights-$job.stderr" "$LAYOUT_CONTROL_DIR/upload-rights-$job.stderr"
  done
  jq -cnS --arg sha "$sha" \
    '{argv:["sh","scripts/collect-boot-evidence.sh","--collect","--repo","szTheory/tendies64","--sha",$sha,"--run","123","--attempt","1","--out","<private-temp>/collected"],return_code:0,transport:"mocked-only"}' \
    > "$LAYOUT_CONTROL_DIR/command.json"
  linux_zip_sha=$(shasum -a 256 "$TMP/$linux_name.zip" | awk '{print $1}')
  macos_zip_sha=$(shasum -a 256 "$TMP/$macos_name.zip" | awk '{print $1}')
  boot_linux_sha=$(shasum -a 256 "$TMP/linux.json" | awk '{print $1}')
  boot_macos_sha=$(shasum -a 256 "$TMP/macos.json" | awk '{print $1}')
  linux_manifest=$TMP/layout/gate-linux/rdp-vi/gate-linux/manifest.json
  macos_manifest=$TMP/layout/gate-macos/rdp-vi/gate-macos/manifest.json
  linux_export=$TMP/layout/gate-linux/rdp-vi/gate-linux/raw-export.json
  macos_export=$TMP/layout/gate-macos/rdp-vi/gate-macos/raw-export.json
  linux_source_sha=$(jq -er '.private_originals[] | select(.path == "rdram.bin") | .sha256' "$linux_manifest")
  linux_packet_sha=$(jq -er '.private_originals[] | select(.path == "vi-packet.bin") | .sha256' "$linux_manifest")
  macos_source_sha=$(jq -er '.private_originals[] | select(.path == "rdram.bin") | .sha256' "$macos_manifest")
  macos_packet_sha=$(jq -er '.private_originals[] | select(.path == "vi-packet.bin") | .sha256' "$macos_manifest")
  linux_serial_sha=$(shasum -a 256 "$linux_export" | awk '{print $1}')
  macos_serial_sha=$(shasum -a 256 "$macos_export" | awk '{print $1}')
  linux_serial_bytes=$(wc -c < "$linux_export" | tr -d ' ')
  macos_serial_bytes=$(wc -c < "$macos_export" | tr -d ' ')
  source_address=$(jq -er '.rdram.address' "$linux_export")
  source_bytes=$(jq -er '.rdram.byte_length' "$linux_export")
  frame_width=$(jq -er '.vi_packet.width' "$linux_export")
  frame_height=$(jq -er '.vi_packet.height' "$linux_export")
  frame_stride=$(jq -er '.vi_packet.stride_bytes' "$linux_export")
  packet_bytes=$(jq -er '.vi_packet.byte_length' "$linux_export")
  jq -cnS --arg sha "$sha" --arg linux_zip "$linux_zip_sha" --arg macos_zip "$macos_zip_sha" \
    --arg boot_linux "$boot_linux_sha" --arg boot_macos "$boot_macos_sha" \
    --arg linux_source "$linux_source_sha" --arg linux_packet "$linux_packet_sha" \
    --arg macos_source "$macos_source_sha" --arg macos_packet "$macos_packet_sha" \
    --arg linux_serial "$linux_serial_sha" --arg macos_serial "$macos_serial_sha" \
    --argjson linux_serial_bytes "$linux_serial_bytes" --argjson macos_serial_bytes "$macos_serial_bytes" \
    --argjson source_address "$source_address" --argjson source_bytes "$source_bytes" \
    --argjson frame_width "$frame_width" --argjson frame_height "$frame_height" \
    --argjson frame_stride "$frame_stride" --argjson packet_bytes "$packet_bytes" \
    --argjson rejects "$rejects" \
    '{schema:"t64.rdp-vi-upload-layout-control",schema_version:1,source_sha:$sha,transport:"mocked-only",hosted_proof:false,artifact_root:"build/evidence/combined/<job>",combined_upload_paths:["gate-linux.json","rdp-vi/gate-linux/manifest.json","rdp-vi/gate-linux/inventory.json","rdp-vi/gate-linux/observation.json","rdp-vi/gate-linux/raw-export.json"],actual_boot_records:{gate_linux_sha256:$boot_linux,gate_macos_sha256:$boot_macos},private_original_hashes:{gate_linux:{rdram_sha256:$linux_source,vi_packet_sha256:$linux_packet},gate_macos:{rdram_sha256:$macos_source,vi_packet_sha256:$macos_packet}},captured_span:{rdram:{address:$source_address,byte_length:$source_bytes},vi_packet:{width:$frame_width,height:$frame_height,stride_bytes:$frame_stride,byte_length:$packet_bytes}},public_serializations:{gate_linux:{sha256:$linux_serial,byte_length:$linux_serial_bytes},gate_macos:{sha256:$macos_serial,byte_length:$macos_serial_bytes},decoded_bytes_match_private_originals:true},combined_archive_sha256:{gate_linux:$linux_zip,gate_macos:$macos_zip},upload_scans:{pii:"passed",rights_provenance:"passed"},download_checks:{exact_artifact_id:true,api_digest_verified_before_extraction:true,original_zip_retained:true},rejected_archive_members:["extra","duplicate","traversal","expanded-size","special-mode"],rejected_archive_digest_mismatch:true,rejected_public_raw_mutation:true,rejection_case_count:$rejects}' \
    > "$LAYOUT_CONTROL_DIR/control.json"
  echo "collect-boot-evidence: upload layout selftest passed (actual local boot/raw producers; mocked transport only; $rejects rejection cases; hosted proof=false; evidence=$LAYOUT_CONTROL_DIR)"
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
  --collect)
    [ "$#" -eq 11 ] || die "--collect requires --repo REPO --sha SHA --run RUN --attempt ATTEMPT --out DIR"
    [ "$2" = "--repo" ] && [ "$4" = "--sha" ] && [ "$6" = "--run" ] &&
      [ "$8" = "--attempt" ] && [ "${10}" = "--out" ] ||
      die "usage: --collect --repo REPO --sha SHA --run RUN --attempt ATTEMPT --out DIR"
    collect "$3" "$5" "$7" "$9" "${11}"
    ;;
  --selftest)
    [ "$#" -eq 1 ] || die "--selftest takes no extra arguments"
    selftest
    ;;
  *)
    echo "usage: collect-boot-evidence.sh --emit | --scan-pii | --collect --repo REPO --sha SHA --run RUN --attempt ATTEMPT --out DIR | --selftest" >&2
    exit 2
    ;;
esac
