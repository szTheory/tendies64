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
  linux_name=t64-boot-evidence-$sha-$run-$attempt-gate-linux
  macos_name=t64-boot-evidence-$sha-$run-$attempt-gate-macos
  jq -e --arg sha "$sha" --arg run "$run" --arg attempt "$attempt" --arg linux "$linux_name" --arg macos "$macos_name" '
    def prior_attempt($candidate; $selected):
      (($candidate | length) < ($selected | length)) or
      ((($candidate | length) == ($selected | length)) and ($candidate < $selected));
    (.total_count == (.artifacts | length)) and
    (
      ([.artifacts[] | select((.name // "") | startswith("t64-boot-evidence-"))]) as $proof |
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
         $parts[1] == "boot" and $parts[2] == "evidence" and
         $parts[3] == $sha and $parts[4] == $run and
         ($artifact_attempt | test("^[1-9][0-9]*$")) and
         $parts[6] == "gate" and ($parts[7] == "linux" or $parts[7] == "macos") and
         (($artifact_attempt == $attempt) or prior_attempt($artifact_attempt; $attempt)) and
         (($artifact.id | type) == "number") and $artifact.id > 0 and
         (($artifact.expired | type) == "boolean") and
         (($artifact.size_in_bytes | type) == "number") and
         $artifact.size_in_bytes > 0 and $artifact.size_in_bytes <= 65536 and
         (($artifact.digest | type) == "string") and
         ($artifact.digest | test("^sha256:[0-9a-f]{64}$")) and
         (($artifact.workflow_run.id | tostring) == $run) and
         ($artifact.workflow_run.head_sha == $sha))
      ) and
      all($current[]; .expired == false)
    )
  ' "$artifacts_file" >/dev/null || die "run must have exactly two unexpired current proof artifacts with only correctly identified older attempts retained"

  mkdir "$TMP/gate-linux" "$TMP/gate-macos" "$TMP/publish" || die "cannot create private artifact directories"
  gh run download "$run" --repo "$repo" --name "$linux_name" --dir "$TMP/gate-linux" || die "cannot download exact gate-linux artifact"
  gh run download "$run" --repo "$repo" --name "$macos_name" --dir "$TMP/gate-macos" || die "cannot download exact gate-macos artifact"
  for job in gate-linux gate-macos; do
    slot=$TMP/$job
    expected=$slot/$job.json
    [ -d "$slot" ] && [ ! -L "$slot" ] || die "$job artifact directory is missing or unsafe"
    [ -f "$expected" ] && [ ! -L "$expected" ] || die "$job artifact must contain its one regular JSON file"
    [ "$(find "$slot" -type f | wc -l | tr -d ' ')" = 1 ] || die "$job artifact contains extra files"
    [ "$(find "$slot" -type l | wc -l | tr -d ' ')" = 0 ] || die "$job artifact contains a symlink"
    [ "$(find "$slot" -type d | wc -l | tr -d ' ')" = 1 ] || die "$job artifact contains nested directories"
    [ "$(find "$slot" ! -type d ! -type f | wc -l | tr -d ' ')" = 0 ] || die "$job artifact contains a special filesystem entry"
    [ "$(wc -c < "$expected" | tr -d ' ')" -le 65536 ] || die "$job JSON exceeds the 64 KiB bound"
    "$ROOT/build/gate/runner/t64-result-check" boot-evidence \
      --sha "$sha" --run "$run" --attempt "$attempt" --job "$job" "$expected" >/dev/null ||
      die "$job record failed strict evidence validation"
    cp "$expected" "$TMP/publish/$job.json" || die "cannot stage validated $job record"
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
  linux_name=t64-boot-evidence-$sha-123-1-gate-linux
  macos_name=t64-boot-evidence-$sha-123-1-gate-macos
  mkdir -p "$TMP/download/$linux_name" "$TMP/download/$macos_name"
  cp "$TMP/linux.json" "$TMP/download/$linux_name/gate-linux.json"
  cp "$TMP/macos.json" "$TMP/download/$macos_name/gate-macos.json"
  linux_attempt2_name=t64-boot-evidence-$sha-123-2-gate-linux
  macos_attempt2_name=t64-boot-evidence-$sha-123-2-gate-macos
  jq -cnS --arg commit "$sha" --arg run 123 --arg attempt 2 --arg job gate-linux \
    --slurpfile result "$TMP/result.json" \
    '{schema:"t64.boot-evidence",schema_version:1,commit_sha:$commit,run_id:$run,run_attempt:$attempt,job_name:$job,result:$result[0]}' \
    > "$TMP/linux-attempt2.json"
  jq -cnS --arg commit "$sha" --arg run 123 --arg attempt 2 --arg job gate-macos \
    --slurpfile result "$TMP/result.json" \
    '{schema:"t64.boot-evidence",schema_version:1,commit_sha:$commit,run_id:$run,run_attempt:$attempt,job_name:$job,result:$result[0]}' \
    > "$TMP/macos-attempt2.json"
  mkdir -p "$TMP/download/$linux_attempt2_name" "$TMP/download/$macos_attempt2_name"
  cp "$TMP/linux-attempt2.json" "$TMP/download/$linux_attempt2_name/gate-linux.json"
  cp "$TMP/macos-attempt2.json" "$TMP/download/$macos_attempt2_name/gate-macos.json"
  linux_size=$(wc -c < "$TMP/linux.json" | tr -d ' ')
  macos_size=$(wc -c < "$TMP/macos.json" | tr -d ' ')
  linux_attempt2_size=$(wc -c < "$TMP/linux-attempt2.json" | tr -d ' ')
  macos_attempt2_size=$(wc -c < "$TMP/macos-attempt2.json" | tr -d ' ')
  jq -cn --arg sha "$sha" '{id:123,run_attempt:1,path:".github/workflows/ci.yml",event:"push",head_sha:$sha,status:"completed",conclusion:"success",repository:{full_name:"szTheory/tendies64"}}' > "$TMP/run.json"
  jq -cn --arg sha "$sha" '{id:123,run_attempt:2,path:".github/workflows/ci.yml",event:"push",head_sha:$sha,status:"completed",conclusion:"success",repository:{full_name:"szTheory/tendies64"}}' > "$TMP/run-attempt2.json"
  jq -cn --arg sha "$sha" '{id:123,run_attempt:1,path:".github/workflows/ci.yml",head_sha:$sha,status:"completed",conclusion:"success"}' > "$TMP/attempt.json"
  jq -cn --arg sha "$sha" '{id:123,run_attempt:2,path:".github/workflows/ci.yml",head_sha:$sha,status:"completed",conclusion:"success"}' > "$TMP/attempt2.json"
  jq -cn --arg sha "$sha" '[{id:301,run_id:123,run_attempt:1,head_sha:$sha,name:"gate-linux",status:"completed",conclusion:"success"},{id:302,run_id:123,run_attempt:1,head_sha:$sha,name:"gate-macos",status:"completed",conclusion:"success"}] | {total_count:length,jobs:.}' > "$TMP/jobs.json"
  jq -cn --arg sha "$sha" '[{id:303,run_id:123,run_attempt:2,head_sha:$sha,name:"gate-linux",status:"completed",conclusion:"success"},{id:304,run_id:123,run_attempt:2,head_sha:$sha,name:"gate-macos",status:"completed",conclusion:"success"}] | {total_count:length,jobs:.}' > "$TMP/jobs-attempt2.json"
  jq -cn --arg sha "$sha" --arg linux "$linux_name" --arg macos "$macos_name" --argjson linux_size "$linux_size" --argjson macos_size "$macos_size" '[{id:401,name:$linux,size_in_bytes:$linux_size,expired:false,digest:"sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",workflow_run:{id:123,head_sha:$sha}},{id:402,name:$macos,size_in_bytes:$macos_size,expired:false,digest:"sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",workflow_run:{id:123,head_sha:$sha}}] | {total_count:length,artifacts:.}' > "$TMP/artifacts.json"
  jq -cn --arg sha "$sha" --arg linux "$linux_attempt2_name" --arg macos "$macos_attempt2_name" --argjson old_size "$linux_size" --argjson linux_size "$linux_attempt2_size" --argjson macos_size "$macos_attempt2_size" '[{id:403,name:("t64-boot-evidence-"+$sha+"-123-1-gate-linux"),size_in_bytes:$old_size,expired:true,digest:"sha256:cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",workflow_run:{id:123,head_sha:$sha}},{id:404,name:$linux,size_in_bytes:$linux_size,expired:false,digest:"sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd",workflow_run:{id:123,head_sha:$sha}},{id:405,name:$macos,size_in_bytes:$macos_size,expired:false,digest:"sha256:eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee",workflow_run:{id:123,head_sha:$sha}}] | {total_count:length,artifacts:.}' > "$TMP/artifacts-attempt2.json"
  printf '%s\n' \
    '#!/bin/sh' \
    'set -eu' \
    'root=${T64_EVIDENCE_MOCK_ROOT:?}' \
    'if [ "${1:-}" = api ]; then' \
    '  endpoint=${2:-}' \
    '  case "$endpoint" in' \
    '    repos/szTheory/tendies64/actions/runs/123) case ${T64_MOCK_CASE:-} in retained-old-attempt|wrong-current-attempt) cat "$root/run-attempt2.json" ;; wrong-run-sha) jq ".head_sha=\"0000000000000000000000000000000000000000\"" "$root/run.json" ;; wrong-run-id) jq ".id=999" "$root/run.json" ;; wrong-run-attempt) jq ".run_attempt=2" "$root/run.json" ;; *) cat "$root/run.json" ;; esac ;;' \
    '    repos/szTheory/tendies64/actions/runs/123/attempts/1) case ${T64_MOCK_CASE:-} in wrong-attempt) jq ".run_attempt=2" "$root/attempt.json" ;; *) cat "$root/attempt.json" ;; esac ;;' \
    '    repos/szTheory/tendies64/actions/runs/123/attempts/1/jobs?per_page=100\&page=1) case ${T64_MOCK_CASE:-} in failed-job) jq ".jobs[1].conclusion=\"failure\"" "$root/jobs.json" ;; incomplete-job) jq ".jobs[1].status=\"in_progress\"" "$root/jobs.json" ;; duplicate-job) jq ".jobs += [.jobs[0]] | .total_count=.jobs|length" "$root/jobs.json" ;; missing-job) jq ".jobs=[.jobs[0]] | .total_count=1" "$root/jobs.json" ;; wrong-job-sha) jq ".jobs[1].head_sha=\"0000000000000000000000000000000000000000\"" "$root/jobs.json" ;; *) cat "$root/jobs.json" ;; esac ;;' \
    '    repos/szTheory/tendies64/actions/runs/123/attempts/2) cat "$root/attempt2.json" ;;' \
    '    repos/szTheory/tendies64/actions/runs/123/attempts/2/jobs?per_page=100\&page=1) cat "$root/jobs-attempt2.json" ;;' \
    '    repos/szTheory/tendies64/actions/runs/123/artifacts?per_page=100\&page=1) case ${T64_MOCK_CASE:-} in retained-old-attempt) cat "$root/artifacts-attempt2.json" ;; wrong-current-attempt) jq ".artifacts[2].name |= sub(\"-2-gate-macos$\";\"-1-gate-macos\")" "$root/artifacts-attempt2.json" ;; missing-artifact) jq ".artifacts=[.artifacts[0]] | .total_count=1" "$root/artifacts.json" ;; duplicate-artifact) jq ".artifacts += [.artifacts[0] | .id=499] | .total_count=.artifacts|length" "$root/artifacts.json" ;; wrong-artifact-attempt) jq ".artifacts |= map(.name |= sub(\"-1-gate-\";\"-2-gate-\"))" "$root/artifacts.json" ;; wrong-artifact-run) jq ".artifacts[0].workflow_run.id=999" "$root/artifacts.json" ;; wrong-artifact-source) jq ".artifacts[0].workflow_run.head_sha=\"0000000000000000000000000000000000000000\"" "$root/artifacts.json" ;; expired-artifact) jq ".artifacts[0].expired=true" "$root/artifacts.json" ;; large-artifact) jq ".artifacts[0].size_in_bytes=65537" "$root/artifacts.json" ;; wrong-artifact-sha) jq ".artifacts[0].name |= sub(\"[0-9a-f]{40}\";\"0000000000000000000000000000000000000000\")" "$root/artifacts.json" ;; *) cat "$root/artifacts.json" ;; esac ;;' \
    '    *) echo "unexpected mock API endpoint: $endpoint" >&2; exit 2 ;;' \
    '  esac' \
    '  exit 0' \
    'fi' \
    '[ "${1:-}" = run ] && [ "${2:-}" = download ] || exit 2' \
    'shift 2; run=$1; shift; name=; dir=' \
    'while [ "$#" -gt 0 ]; do case $1 in --repo) shift 2 ;; --name) name=$2; shift 2 ;; --dir) dir=$2; shift 2 ;; *) exit 2 ;; esac; done' \
    '[ "$run" = 123 ] && [ -n "$name" ] && [ -n "$dir" ] || exit 2' \
    'case $name in *-gate-linux) file=gate-linux.json ;; *-gate-macos) file=gate-macos.json ;; *) exit 2 ;; esac' \
    'case ${T64_MOCK_CASE:-} in malformed-record) printf "{not-json" > "$dir/$file" ;; unknown-record-field) jq -cS ".unexpected=true" "$root/download/$name/$file" > "$dir/$file" ;; wrong-record-job) if [ "$file" = gate-linux.json ]; then cp "$root/macos.json" "$dir/$file"; else cp "$root/download/$name/$file" "$dir/$file"; fi ;; *) cp "$root/download/$name/$file" "$dir/$file" ;; esac' \
    'case ${T64_MOCK_CASE:-} in extra-file) printf "unexpected" > "$dir/unexpected.txt" ;; symlink) ln -s "$dir/$file" "$dir/unexpected-link" ;; esac' \
    > "$TMP/mockbin/gh"
  chmod 700 "$TMP/mockbin/gh"
  PATH="$TMP/mockbin:$PATH" T64_EVIDENCE_MOCK_ROOT="$TMP" \
    sh "$ROOT/scripts/collect-boot-evidence.sh" --collect --repo szTheory/tendies64 \
    --sha "$sha" --run 123 --attempt 1 --out "$TMP/collected" >/dev/null ||
    die "mocked exact-run collection failed for a valid pair"
  [ -f "$TMP/collected/gate-linux.json" ] && [ ! -L "$TMP/collected/gate-linux.json" ] || die "mocked collector did not publish Linux record"
  [ -f "$TMP/collected/gate-macos.json" ] && [ ! -L "$TMP/collected/gate-macos.json" ] || die "mocked collector did not publish macOS record"
  [ "$(find "$TMP/collected" -type f | wc -l | tr -d ' ')" = 2 ] || die "mocked collector did not publish exactly two records"
  cmp -s "$TMP/linux.json" "$TMP/collected/gate-linux.json" || die "mocked Linux record changed during collection"
  cmp -s "$TMP/macos.json" "$TMP/collected/gate-macos.json" || die "mocked macOS record changed during collection"
  PATH="$TMP/mockbin:$PATH" T64_EVIDENCE_MOCK_ROOT="$TMP" T64_MOCK_CASE=retained-old-attempt \
    sh "$ROOT/scripts/collect-boot-evidence.sh" --collect --repo szTheory/tendies64 \
    --sha "$sha" --run 123 --attempt 2 --out "$TMP/collected-attempt2" >/dev/null ||
    die "mocked exact-run collection rejected a valid attempt-2 pair with a retained attempt-1 artifact"
  [ "$(find "$TMP/collected-attempt2" -type f | wc -l | tr -d ' ')" = 2 ] || die "attempt-2 collection did not publish exactly two current records"
  cmp -s "$TMP/linux-attempt2.json" "$TMP/collected-attempt2/gate-linux.json" || die "attempt-2 Linux record changed during collection"
  cmp -s "$TMP/macos-attempt2.json" "$TMP/collected-attempt2/gate-macos.json" || die "attempt-2 macOS record changed during collection"
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
  expect_reject malformed-record "malformed downloaded JSON"
  expect_reject unknown-record-field "a record with an unknown wrapper field"
  expect_reject wrong-record-job "a replayed record for the other job"
  expect_reject extra-file "an artifact with an extra file"
  expect_reject symlink "an artifact containing a symlink"
  mkdir "$TMP/rejected"
  printf '%s\n' 'stale local bytes' > "$TMP/rejected/local.json"
  if PATH="$TMP/mockbin:$PATH" T64_EVIDENCE_MOCK_ROOT="$TMP" \
    sh "$ROOT/scripts/collect-boot-evidence.sh" --collect --repo szTheory/tendies64 \
    --sha "$sha" --run 123 --attempt 1 --out "$TMP/rejected" >/dev/null 2>&1; then
    die "collector selftest accepted pre-existing local output"
  fi
  [ -f "$TMP/rejected/local.json" ] || die "collector selftest altered pre-existing output"
  rejects=$((rejects + 1))
  echo "collect-boot-evidence: selftest passed (valid mocked pair; $rejects rejection cases; local records are not hosted evidence)"
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
