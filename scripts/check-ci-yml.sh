#!/bin/sh
# check-ci-yml.sh: static hardening checks for the CI workflow (VER-04, T-01-11 to T-01-13).
# Line-pattern awk, not a YAML parser, so it fails closed on anything it does not recognize.
#
# Usage: check-ci-yml.sh [--file PATH | --dir DIR] [--gate PATH] [--selftest]
#   --file PATH   one workflow file to check
#   --dir DIR     a workflow directory: every entry is checked (default: .github/workflows in
#                 the repository, so a second workflow file cannot escape the rules)
#   --gate PATH   script whose executable bit CI-GATE-EXEC checks (default scripts/gate.sh)
# Output: one line per violation, "RULE-ID path: message". Exit 0 clean, 1 violations, 2 tool error.
#
# Rules:
#   CI-RUN-LINE          only the exact gate and evidence commands are allowed in each job;
#                        no block scalars or expressions are allowed in run lines
#   CI-USES-PIN          every uses value, minus a trailing comment, ends in @ plus 40 lowercase hex
#   CI-RUNNER-LABEL      runs-on is exactly ubuntu-24.04 or macos-26
#   CI-LATEST-LABEL      no "-latest" runner label
#   CI-PR-TARGET         the privileged PR trigger never appears
#   CI-TRIGGER           the on: triggers are a subset of pull_request, push and workflow_dispatch;
#                        workflow_run, issue_comment, pull_request_review_comment and
#                        repository_dispatch never appear anywhere
#   CI-WORKFLOW-FILE     the workflow directory holds at least one .yml/.yaml file and nothing else
#   CI-PERMISSIONS-TOP   the top-level permissions value is the empty mapping
#   CI-PERMISSIONS-JOB   each job grants exactly contents: read
#   CI-PERSIST-CREDS     each checkout step sets persist-credentials: false
#   CI-FETCH-DEPTH       each checkout step sets fetch-depth: 0 (full-history rights scan)
#   CI-TIMEOUT           required gate jobs set timeout-minutes as an integer from 1 to 15 for
#                        gate-macos and from 1 to 10 for the other gate jobs
#   CI-STRESS-WORKFLOW   the separate authored-only stress workflow matches its reviewed
#                        exact contract: weekly/dispatch, no uploads, 15-minute job
#   CI-REQUIRED-STEP     jobs and steps cannot weaken required success semantics
#   CI-SECRETS           no secrets reference in any spelling (secrets.X, secrets['X'], secrets[...],
#                        toJSON(secrets)) and no token (github.token, github['token'], toJSON(github))
#   CI-CACHE-RESTORE     each gate job restores the pinned Binutils install directory with exactly one
#                        reviewed SHA-pinned actions/cache step (exact path and key, no other inputs,
#                        no other keys on the step, no other cache action), strictly after Checkout
#                        and before Gate
#   CI-UPLOAD-ARTIFACT   both existing gate jobs upload one exact reviewed list of sanitized evidence
#                        files (boot, raw RDP VI, audio and Phase 9 manifests) after every scan;
#                        the upload step's path is the only block scalar admitted
#   CI-EXPR-IN-RUN       no expression in a run line
#   CI-GATE-EXEC         the gate script is executable
#   CI-ZERO-JOBS         the workflow defines no jobs
set -eu
LC_ALL=C
export LC_ALL

SELF=$0
ROOT=$(cd "$(dirname "$0")/.." && pwd -P)
ALL_RULES="CI-RUN-LINE CI-USES-PIN CI-RUNNER-LABEL CI-LATEST-LABEL CI-PR-TARGET CI-TRIGGER CI-WORKFLOW-FILE CI-PERMISSIONS-TOP CI-PERMISSIONS-JOB CI-PERSIST-CREDS CI-FETCH-DEPTH CI-TIMEOUT CI-REQUIRED-STEP CI-SECRETS CI-UPLOAD-ARTIFACT CI-CACHE-RESTORE CI-EXPR-IN-RUN CI-GATE-EXEC CI-ZERO-JOBS CI-STRESS-WORKFLOW"

usage() {
  echo "usage: check-ci-yml.sh [--file PATH | --dir DIR] [--gate PATH] [--selftest]" >&2
}

