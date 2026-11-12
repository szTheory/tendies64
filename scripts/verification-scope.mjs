#!/usr/bin/env node
// Advisory native-verification scope. This never authorizes a test, use or push.
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';
import { parseArgs } from 'node:util';

const usage = 'usage: node scripts/verification-scope.mjs --base <last-tested-commit> [--repo <checkout>]';

function git(repo, ...args) {
  const env = { ...process.env };
  for (const key of ['GIT_DIR', 'GIT_WORK_TREE', 'GIT_INDEX_FILE', 'GIT_EXTERNAL_DIFF']) delete env[key];
  return execFileSync('git', ['-C', repo, ...args], {
    encoding: 'utf8', env, stdio: ['ignore', 'pipe', 'pipe'],
  });
}

function category(path) {
  if (path === 'scripts/verification-scope.mjs' || path.startsWith('tests/workflow/')) {
    return 'verification_tools';
  }
  if (path.startsWith('.planning/') || path.startsWith('docs/') || path === 'AGENTS.md') {
    return 'metadata';
  }
  // These files enter T64_BUILD_MANIFEST, even when a CMake edit only adds a test.
  if (path === 'CMakeLists.txt' || path === 'CMakePresets.json' ||
      path === 'core/CMakeLists.txt' || path === 'tools/common/CMakeLists.txt' ||
      /^tools\/common\/t64_sha256\.[ch]$/.test(path)) {
    return 'core_or_build_inputs';
  }
  if (path.startsWith('core/tests/') || path.startsWith('runner/tests/') ||
      path.startsWith('tests/')) return 'tests';
  if (path.startsWith('core/')) return 'core_or_build_inputs';
  // .gitignore affects the diagnostic's private-output boundary.
  if (path === '.gitignore' || path.startsWith('runner/') ||
      path.startsWith('tools/') || path.startsWith('scripts/') ||
      path.startsWith('fixtures/') || path.startsWith('.github/')) {
    return 'host_fixture_or_pipeline';
  }
  return 'unclassified';
}

try {
  const { values } = parseArgs({ options: {
    base: { type: 'string' }, repo: { type: 'string' }, help: { type: 'boolean' },
  } });
  if (values.help) {
    console.log(usage);
  } else {
    if (!values.base) throw new Error(usage);
    const requested = resolve(values.repo ?? resolve(dirname(fileURLToPath(import.meta.url)), '..'));
    const repo = git(requested, 'rev-parse', '--show-toplevel').replace(/\n$/, '');
    const base = git(repo, 'rev-parse', '--verify', '--end-of-options',
      `${values.base}^{commit}`).trim();
    const head = git(repo, 'rev-parse', '--verify', 'HEAD').trim();
    // Compare both index and worktree with the tested commit. A worktree edit
    // can cancel a staged change without removing it from the next commit.
    // Disable rename detection so a core deletion cannot hide behind a docs rename.
    const tracked = git(repo, 'diff', '--name-only', '--no-renames', '--no-ext-diff', '--no-textconv', '-z', base, '--');
    const staged = git(repo, 'diff', '--cached', '--name-only', '--no-renames', '--no-ext-diff', '--no-textconv', '-z', base, '--');
    const untracked = git(repo, 'ls-files', '--others', '--exclude-standard', '-z');
    // CMake globs can compile ignored/untracked code too. Keep private build
    // caches out, but never let an ignore rule hide source in production trees.
    const ignoredSource = git(repo, 'ls-files', '--others', '-z', '--', 'core', 'runner', 'tools', 'scripts');
    const paths = [...new Set((tracked + staged + untracked + ignoredSource).split('\0').filter(Boolean))].sort();
    const changed = { core_or_build_inputs: [], host_fixture_or_pipeline: [],
      tests: [], verification_tools: [], metadata: [], unclassified: [] };
    for (const path of paths) changed[category(path)].push(path);
    const production = changed.core_or_build_inputs.length > 0 ||
      changed.host_fixture_or_pipeline.length > 0;
    const guidance = [];
    if (changed.core_or_build_inputs.length) guidance.push('Run owning core/build checks and necessary regressions.');
    if (changed.host_fixture_or_pipeline.length) guidance.push('Run owning host/fixture/pipeline checks and affected integrations.');
    if (changed.tests.length) guidance.push('Build and execute changed tests and any affected inventory controls.');
    if (changed.verification_tools.length) guidance.push('Run the workflow-tool tests.');
    if (changed.unclassified.length) guidance.push('Inspect unclassified changes before choosing verification scope.');
    if (!production && !changed.unclassified.length) {
      guidance.push('Production/build inputs are unchanged: reuse prior semantic verification; check changed tests, docs and policies.');
    }
    console.log(JSON.stringify({ base, head, changed,
      metadata_only: paths.every(path => category(path) === 'metadata'),
      production_or_build_inputs_changed: production,
      guidance,
      limits: [
        'Advisory path classification, not proof of correctness or permission to skip required checks.',
        'Private/build-cache inputs and external tools are not examined; this cannot qualify them.',
        'Required first-time checks, source/artifact identity, rights and publication gates still apply.',
      ],
    }, null, 2));
  }
} catch (error) {
  console.error(`verification-scope: ${error.message}`);
  process.exitCode = 2;
}