# scan_yml FILE: print violations from the line-pattern checks.
scan_yml() {
  # This is a separate, exact reviewed workflow contract rather than a relaxation
  # of the two required gate jobs. Any byte change requires review and rebinding.
  # In particular its commands cannot execute external assets or upload output.
  case "${1##*/}" in
    rsp-stress.yml)
      _stress_sum=$(shasum -a 256 "$1") || return 2
      _stress_sum=${_stress_sum%% *}
      if [ "$_stress_sum" != "00a9e2303d10798fb407a92e79b9ce69bf6af71a87e64755c83ac89c3a1576c7" ]; then
        echo "CI-STRESS-WORKFLOW $1: authored stress workflow differs from its reviewed contract"
      fi
      return 0
      ;;
  esac
  awk -v file="$1" '
    function viol(rule, msg) { printf "%s %s: line %d: %s\n", rule, file, NR, msg; nviol++ }
    function vjob(rule, msg) { printf "%s %s: job %s: %s\n", rule, file, job, msg; nviol++ }
    function closestep() {
      if (ischeckout) {
        if (!pc) { printf "CI-PERSIST-CREDS %s: job %s: checkout step lacks persist-credentials: false\n", file, job; nviol++ }
        if (!fd) { printf "CI-FETCH-DEPTH %s: job %s: checkout step lacks fetch-depth: 0\n", file, job; nviol++ }
      }
      ischeckout = 0; pc = 0; fd = 0
    }
    function expected_upload_path(j,    b) {
      b = "build/evidence/combined/" j
      return b "/" j ".json|" b "/rdp-vi/" j "/manifest.json|" b "/rdp-vi/" j "/inventory.json|" \
             b "/rdp-vi/" j "/observation.json|" b "/rdp-vi/" j "/raw-export.json|" \
             b "/audio/" j "/manifest.json|" b "/phase09/" j "/manifest.json"
    }
    function expected_cache_pin() {
      return "actions/cache@55cc8345863c7cc4c66a329aec7e433d2d1c52a9"
    }
    function expected_cache_key() {
      return "binutils-${{ runner.os }}-${{ runner.arch }}-${{ hashFiles(\047scripts/build-boot-fixtures.sh\047) }}"
    }
    function expected_artifact_name() {
      return "t64-authored-evidence-${{ github.sha }}-${{ github.run_id }}-${{ github.run_attempt }}-${{ github.job }}"
    }
    function closejob() {
      if (job != "") {
        closestep()
        if (!hascontents) vjob("CI-PERMISSIONS-JOB", "permissions must grant contents: read")
        if (!hastimeout) vjob("CI-TIMEOUT", "timeout-minutes is missing")
        if (!hasruns) vjob("CI-RUNNER-LABEL", "runs-on is missing")
        if (job_control) vjob("CI-REQUIRED-STEP", "job-level if and continue-on-error controls are forbidden")
        if (job == "gate-linux" && job_runs_on != "ubuntu-24.04")
          vjob("CI-RUNNER-LABEL", "gate-linux must run on ubuntu-24.04")
        if (job == "gate-macos" && job_runs_on != "macos-26")
          vjob("CI-RUNNER-LABEL", "gate-macos must run on macos-26")
        if (job_runs != 12 || gate_runs != 1 || audio_case_runs != 1 || audio_emit_runs != 1 ||
            p9_case_runs != 1 || p9_emit_runs != 1 ||
            raw_emit_runs != 1 || emit_runs != 1 || rights_runs != 1 || pii_runs != 1 ||
            raw_pii_runs != 1 || audio_pii_runs != 1 || p9_pii_runs != 1)
          vjob("CI-RUN-LINE", "job must run the gate and each exact evidence command once")
        if (step_count != 15 || checkout_step_count != 1 || cache_step_count != 1 || gate_step_count != 1 ||
            audio_case_step_count != 1 || audio_emit_step_count != 1 ||
            p9_case_step_count != 1 || p9_emit_step_count != 1 ||
            raw_emit_step_count != 1 || emit_step_count != 1 || rights_step_count != 1 ||
            pii_step_count != 1 || raw_pii_step_count != 1 || audio_pii_step_count != 1 ||
            p9_pii_step_count != 1 ||
            upload_step_count != 1 ||
            !(checkout_step_line < cache_step_line && cache_step_line < gate_step_line &&
              gate_step_line < audio_case_step_line &&
              audio_case_step_line < p9_case_step_line && p9_case_step_line < audio_emit_step_line &&
              audio_emit_step_line < p9_emit_step_line && p9_emit_step_line < raw_emit_step_line &&
              raw_emit_step_line < emit_step_line && emit_step_line < rights_step_line &&
              rights_step_line < pii_step_line && pii_step_line < raw_pii_step_line &&
              raw_pii_step_line < audio_pii_step_line && audio_pii_step_line < p9_pii_step_line &&
              p9_pii_step_line < upload_step_line) ||
            !(gate_run_line > gate_step_line && gate_run_line < audio_case_step_line &&
              audio_case_run_line > audio_case_step_line && audio_case_run_line < p9_case_step_line &&
              p9_case_run_line > p9_case_step_line && p9_case_run_line < audio_emit_step_line &&
              audio_emit_run_line > audio_emit_step_line && audio_emit_run_line < p9_emit_step_line &&
              p9_emit_run_line > p9_emit_step_line && p9_emit_run_line < raw_emit_step_line &&
              raw_emit_run_line > raw_emit_step_line && raw_emit_run_line < emit_step_line &&
              emit_run_line > emit_step_line && emit_run_line < rights_step_line &&
              rights_run_line > rights_step_line && rights_run_line < pii_step_line &&
              pii_run_line > pii_step_line && pii_run_line < raw_pii_step_line &&
              raw_pii_run_line > raw_pii_step_line && raw_pii_run_line < audio_pii_step_line &&
              audio_pii_run_line > audio_pii_step_line && audio_pii_run_line < p9_pii_step_line &&
              p9_pii_run_line > p9_pii_step_line && p9_pii_run_line < upload_step_line))
          vjob("CI-UPLOAD-ARTIFACT", "gate, audio, Phase 9 and boot/RDP emitters, staged scans, and upload steps must be uniquely named and ordered")
        if (cache_step_count != 1 || cache_actions != 1 || cache_pin_count != 1 || cache_with_count != 1 ||
            cache_path_count != 1 || cache_key_count != 1)
          vjob("CI-CACHE-RESTORE", "job must restore exactly one reviewed Binutils cache: one cache step, one pinned action, one with block, one path, one key")
        if (!(checkout_step_line < cache_step_line && cache_step_line < gate_step_line))
          vjob("CI-CACHE-RESTORE", "the Binutils cache restore must come strictly after Checkout and before Gate")
        if (upload_actions != 1 || upload_name_count != 1 ||
            upload_path_count != 1 || upload_missing_count != 1)
          vjob("CI-UPLOAD-ARTIFACT", "job must upload exactly one scoped record with exact name, path, and error-on-missing")
        if (job == "gate-linux") {
          linux_jobs++
          if (upload_path != expected_upload_path("gate-linux"))
            vjob("CI-UPLOAD-ARTIFACT", "gate-linux may upload only its exact reviewed authored evidence file list")
        } else if (job == "gate-macos") {
          macos_jobs++
          if (upload_path != expected_upload_path("gate-macos"))
            vjob("CI-UPLOAD-ARTIFACT", "gate-macos may upload only its exact reviewed authored evidence file list")
        } else {
          vjob("CI-UPLOAD-ARTIFACT", "only gate-linux and gate-macos jobs are permitted")
        }
      }
      job = ""
    }
    function trigger(t) {
      t = clean(t); gsub(/^["\047]|["\047]$/, "", t)
      if (t == "") return
      if (t != "pull_request" && t != "push" && t != "workflow_dispatch")
        viol("CI-TRIGGER", "trigger " t " is not one of pull_request, push, workflow_dispatch")
    }
    function clean(v) {
      sub(/[ \t]+#.*$/, "", v); sub(/^[ \t]+/, "", v); sub(/[ \t]+$/, "", v)
      return v
    }
    BEGIN { job = ""; injobs = 0; inon = 0 }
    { raw = $0; sub(/\r$/, "", raw) }
    raw ~ /^[ \t]*(#.*)?$/ { next }
    {
      line = raw
      match(line, /^ */); ind = RLENGTH
      body = substr(line, ind + 1)
      isitem = 0
      if (body ~ /^- /) { isitem = 1; sub(/^-[ \t]+/, "", body) }

      # Whole-line patterns.
      if (line ~ /pull_request_target/) viol("CI-PR-TARGET", "the privileged pull_request_target trigger is forbidden")
      if (line ~ /-latest/) viol("CI-LATEST-LABEL", "a -latest runner label is forbidden; use an explicit label")
      if (line ~ /(workflow_run|issue_comment|pull_request_review_comment|repository_dispatch)/) viol("CI-TRIGGER", "a privileged trigger name is forbidden")
      if (line ~ /(^|[^A-Za-z0-9_])secrets([^A-Za-z0-9_]|$)/ ||
          line ~ /github[ \t]*(\.[ \t]*token|\[[ \t]*["\047][ \t]*token[ \t]*["\047][ \t]*\])/ ||
          line ~ /[(,][ \t]*github[ \t]*[),]/) viol("CI-SECRETS", "secrets and the github token must not be referenced")
      if (line ~ /:[ \t]+[|>][-+0-9]*[ \t]*$/ &&
          !(ind == 10 && body ~ /^path:[ \t]+\|[ \t]*$/ && in_upload_with && upload_action))
        viol("CI-RUN-LINE", "block scalars are forbidden")

      key = body; sub(/[ \t]*:.*/, "", key)
      field = key; gsub(/^["\047]|["\047]$/, "", field)
      val = ""
      p = index(body, ":")
      if (p > 0) val = clean(substr(body, p + 1))

      # The admitted upload path block: plain, non-item lines indented under path:.
      if (in_path_block) {
        if (ind >= 12 && !isitem) { upload_path = (upload_path == "" ? body : upload_path "|" body); next }
        in_path_block = 0
      }

      if (body ~ /^run[ \t]*:/) {
        nruns++; job_runs++
        if (val == "./scripts/gate.sh") { gate_runs++; gate_run_line = NR }
        else if (val == "python3 -B scripts/check-audio-cases.py --build build/gate --suite qualification --run --artifact-prefix \"08-05-${GITHUB_JOB}-${GITHUB_RUN_ID}-${GITHUB_RUN_ATTEMPT}-${GITHUB_SHA}\"") { audio_case_runs++; audio_case_run_line = NR }
        else if (val == "python3 -B scripts/qualify-audio.py --emit-hosted --job \"$GITHUB_JOB\" --sha \"$GITHUB_SHA\" --run \"$GITHUB_RUN_ID\" --attempt \"$GITHUB_RUN_ATTEMPT\" --build build/gate --suite qualification --artifact-prefix \"08-05-${GITHUB_JOB}-${GITHUB_RUN_ID}-${GITHUB_RUN_ATTEMPT}-${GITHUB_SHA}\" --out \"build/evidence/combined/$GITHUB_JOB/audio/$GITHUB_JOB\"") { audio_emit_runs++; audio_emit_run_line = NR }
        else if (val == "python3 -B scripts/qualify-audio.py --scan-pii --job \"$GITHUB_JOB\" --sha \"$GITHUB_SHA\" --run \"$GITHUB_RUN_ID\" --attempt \"$GITHUB_RUN_ATTEMPT\" --out \"build/evidence/combined/$GITHUB_JOB/audio/$GITHUB_JOB\"") { audio_pii_runs++; audio_pii_run_line = NR }
        else if (val == "python3 -B scripts/check-phase09-cases.py --build build/gate --suite hosted --run --artifact-prefix \"09-hosted-${GITHUB_JOB}-${GITHUB_RUN_ID}-${GITHUB_RUN_ATTEMPT}-${GITHUB_SHA}\"") { p9_case_runs++; p9_case_run_line = NR }
        else if (val == "python3 -B scripts/qualify-phase09.py --emit-hosted --job \"$GITHUB_JOB\" --sha \"$GITHUB_SHA\" --run \"$GITHUB_RUN_ID\" --attempt \"$GITHUB_RUN_ATTEMPT\" --build build/gate --suite hosted --artifact-prefix \"09-hosted-${GITHUB_JOB}-${GITHUB_RUN_ID}-${GITHUB_RUN_ATTEMPT}-${GITHUB_SHA}\" --out \"build/evidence/combined/$GITHUB_JOB/phase09/$GITHUB_JOB\"") { p9_emit_runs++; p9_emit_run_line = NR }
        else if (val == "python3 -B scripts/qualify-phase09.py --scan-pii --job \"$GITHUB_JOB\" --sha \"$GITHUB_SHA\" --run \"$GITHUB_RUN_ID\" --attempt \"$GITHUB_RUN_ATTEMPT\" --out \"build/evidence/combined/$GITHUB_JOB/phase09/$GITHUB_JOB\"") { p9_pii_runs++; p9_pii_run_line = NR }
        else if (val == "python3 -B scripts/qualify-rdp-vi.py --emit-hosted --job \"$GITHUB_JOB\" --sha \"$GITHUB_SHA\"") { raw_emit_runs++; raw_emit_run_line = NR }
        else if (val == "sh scripts/collect-boot-evidence.sh --emit") { emit_runs++; emit_run_line = NR }
        else if (val == "sh scripts/rights-scan.sh --staging build/evidence") { rights_runs++; rights_run_line = NR }
        else if (val == "sh scripts/collect-boot-evidence.sh --scan-pii") { pii_runs++; pii_run_line = NR }
        else if (val == "python3 -B scripts/qualify-rdp-vi.py --scan-pii") { raw_pii_runs++; raw_pii_run_line = NR }
        else viol("CI-RUN-LINE", "run lines must use the canonical gate and exact evidence commands")
        if (line ~ /\$\{\{/) viol("CI-EXPR-IN-RUN", "an expression in a run line is an injection path")
      }

      # Triggers: the on: mapping, list or scalar (quoted key spellings too).
      if (inon && ind > 0) {
        if (ind == 2 && isitem) trigger(body)
        else if (ind == 2) trigger(key)
        next
      }

      # Top level.
      if (ind == 0 && !isitem) {
        inon = 0
        k2 = key; gsub(/^["\047]|["\047]$/, "", k2)
        if (k2 == "on" || k2 == "true") {
          inon = 1
          if (val != "") {
            v2 = val; gsub(/^\[|\]$/, "", v2)
            nt = split(v2, tl, ",")
            for (ti = 1; ti <= nt; ti++) trigger(tl[ti])
          }
        }
        if (key == "jobs") { injobs = 1; sawjobs = 1 }
        else { closejob(); injobs = 0 }
        if (key == "permissions") { sawtop = 1; topok = (val == "{}") }
        next
      }

      # Job header: two-space key directly under jobs.
      if (injobs && ind == 2 && !isitem && body ~ /^[A-Za-z0-9_-]+:[ \t]*(#.*)?$/) {
        closejob()
        job = key; njobs++
        hascontents = 0; hastimeout = 0; hasruns = 0; inperm = 0
        job_control = 0; job_runs_on = ""
        job_runs = 0; gate_runs = 0; raw_emit_runs = 0; emit_runs = 0
        rights_runs = 0; pii_runs = 0; raw_pii_runs = 0
        audio_case_runs = 0; audio_emit_runs = 0; audio_pii_runs = 0
        p9_case_runs = 0; p9_emit_runs = 0; p9_pii_runs = 0
        step_count = 0; checkout_step_count = 0; gate_step_count = 0
        raw_emit_step_count = 0; emit_step_count = 0; rights_step_count = 0
        pii_step_count = 0; raw_pii_step_count = 0
        audio_case_step_count = 0; audio_emit_step_count = 0; audio_pii_step_count = 0
        p9_case_step_count = 0; p9_emit_step_count = 0; p9_pii_step_count = 0
        in_path_block = 0
        cache_step_count = 0; cache_actions = 0; cache_pin_count = 0; cache_with_count = 0
        cache_path_count = 0; cache_key_count = 0; cache_step_line = 0
        in_cache_step = 0; in_cache_with = 0
        upload_step_count = 0; upload_actions = 0
        upload_name_count = 0; upload_path_count = 0; upload_missing_count = 0
        upload_name = ""; upload_path = ""; upload_action = 0; in_upload_with = 0
        current_step = ""
        checkout_step_line = 0; gate_step_line = 0; raw_emit_step_line = 0
        emit_step_line = 0; rights_step_line = 0; pii_step_line = 0
        raw_pii_step_line = 0; upload_step_line = 0
        audio_case_step_line = 0; audio_emit_step_line = 0; audio_pii_step_line = 0
        p9_case_step_line = 0; p9_emit_step_line = 0; p9_pii_step_line = 0
        gate_run_line = 0; raw_emit_run_line = 0; emit_run_line = 0
        rights_run_line = 0; pii_run_line = 0; raw_pii_run_line = 0
        audio_case_run_line = 0; audio_emit_run_line = 0; audio_pii_run_line = 0
        p9_case_run_line = 0; p9_emit_run_line = 0; p9_pii_run_line = 0
        next
      }

      if (injobs && job != "") {
        if (ind == 4 && !isitem) {
          inperm = (field == "permissions" && val == "")
          if (field == "if" || field == "continue-on-error") job_control = 1
          if (field == "timeout-minutes") {
            tmax = (job == "gate-macos" ? 15 : 10)
            if (val ~ /^[0-9]+$/ && val + 0 >= 1 && val + 0 <= tmax) hastimeout = 1
            else viol("CI-TIMEOUT", "timeout-minutes must be an integer from 1 to 15 for gate-macos and 1 to 10 for other gate jobs")
          }
          if (field == "runs-on") {
            hasruns = 1
            job_runs_on = val
            if (val != "ubuntu-24.04" && val != "macos-26") viol("CI-RUNNER-LABEL", "runs-on must be exactly ubuntu-24.04 or macos-26, found: " val)
          }
        } else if (ind == 6 && !isitem && inperm) {
          if (clean(body) == "contents: read") hascontents = 1
          else viol("CI-PERMISSIONS-JOB", "a job may grant only contents: read, found: " clean(body))
        }
        if (ind == 6 && isitem) {
          closestep()
          step_count++
          current_step = (key == "name" ? val : "")
          upload_action = 0; in_upload_with = 0
          in_cache_step = (current_step == "Restore pinned Binutils"); in_cache_with = 0
          if (in_cache_step) { cache_step_count++; cache_step_line = NR }
          if (current_step == "Checkout") { checkout_step_count++; checkout_step_line = NR }
          else if (current_step == "Gate") { gate_step_count++; gate_step_line = NR }
          else if (current_step == "Run authored audio qualification records") { audio_case_step_count++; audio_case_step_line = NR }
          else if (current_step == "Emit authored audio evidence") { audio_emit_step_count++; audio_emit_step_line = NR }
          else if (current_step == "PII scan authored audio evidence") { audio_pii_step_count++; audio_pii_step_line = NR }
          else if (current_step == "Run authored Phase 9 qualification records") { p9_case_step_count++; p9_case_step_line = NR }
          else if (current_step == "Emit authored Phase 9 evidence") { p9_emit_step_count++; p9_emit_step_line = NR }
          else if (current_step == "PII scan authored Phase 9 evidence") { p9_pii_step_count++; p9_pii_step_line = NR }
          else if (current_step == "Emit authored raw RDP VI evidence") { raw_emit_step_count++; raw_emit_step_line = NR }
          else if (current_step == "Emit guest evidence") { emit_step_count++; emit_step_line = NR }
          else if (current_step == "Rights scan sanitized output") { rights_step_count++; rights_step_line = NR }
          else if (current_step == "PII scan sanitized output") { pii_step_count++; pii_step_line = NR }
          else if (current_step == "PII scan authored raw RDP VI evidence") { raw_pii_step_count++; raw_pii_step_line = NR }
          else if (current_step == "Upload sanitized authored evidence") { upload_step_count++; upload_step_line = NR }
        }
        if (ind == 8 && !isitem && (field == "if" || field == "continue-on-error"))
          vjob("CI-REQUIRED-STEP", "step-level if and continue-on-error controls are forbidden")
        if (ind == 8 && !isitem) in_upload_with = (key == "with" && val == "")
        # The cache step may hold only its name, its pinned uses and one with block of path and key.
        if (in_cache_step && !(ind == 6 && isitem)) {
          if (ind == 8) {
            in_cache_with = 0
            if (isitem || (field != "uses" && field != "with"))
              viol("CI-CACHE-RESTORE", "the cache step may contain only name, uses and with")
            else if (field == "with") {
              cache_with_count++
              if (val == "") in_cache_with = 1
              else viol("CI-CACHE-RESTORE", "the cache step with must be a block mapping of path and key")
            }
          } else if (ind == 10 && in_cache_with && !isitem) {
            if (field == "path") {
              cache_path_count++
              if (val != "build/tools/install") viol("CI-CACHE-RESTORE", "the cache path must be exactly build/tools/install")
            } else if (field == "key") {
              cache_key_count++
              if (val != expected_cache_key()) viol("CI-CACHE-RESTORE", "the cache key must be exactly the reviewed runner OS, architecture and build script hash key")
            } else viol("CI-CACHE-RESTORE", "the cache step with may contain only path and key")
          } else if (ind > 8) {
            viol("CI-CACHE-RESTORE", "unexpected content in the cache step")
          }
        }
        if (ind == 10 && !isitem && in_upload_with && upload_action) {
          if (key == "name") {
            upload_name_count++; upload_name = val
            if (val != expected_artifact_name())
              vjob("CI-UPLOAD-ARTIFACT", "artifact name must bind the exact SHA, run, attempt, and job")
          } else if (key == "path") {
            upload_path_count++
            if (val == "|") { upload_path = ""; in_path_block = 1 }
            else upload_path = val
          } else if (key == "if-no-files-found") {
            upload_missing_count++
            if (val != "error") vjob("CI-UPLOAD-ARTIFACT", "upload must fail when its evidence directory is absent")
          } else {
            vjob("CI-UPLOAD-ARTIFACT", "upload may contain only name, path, and if-no-files-found")
          }
        }
      }

      if (key == "uses") {
        n = split(val, parts, "@")
        sha = parts[n]
        if (n < 2 || length(sha) != 40 || sha !~ /^[0-9a-f]+$/) viol("CI-USES-PIN", "uses must end in @ plus a 40-character lowercase hex SHA: " val)
        if (val ~ /^actions\/checkout@/) ischeckout = 1
        if (tolower(val) ~ /cache/) {
          cache_actions++
          if (current_step != "Restore pinned Binutils")
            viol("CI-CACHE-RESTORE", "a cache action is permitted only in the Restore pinned Binutils step: " val)
          else if (val != expected_cache_pin())
            viol("CI-CACHE-RESTORE", "the cache step must use exactly the verified actions/cache v6.1.0 full commit pin")
          else cache_pin_count++
        }
        if (val ~ /upload(-pages)?-artifact/) {
          upload_actions++
          upload_action = 1
          if (val != "actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02")
            vjob("CI-UPLOAD-ARTIFACT", "upload-artifact must use the verified v4.6.2 full commit pin")
          if (current_step != "Upload sanitized authored evidence")
            vjob("CI-UPLOAD-ARTIFACT", "upload-artifact must be the final named job step")
        }
      }
      if (key == "persist-credentials") pc = (val == "false")
      if (key == "fetch-depth") fd = (val == "0")
    }
    END {
      closejob()
      if (!sawtop || !topok) { printf "CI-PERMISSIONS-TOP %s: the top-level permissions value must be the empty mapping {}\n", file; nviol++ }
      if (njobs == 0) { printf "CI-ZERO-JOBS %s: the workflow defines no jobs\n", file; nviol++ }
      else {
        if (njobs != 2 || linux_jobs != 1 || macos_jobs != 1) {
          printf "CI-UPLOAD-ARTIFACT %s: exactly one gate-linux and one gate-macos job are required\n", file; nviol++
        }
        if (nruns != 24) {
          printf "CI-RUN-LINE %s: expected twenty-four gate/evidence commands across two jobs, found %d\n", file, nruns; nviol++
        }
      }
    }
  ' "$1"
}

# ---- selftest -------------------------------------------------------------------------------

FAILS=0
CASES=0

# expect RULE DESCRIPTION FILE [GATE [--dir]]: the checker must exit 1 and print
# the rule id. With --dir, FILE is a workflow directory.
expect() {
  CASES=$((CASES + 1))
  _g=${4:-$ROOT/scripts/gate.sh}
  [ -n "$_g" ] || _g=$ROOT/scripts/gate.sh
  _how=--file
  [ "${5:-}" != --dir ] || _how=--dir
  _out=$(sh "$SELF" $_how "$3" --gate "$_g" 2>&1) && _rc=0 || _rc=$?
  case "$_out" in
    *"$1 "*)
      if [ "$_rc" -eq 1 ]; then echo "ok $1: $2"; else echo "FAIL $1: $2 (exit $_rc, expected 1)"; FAILS=$((FAILS + 1)); fi
      ;;
    *) echo "FAIL $1: $2 (rule id not printed; exit $_rc)"; FAILS=$((FAILS + 1)) ;;
  esac
  SEEN="$SEEN $1"
}

# mut NAME SED-EXPR: write a mutated copy of the real workflow to $W/NAME.yml (never in place).
mut() {
  sed "$2" "$REAL" > "$W/$1.yml"
}

inject_step_key() {
  awk -v target="$1" -v field="$2" -v value="$3" '
    $0 == "      - name: " target { print; print "        " field ": " value; found = 1; next }
    { print }
    END { if (!found) exit 2 }
  ' "$REAL" > "$W/$4.yml"
}

inject_job_key() {
  awk -v target="$1" -v field="$2" -v value="$3" '
    $0 == "  " target ":" { print; print "    " field ": " value; found = 1; next }
    { print }
    END { if (!found) exit 2 }
  ' "$REAL" > "$W/$4.yml"
}

# mut_timeout JOB VALUE NAME: a copy of the real workflow whose JOB has timeout-minutes VALUE.
mut_timeout() {
  awk -v target="$1" -v value="$2" '
    /^  [A-Za-z0-9_-]+:[ \t]*$/ { injob = ($1 == target ":") }
    injob && /^    timeout-minutes:/ { print "    timeout-minutes: " value; found = 1; next }
    { print }
    END { if (!found) exit 2 }
  ' "$REAL" > "$W/$3.yml"
}

selftest() {
  REAL=$ROOT/.github/workflows/ci.yml
  if [ ! -f "$REAL" ]; then echo "check-ci-yml: workflow not found: $REAL" >&2; return 1; fi
  W=$(mktemp -d "${TMPDIR:-/tmp}/t64ci.XXXXXX")
  trap 'rm -rf "$W"' EXIT HUP INT TERM
  SEEN=""

  cp "$REAL" "$W/clean.yml"
  CASES=$((CASES + 1))
  if _out=$(sh "$SELF" --file "$W/clean.yml" 2>&1); then echo "ok clean: the unmodified copy passes"
  else echo "FAIL clean: the unmodified copy was rejected: $_out"; FAILS=$((FAILS + 1)); fi

  # The independent lane is admitted only with its complete reviewed bytes.
  STRESS=$ROOT/.github/workflows/rsp-stress.yml
  cp "$STRESS" "$W/rsp-stress.yml"
  CASES=$((CASES + 1))
  if _out=$(sh "$SELF" --file "$W/rsp-stress.yml" 2>&1); then echo "ok stress-clean: reviewed authored stress passes"
  else echo "FAIL stress-clean: $_out"; FAILS=$((FAILS + 1)); fi
  for _mutation in timeout trigger command permission action upload; do
    case "$_mutation" in
      timeout) sed 's/timeout-minutes: 15/timeout-minutes: 16/' "$STRESS" > "$W/rsp-stress.yml" ;;
      trigger) sed 's/workflow_dispatch:/pull_request_target:/' "$STRESS" > "$W/rsp-stress.yml" ;;
      command) sed 's/--authored/--external/' "$STRESS" > "$W/rsp-stress.yml" ;;
      permission) sed 's/contents: read/contents: write/' "$STRESS" > "$W/rsp-stress.yml" ;;
      action) sed 's|actions/checkout@[0-9a-f]*|actions/checkout@main|' "$STRESS" > "$W/rsp-stress.yml" ;;
      upload) { cat "$STRESS"; printf '\n      - uses: actions/upload-artifact@main\n'; } > "$W/rsp-stress.yml" ;;
    esac
    expect CI-STRESS-WORKFLOW "stress $_mutation mutation" "$W/rsp-stress.yml"
  done

  mut run-extra 's|run: ./scripts/gate.sh|run: ./scripts/gate.sh \&\& echo extra|'
  expect CI-RUN-LINE "a run line with extra commands" "$W/run-extra.yml"
  awk '/run: .\/scripts\/gate.sh/ && !done { print "        run: |"; print "          ./scripts/gate.sh"; done = 1; next } { print }' "$REAL" > "$W/run-block.yml"
  expect CI-RUN-LINE "a block scalar" "$W/run-block.yml"
  awk '/- name: Gate/ && !done { print; getline; print; print "      - run: echo second"; done = 1; next } { print }' "$REAL" > "$W/run-count.yml"
  expect CI-RUN-LINE "a second run line" "$W/run-count.yml"

  mut uses-tag 's|actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1  # v7.0.1|actions/checkout@v4  # v4|'
  expect CI-USES-PIN "a tag instead of a SHA" "$W/uses-tag.yml"
  mut uses-short 's|@3d3c42e5aac5ba805825da76410c181273ba90b1|@3d3c42e5aac5ba805825da76410c181273ba90b|'
  expect CI-USES-PIN "a 39-character SHA" "$W/uses-short.yml"

  mut runner 's|ubuntu-24.04|ubuntu-22.04|'
  expect CI-RUNNER-LABEL "an unlisted runner label" "$W/runner.yml"
  awk '!changed && /^    runs-on: ubuntu-24.04$/ { sub(/ubuntu-24.04$/, "macos-26"); changed = 1 } { print } END { if (!changed) exit 2 }' "$REAL" > "$W/runner-linux-macos.yml"
  expect CI-RUNNER-LABEL "gate-linux assigned to macOS" "$W/runner-linux-macos.yml"
  awk '!changed && /^    runs-on: macos-26$/ { sub(/macos-26$/, "ubuntu-24.04"); changed = 1 } { print } END { if (!changed) exit 2 }' "$REAL" > "$W/runner-macos-linux.yml"
  expect CI-RUNNER-LABEL "gate-macos assigned to Linux" "$W/runner-macos-linux.yml"
  mut latest 's|macos-26|macos-latest|'
  expect CI-LATEST-LABEL "a -latest label" "$W/latest.yml"
  mut prtarget 's|^  pull_request:|  pull_request_target:|'
  expect CI-PR-TARGET "the privileged PR trigger" "$W/prtarget.yml"
  mut permtop 's|^permissions: {}|permissions: write-all|'
  expect CI-PERMISSIONS-TOP "a non-empty top-level permissions value" "$W/permtop.yml"
  mut permjob 's|      contents: read|      contents: write|'
  expect CI-PERMISSIONS-JOB "a job that grants contents: write" "$W/permjob.yml"
  mut persist 's|persist-credentials: false|persist-credentials: true|'
  expect CI-PERSIST-CREDS "persisted credentials" "$W/persist.yml"
  mut fetch 's|fetch-depth: 0|fetch-depth: 1|'
  expect CI-FETCH-DEPTH "a shallow checkout" "$W/fetch.yml"
  grep -v 'timeout-minutes' "$REAL" > "$W/timeout.yml" || true
  expect CI-TIMEOUT "a job without a timeout" "$W/timeout.yml"
  # The bound is per job: the clean copy above passes with gate-macos at its limit of 15 and
  # gate-linux at its limit of 10; each case below moves one job's value across its own limit
  # (the only CI-TIMEOUT violation of the copy), so a global bound of 10 (rejects the clean
  # copy), of 15 (accepts linux-11), or a missing lower or integer check fails a case.
  mut_timeout gate-macos 16 timeout-macos-16
  expect CI-TIMEOUT "gate-macos timeout-minutes 16, above its limit of 15" "$W/timeout-macos-16.yml"
  mut_timeout gate-linux 11 timeout-linux-11
  expect CI-TIMEOUT "gate-linux timeout-minutes 11, above its limit of 10" "$W/timeout-linux-11.yml"
  mut_timeout gate-macos 0 timeout-macos-0
  expect CI-TIMEOUT "gate-macos timeout-minutes 0, below the minimum of 1" "$W/timeout-macos-0.yml"
  mut_timeout gate-macos 15.5 timeout-macos-fraction
  expect CI-TIMEOUT "gate-macos timeout-minutes 15.5, not an integer" "$W/timeout-macos-fraction.yml"
  mut_timeout gate-macos 14.5 timeout-macos-fraction-inside
  expect CI-TIMEOUT "gate-macos timeout-minutes 14.5, not an integer although below the limit" "$W/timeout-macos-fraction-inside.yml"

  # Required jobs and steps cannot be skipped or have failures softened.
  for _step in "Restore pinned Binutils" "Gate" "Run authored audio qualification records" "Emit authored audio evidence" \
    "Run authored Phase 9 qualification records" "Emit authored Phase 9 evidence" \
    "Emit authored raw RDP VI evidence" "Emit guest evidence" \
    "Rights scan sanitized output" "PII scan sanitized output" \
    "PII scan authored raw RDP VI evidence" "PII scan authored audio evidence" \
    "PII scan authored Phase 9 evidence" \
    "Upload sanitized authored evidence"; do
    _label=$(printf '%s' "$_step" | tr '[:upper:] ' '[:lower:]-')
    inject_step_key "$_step" continue-on-error true "soft-$_label"
    expect CI-REQUIRED-STEP "$_step with continue-on-error enabled" "$W/soft-$_label.yml"
    inject_step_key "$_step" if false "skip-$_label"
    expect CI-REQUIRED-STEP "$_step with a skip condition" "$W/skip-$_label.yml"
  done
  inject_step_key "Gate" '"if"' false soft-quoted-if
  expect CI-REQUIRED-STEP "a quoted step-level skip key" "$W/soft-quoted-if.yml"
  inject_job_key gate-linux continue-on-error true job-soft
  expect CI-REQUIRED-STEP "a job-level continue-on-error control" "$W/job-soft.yml"
  inject_job_key gate-linux if false job-skip
  expect CI-REQUIRED-STEP "a job-level skip condition" "$W/job-skip.yml"
  inject_job_key gate-linux '"continue-on-error"' true job-quoted-soft
  expect CI-REQUIRED-STEP "a quoted job-level continue-on-error key" "$W/job-quoted-soft.yml"
  inject_job_key gate-linux '"if"' false job-quoted-skip
  expect CI-REQUIRED-STEP "a quoted job-level skip key" "$W/job-quoted-skip.yml"

  awk '/timeout-minutes: 10/ && !done { print; print "    env:"; print "      TOKEN: ${{ secrets.TOKEN }}"; done = 1; next } { print }' "$REAL" > "$W/secrets.yml"
  expect CI-SECRETS "a secrets reference" "$W/secrets.yml"
  awk '/^    steps:/ && !done { print; print "      - uses: actions/upload-artifact@cf430e030ddbb5b0abf93d22962f4752f3646cd9  # v7.0.2"; done = 1; next } { print }' "$REAL" > "$W/upload.yml"
  expect CI-UPLOAD-ARTIFACT "an upload-artifact step" "$W/upload.yml"
  mut upload-pin 's|@ea165f8d65b6e75b540449e92b4886f43607fa02|@0000000000000000000000000000000000000000|'
  expect CI-UPLOAD-ARTIFACT "an unverified upload-artifact pin" "$W/upload-pin.yml"
  mut upload-name 's|github.run_attempt|github.run_id|'
  expect CI-UPLOAD-ARTIFACT "an artifact name with the wrong run selector" "$W/upload-name.yml"
  mut upload-path 's|build/evidence/combined/gate-linux|build/evidence/combined/other|'
  expect CI-UPLOAD-ARTIFACT "an upload path outside the exact Linux combined evidence directory" "$W/upload-path.yml"
  # The audio lane is admitted only as exact single-line commands in a fixed order, and the
  # upload step's path is the only block scalar, equal to one exact reviewed file list per job.
  awk '/- name: Emit authored audio evidence/ && !done { print; getline; sub(/run: /, "run: |\n          "); print; done = 1; next } { print }' "$REAL" > "$W/audio-run-block.yml"
  expect CI-RUN-LINE "an audio emit command written as a block scalar" "$W/audio-run-block.yml"
  mut audio-cmd 's|--suite qualification --run|--suite capture-errors --run|'
  expect CI-RUN-LINE "an altered audio qualification command" "$W/audio-cmd.yml"
  mut audio-prefix 's|--artifact-prefix "08-05-\${GITHUB_JOB}|--artifact-prefix "08-05-fixed|'
  expect CI-RUN-LINE "an audio artifact prefix that no longer binds the job" "$W/audio-prefix.yml"
  mut audio-emit-sha 's|--sha "\$GITHUB_SHA" --run "\$GITHUB_RUN_ID" --attempt "\$GITHUB_RUN_ATTEMPT" --build|--sha "deadbeef" --run "$GITHUB_RUN_ID" --attempt "$GITHUB_RUN_ATTEMPT" --build|'
  expect CI-RUN-LINE "an audio emitter that does not bind the workflow SHA" "$W/audio-emit-sha.yml"
  awk '/- name: Run authored audio qualification records/ && !done { getline; done = 1; next } { print }' "$REAL" > "$W/audio-step-missing.yml"
  expect CI-RUN-LINE "a job with its audio qualification step removed" "$W/audio-step-missing.yml"
  awk '/- name: PII scan authored audio evidence/ && !done { getline; done = 1; next } { print }' "$REAL" > "$W/audio-pii-missing.yml"
  expect CI-RUN-LINE "a job with its audio PII scan removed" "$W/audio-pii-missing.yml"
  awk '/- name: Emit authored audio evidence/ { held = $0; getline; heldrun = $0; next }
       /- name: Emit authored raw RDP VI evidence/ && held != "" && !done { print; getline; print; print held; print heldrun; done = 1; next }
       { print }' "$REAL" > "$W/audio-order.yml"
  expect CI-UPLOAD-ARTIFACT "audio emission moved after the raw RDP VI emitter" "$W/audio-order.yml"
  awk '/- name: PII scan authored audio evidence/ { held = $0; getline; heldrun = $0; next }
       /- name: Upload sanitized authored evidence/ && !done { print "      - name: Gate"; print "        run: ./scripts/gate.sh"; done = 1 }
       { print }' "$REAL" > "$W/audio-dup-gate.yml"
  expect CI-RUN-LINE "a duplicated gate step in place of the audio PII scan" "$W/audio-dup-gate.yml"
  awk '/^            build\/evidence\/combined\/gate-linux\/audio\/gate-linux\/manifest.json$/ { print; print "            build/evidence/combined/gate-linux/audio/gate-linux/raw.bin"; next } { print }' "$REAL" > "$W/upload-extra.yml"
  expect CI-UPLOAD-ARTIFACT "an extra file in the upload list" "$W/upload-extra.yml"
  mut upload-dir 's|^            build/evidence/combined/gate-linux/audio/gate-linux/manifest.json$|            build/evidence/combined/gate-linux/audio|'
  expect CI-UPLOAD-ARTIFACT "a directory in place of the audio manifest in the upload list" "$W/upload-dir.yml"
  mut upload-glob 's|^            build/evidence/combined/gate-linux/rdp-vi/gate-linux/observation.json$|            build/evidence/combined/gate-linux/rdp-vi/gate-linux/*.json|'
  expect CI-UPLOAD-ARTIFACT "a wildcard in the upload list" "$W/upload-glob.yml"
  mut upload-cross 's|^            build/evidence/combined/gate-macos/audio/gate-macos/manifest.json$|            build/evidence/combined/gate-linux/audio/gate-linux/manifest.json|'
  expect CI-UPLOAD-ARTIFACT "the macOS job uploading the Linux audio manifest" "$W/upload-cross.yml"
  grep -v '^            build/evidence/combined/gate-linux/audio/gate-linux/manifest.json$' "$REAL" > "$W/upload-short.yml"
  expect CI-UPLOAD-ARTIFACT "an upload list missing the audio manifest" "$W/upload-short.yml"
  awk '/^          if-no-files-found: error$/ && !done { print "          path: |"; print "            build/evidence/combined/gate-linux/extra.json"; done = 1 } { print }' "$REAL" > "$W/upload-second-path.yml"
  expect CI-UPLOAD-ARTIFACT "a second path key after the reviewed upload list" "$W/upload-second-path.yml"
  awk '/persist-credentials: false/ && !done { print; print "          path: |"; print "            build/evidence/combined/gate-linux/gate-linux.json"; done = 1; next } { print }' "$REAL" > "$W/path-block-checkout.yml"
  expect CI-RUN-LINE "a path block scalar outside the upload step" "$W/path-block-checkout.yml"
  mut upload-folded 's#^          path: |$#          path: >#'
  expect CI-RUN-LINE "a folded block scalar in place of the reviewed upload list" "$W/upload-folded.yml"

  # The Phase 9 triple (case run, emit, PII scan) is admitted as the same kind of exact
  # single-line commands in a fixed order, and its one manifest follows the audio manifest.
  awk '/- name: Emit authored Phase 9 evidence/ && !done { print; getline; sub(/run: /, "run: |\n          "); print; done = 1; next } { print }' "$REAL" > "$W/p9-run-block.yml"
  expect CI-RUN-LINE "a Phase 9 emit command written as a block scalar" "$W/p9-run-block.yml"
  mut p9-suite 's|check-phase09-cases.py --build build/gate --suite hosted --run|check-phase09-cases.py --build build/gate --suite controller --run|'
  expect CI-RUN-LINE "an altered Phase 9 suite name" "$W/p9-suite.yml"
  mut p9-prefix 's|--artifact-prefix "09-hosted-\${GITHUB_JOB}|--artifact-prefix "09-hosted-fixed|'
  expect CI-RUN-LINE "a Phase 9 artifact prefix that no longer binds the job" "$W/p9-prefix.yml"
  mut p9-emit-sha '/qualify-phase09.py --emit-hosted/ s|--sha "\$GITHUB_SHA" --run|--sha "deadbeef" --run|'
  expect CI-RUN-LINE "a Phase 9 emitter that does not bind the workflow SHA" "$W/p9-emit-sha.yml"
  awk '/- name: Run authored Phase 9 qualification records/ && !done { getline; done = 1; next } { print }' "$REAL" > "$W/p9-case-missing.yml"
  expect CI-RUN-LINE "a job with its Phase 9 case run removed" "$W/p9-case-missing.yml"
  awk '/- name: PII scan authored Phase 9 evidence/ && !done { getline; done = 1; next } { print }' "$REAL" > "$W/p9-pii-missing.yml"
  expect CI-RUN-LINE "a job with its Phase 9 PII scan removed" "$W/p9-pii-missing.yml"
  awk '/- name: Emit authored Phase 9 evidence/ { held = $0; getline; heldrun = $0; next }
       /- name: Emit authored raw RDP VI evidence/ && held != "" && !done { print; getline; print; print held; print heldrun; done = 1; next }
       { print }' "$REAL" > "$W/p9-order.yml"
  expect CI-UPLOAD-ARTIFACT "Phase 9 emission moved after the raw RDP VI emitter" "$W/p9-order.yml"
  awk '/^            build\/evidence\/combined\/gate-linux\/phase09\/gate-linux\/manifest.json$/ { print; print "            build/evidence/combined/gate-linux/phase09/gate-linux/raw.bin"; next } { print }' "$REAL" > "$W/p9-upload-extra.yml"
  expect CI-UPLOAD-ARTIFACT "an extra file after the Phase 9 manifest in the upload list" "$W/p9-upload-extra.yml"
  grep -v '^            build/evidence/combined/gate-linux/phase09/gate-linux/manifest.json$' "$REAL" > "$W/p9-upload-short.yml"
  expect CI-UPLOAD-ARTIFACT "an upload list missing the Phase 9 manifest" "$W/p9-upload-short.yml"
  mut p9-upload-cross 's|^            build/evidence/combined/gate-macos/phase09/gate-macos/manifest.json$|            build/evidence/combined/gate-linux/phase09/gate-linux/manifest.json|'
  expect CI-UPLOAD-ARTIFACT "the macOS job uploading the Linux Phase 9 manifest" "$W/p9-upload-cross.yml"
  awk '/- name: PII scan authored Phase 9 evidence/ { held = $0; getline; heldrun = $0; next }
       /- name: Upload sanitized authored evidence/ && !done { print "      - name: Gate"; print "        run: ./scripts/gate.sh"; done = 1 }
       { print }' "$REAL" > "$W/p9-dup-gate.yml"
  expect CI-RUN-LINE "a duplicated gate step in place of the Phase 9 PII scan" "$W/p9-dup-gate.yml"

  # The Binutils cache restore is admitted only as the one reviewed SHA-pinned actions/cache step
  # with exactly the reviewed path and key, strictly between Checkout and Gate in each job.
  mut cache-tag 's|actions/cache@55cc8345863c7cc4c66a329aec7e433d2d1c52a9  # v6.1.0|actions/cache@v6.1.0  # v6.1.0|'
  expect CI-CACHE-RESTORE "the cache action pinned by tag" "$W/cache-tag.yml"
  expect CI-USES-PIN "the cache action pinned by tag is also an unpinned action" "$W/cache-tag.yml"
  mut cache-sha 's|@55cc8345863c7cc4c66a329aec7e433d2d1c52a9|@0123456789abcdef0123456789abcdef01234567|'
  expect CI-CACHE-RESTORE "the cache action pinned to an unreviewed 40-hex commit" "$W/cache-sha.yml"
  mut cache-restore-action 's|actions/cache@55cc8345863c7cc4c66a329aec7e433d2d1c52a9|actions/cache/restore@55cc8345863c7cc4c66a329aec7e433d2d1c52a9|'
  expect CI-CACHE-RESTORE "the cache restore sub-action in place of the reviewed action" "$W/cache-restore-action.yml"
  mut cache-path 's|^          path: build/tools/install$|          path: build/gate|'
  expect CI-CACHE-RESTORE "a cache path that is not the Binutils install directory" "$W/cache-path.yml"
  mut cache-path-extra 's|^          path: build/tools/install$|          path: build/tools/install/bin|'
  expect CI-CACHE-RESTORE "a cache path narrowed below the Binutils install directory" "$W/cache-path-extra.yml"
  awk '/^          key: binutils-/ && !done { print; print "          restore-keys: binutils-"; done = 1; next } { print }' "$REAL" > "$W/cache-restore-keys.yml"
  expect CI-CACHE-RESTORE "an added restore-keys fallback" "$W/cache-restore-keys.yml"
  awk '/^          key: binutils-/ && !done { print; print "          save-always: true"; done = 1; next } { print }' "$REAL" > "$W/cache-save-always.yml"
  expect CI-CACHE-RESTORE "an added save-always input" "$W/cache-save-always.yml"
  for _input in "lookup-only: true" "fail-on-cache-miss: true" "enableCrossOsArchive: true"; do
    _label=$(printf '%s' "$_input" | sed 's/:.*//' | tr '[:upper:]' '[:lower:]')
    awk -v add="          $_input" '/^          key: binutils-/ && !done { print; print add; done = 1; next } { print }' "$REAL" > "$W/cache-input-$_label.yml"
    expect CI-CACHE-RESTORE "an added $_input input" "$W/cache-input-$_label.yml"
  done
  mut cache-key-arch 's|-\${{ runner.arch }}-|-|'
  expect CI-CACHE-RESTORE "a cache key that no longer binds the runner architecture" "$W/cache-key-arch.yml"
  mut cache-key-hash 's|hashFiles(.scripts/build-boot-fixtures.sh.)|hashFiles('"'"'scripts/gate.sh'"'"')|'
  expect CI-CACHE-RESTORE "a cache key that hashes a different file" "$W/cache-key-hash.yml"
  mut cache-key-fixed 's|^          key: .*$|          key: binutils-fixed|'
  expect CI-CACHE-RESTORE "a fixed cache key" "$W/cache-key-fixed.yml"
  awk '/^      - name: Restore pinned Binutils$/ { n++ } n == 2 && /^      - name: Gate$/ { n = 3 } n == 2 { next } { print }' "$REAL" > "$W/cache-missing.yml"
  expect CI-CACHE-RESTORE "the cache step missing from the macOS job only" "$W/cache-missing.yml"
  awk '/^      - name: Restore pinned Binutils$/ && !done { held = 1; done = 1 }
       held && /^      - name: Gate$/ { held = 0; gate = 1 }
       held { buf = buf $0 "\n"; next }
       { print }
       gate && /^        run: .\/scripts\/gate.sh$/ { printf "%s", buf; gate = 0 }' "$REAL" > "$W/cache-after-gate.yml"
  expect CI-CACHE-RESTORE "the cache step moved after Gate" "$W/cache-after-gate.yml"
  awk '/^      - name: Restore pinned Binutils$/ && !done { held = 1; done = 1 }
       held && /^      - name: Gate$/ { held = 0 }
       held { buf = buf $0 "\n"; next }
       { out[++n] = $0 }
       END { for (i = 1; i <= n; i++) { if (out[i] == "      - name: Checkout" && !put) { printf "%s", buf; put = 1 } print out[i] } }' "$REAL" > "$W/cache-before-checkout.yml"
  expect CI-CACHE-RESTORE "the cache step moved before Checkout" "$W/cache-before-checkout.yml"
  awk '/^      - name: Restore pinned Binutils$/ && !done { held = 1; done = 1 }
       held && /^      - name: Gate$/ { held = 0; printf "%s", buf }
       held { buf = buf $0 "\n" }
       { print }' "$REAL" > "$W/cache-duplicate.yml"
  expect CI-CACHE-RESTORE "a duplicated cache step" "$W/cache-duplicate.yml"
  awk '/^      - name: Gate$/ && !done { print "      - name: Restore unrelated build cache"; print "        uses: buildjet/cache@0123456789abcdef0123456789abcdef01234567"; print "        with:"; print "          path: build/gate"; print "          key: other"; done = 1 } { print }' "$REAL" > "$W/cache-second.yml"
  expect CI-CACHE-RESTORE "a second, unrelated cache action" "$W/cache-second.yml"
  expect CI-CACHE-RESTORE "the cache step with a step-level skip condition" "$W/skip-restore-pinned-binutils.yml"
  expect CI-CACHE-RESTORE "the cache step with continue-on-error enabled" "$W/soft-restore-pinned-binutils.yml"

  mut upload-missing-policy 's|if-no-files-found: error|if-no-files-found: warn|'
  expect CI-UPLOAD-ARTIFACT "an upload that succeeds when its file is missing" "$W/upload-missing-policy.yml"
  sed '/^      - name: Upload sanitized authored evidence$/,/^          if-no-files-found: error$/d' "$REAL" > "$W/upload-missing.yml"
  expect CI-UPLOAD-ARTIFACT "a job with its evidence upload removed" "$W/upload-missing.yml"
  mut expr 's|run: ./scripts/gate.sh|run: ./scripts/gate.sh ${{ github.head_ref }}|'
  expect CI-EXPR-IN-RUN "an expression in a run line" "$W/expr.yml"

  # WR-15: every spelling of a secret or the token, in an env value.
  _n=0
  for _e in "\${{ secrets['TOKEN'] }}" "\${{ secrets[format('{0}', 'T')] }}" \
    "\${{ toJSON(secrets) }}" "\${{ github['token'] }}" "\${{ github.token }}" \
    "\${{ github [ \"token\" ] }}" "\${{ toJSON(github) }}"; do
    _n=$((_n + 1))
    awk -v e="$_e" '/timeout-minutes: 10/ && !done { print; print "    env:"; print "      V: " e; done = 1; next } { print }' "$REAL" > "$W/secret$_n.yml"
    expect CI-SECRETS "secret or token spelling $_n" "$W/secret$_n.yml"
  done
  # WR-15: privileged and unlisted triggers, in block and flow form.
  for _t in workflow_run issue_comment pull_request_review_comment repository_dispatch schedule; do
    sed "s|^  workflow_dispatch:|  $_t:|" "$REAL" > "$W/trig-$_t.yml"
    expect CI-TRIGGER "the $_t trigger" "$W/trig-$_t.yml"
  done
  awk '/^on:/ { print "on: [push, workflow_run]"; skip = 1; next } skip && /^[^ ]/ { skip = 0 } !skip { print }' "$REAL" > "$W/trig-flow.yml"
  expect CI-TRIGGER "a privileged trigger in flow form" "$W/trig-flow.yml"
  awk '/^on:/ { print "\"on\":"; next } { print }' "$REAL" | sed 's|^  push:|  - issue_comment|' > "$W/trig-list.yml"
  expect CI-TRIGGER "a quoted on key with a list item" "$W/trig-list.yml"
  awk '/^on:/ { print "on: issue_comment"; skip = 1; next } skip && /^[^ ]/ { skip = 0 } !skip { print }' "$REAL" > "$W/trig-scalar.yml"
  expect CI-TRIGGER "a scalar on value" "$W/trig-scalar.yml"

  # WR-15: directory mode checks every workflow file, not only ci.yml.
  mkdir "$W/wf-second" "$W/wf-other" "$W/wf-empty" "$W/wf-ok"
  cp "$REAL" "$W/wf-second/ci.yml"
  sed 's|^  pull_request:|  pull_request_target:|' "$REAL" > "$W/wf-second/release.yaml"
  expect CI-PR-TARGET "a second workflow file with the privileged trigger" "$W/wf-second" "" --dir
  cp "$REAL" "$W/wf-other/ci.yml"
  printf 'name: x\n' > "$W/wf-other/notes.txt"
  expect CI-WORKFLOW-FILE "a non-YAML file in the workflow directory" "$W/wf-other" "" --dir
  expect CI-WORKFLOW-FILE "a workflow directory with no workflow" "$W/wf-empty" "" --dir
  cp "$REAL" "$W/wf-ok/ci.yml"
  cp "$REAL" "$W/wf-ok/second.yml"
  CASES=$((CASES + 1))
  if _out=$(sh "$SELF" --dir "$W/wf-ok" 2>&1); then echo "ok dir-clean: two hardened workflow files pass"
  else echo "FAIL dir-clean: two hardened workflow files were rejected: $_out"; FAILS=$((FAILS + 1)); fi

  cp "$ROOT/scripts/gate.sh" "$W/gate-noexec.sh"
  chmod 644 "$W/gate-noexec.sh"
  expect CI-GATE-EXEC "a gate script without the executable bit" "$REAL" "$W/gate-noexec.sh"

  awk '/^jobs:/ { print; exit } { print }' "$REAL" > "$W/zero.yml"
  expect CI-ZERO-JOBS "a workflow with no jobs" "$W/zero.yml"

  for _r in $ALL_RULES; do
    case " $SEEN " in
      *" $_r "*) ;;
      *) echo "FAIL $_r: no seeded mutation asserts this rule"; FAILS=$((FAILS + 1)) ;;
    esac
  done

  echo "check-ci-yml selftest: cases=$CASES failures=$FAILS"
  if [ "$FAILS" -ne 0 ]; then return 1; fi
  return 0
}

# ---- main -----------------------------------------------------------------------------------

FILE=""
DIR=$ROOT/.github/workflows
GATE=$ROOT/scripts/gate.sh
MODE=check
while [ $# -gt 0 ]; do
  case "$1" in
    --file)
      if [ $# -lt 2 ]; then usage; exit 2; fi
      FILE=$2
      DIR=""
      shift 2
      ;;
    --dir)
      if [ $# -lt 2 ]; then usage; exit 2; fi
      DIR=$2
      FILE=""
      shift 2
      ;;
    --gate)
      if [ $# -lt 2 ]; then usage; exit 2; fi
      GATE=$2
      shift 2
      ;;
    --selftest)
      MODE=selftest
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage
      exit 2
      ;;
  esac
done

if [ "$MODE" = "selftest" ]; then
  if selftest; then exit 0; else exit 1; fi
fi

OUT=""
# add_out TEXT: appends lines to OUT.
add_out() {
  [ -n "$1" ] || return 0
  if [ -n "$OUT" ]; then OUT="$OUT
$1"; else OUT=$1; fi
}
if [ -n "$FILE" ]; then
  if [ ! -f "$FILE" ]; then
    echo "check-ci-yml: workflow not found: $FILE" >&2
    exit 2
  fi
  _o=$(scan_yml "$FILE") || { echo "check-ci-yml: tool error while scanning $FILE" >&2; exit 2; }
  add_out "$_o"
else
  [ -d "$DIR" ] || { echo "check-ci-yml: workflow directory not found: $DIR" >&2; exit 2; }
  _nwf=0
  for _f in "$DIR"/* "$DIR"/.[!.]*; do
    [ -e "$_f" ] || [ -L "$_f" ] || continue
    case "$_f" in
      *.yml | *.yaml)
        if [ -f "$_f" ] && [ ! -L "$_f" ]; then
          _nwf=$((_nwf + 1))
          _o=$(scan_yml "$_f") || { echo "check-ci-yml: tool error while scanning $_f" >&2; exit 2; }
          add_out "$_o"
          continue
        fi
        ;;
    esac
    add_out "CI-WORKFLOW-FILE $_f: only regular .yml or .yaml workflow files may live in the workflow directory"
  done
  if [ "$_nwf" -eq 0 ]; then
    add_out "CI-WORKFLOW-FILE $DIR: the workflow directory holds no workflow file"
  fi
fi
if [ ! -x "$GATE" ]; then
  add_out "CI-GATE-EXEC $GATE: the gate script is missing or not executable"
fi
if [ -n "$OUT" ]; then
  printf '%s\n' "$OUT"
  exit 1
fi
echo "check-ci-yml: ok"
exit 0
