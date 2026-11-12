import test from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync, spawnSync } from 'node:child_process';
import { mkdtempSync, mkdirSync, writeFileSync, renameSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const helper = fileURLToPath(new URL('../../scripts/verification-scope.mjs', import.meta.url));

function fixture(t) {
  const root = mkdtempSync(join(tmpdir(), 't64 workflow scope '));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const env = { ...process.env };
  for (const key of Object.keys(env)) {
    if (key.startsWith('GIT_')) delete env[key];
  }
  env.GIT_CONFIG_GLOBAL = '/dev/null';
  env.GIT_CONFIG_NOSYSTEM = '1';
  const git = (...args) => execFileSync('git', ['-C', root,
    '-c', 'user.name=workflow-fixture', '-c', 'user.email=workflow-fixture', ...args],
    { encoding: 'utf8', env, stdio: ['ignore', 'pipe', 'pipe'] });
  const write = (path, content) => {
    const full = join(root, path);
    mkdirSync(resolve(full, '..'), { recursive: true });
    writeFileSync(full, content);
  };
  git('init', '--quiet');
  write('core/t64_sample.c', 'int sample(void) { return 0; }\n');
  write('.gitignore', 'build/\n');
  git('add', '.'); git('commit', '--quiet', '-m', 'baseline');
  const base = git('rev-parse', 'HEAD').trim();
  const scope = () => JSON.parse(execFileSync(process.execPath,
    [helper, '--repo', root, '--base', base], { encoding: 'utf8', env }));
  return { root, env, git, write, base, scope };
}

test('metadata commit and dirty owner instructions retain production evidence', t => {
  const f = fixture(t);
  f.write('.planning/STATE.md', 'position\n');
  f.git('add', '.planning/STATE.md'); f.git('commit', '--quiet', '-m', 'tracking');
  f.write('AGENTS.md', 'instructions\n');
  f.write('build/private-cpu-diagnostic/source.rs', 'private input\n');
  const result = f.scope();
  assert.equal(result.metadata_only, true);
  assert.equal(result.production_or_build_inputs_changed, false);
  assert.deepEqual(result.changed.metadata, ['.planning/STATE.md', 'AGENTS.md']);
  assert.equal(JSON.stringify(result).includes('source.rs'), false);
  assert.ok(result.limits.some(line => line.includes('Private/build-cache')));
});

test('staged, unstaged and untracked core changes all require owning verification', t => {
  const f = fixture(t);
  f.write('core/t64_sample.c', 'int sample(void) { return 1; }\n');
  f.git('add', 'core/t64_sample.c');
  f.write('core/t64_sample.c', 'int sample(void) { return 2; }\n');
  f.write('core/t64_new.c', 'int new_sample(void) { return 3; }\n');
  assert.deepEqual(f.scope().changed.core_or_build_inputs, ['core/t64_new.c', 'core/t64_sample.c']);
});

test('renaming core code into docs does not hide the production deletion', t => {
  const f = fixture(t);
  mkdirSync(join(f.root, 'docs'));
  renameSync(join(f.root, 'core/t64_sample.c'), join(f.root, 'docs/sample.md'));
  f.git('add', '-A');
  const result = f.scope();
  assert.equal(result.production_or_build_inputs_changed, true);
  assert.deepEqual(result.changed.core_or_build_inputs, ['core/t64_sample.c']);
});

test('a worktree edit cannot hide a different staged production version', t => {
  const f = fixture(t);
  f.write('core/t64_sample.c', 'int sample(void) { return 2; }\n');
  f.git('add', 'core/t64_sample.c');
  f.write('core/t64_sample.c', 'int sample(void) { return 0; }\n');
  assert.deepEqual(f.scope().changed.core_or_build_inputs, ['core/t64_sample.c']);
});

test('an ignore rule cannot hide an untracked production source file', t => {
  const f = fixture(t);
  f.write('.gitignore', 'build/\ncore/t64_hidden.c\n');
  f.write('core/t64_hidden.c', 'int hidden_sample(void) { return 4; }\n');
  assert.deepEqual(f.scope().changed.core_or_build_inputs, ['core/t64_hidden.c']);
});

test('build policy, ignore security, tests and unknown files remain visible', t => {
  const f = fixture(t);
  f.write('CMakePresets.json', '{}\n');
  f.write('.gitignore', 'build/\nnew-ignore\n');
  f.write('runner/tests/control.c', 'test\n');
  f.write('unknown-policy', 'review me\n');
  const result = f.scope();
  assert.equal(result.metadata_only, false);
  assert.deepEqual(result.changed.core_or_build_inputs, ['CMakePresets.json']);
  assert.deepEqual(result.changed.host_fixture_or_pipeline, ['.gitignore']);
  assert.deepEqual(result.changed.tests, ['runner/tests/control.c']);
  assert.deepEqual(result.changed.unclassified, ['unknown-policy']);
});

test('an invalid baseline fails instead of returning an unchanged-source decision', t => {
  const f = fixture(t);
  const result = spawnSync(process.execPath, [helper, '--repo', f.root, '--base', 'missing-ref'],
    { encoding: 'utf8', env: f.env });
  assert.equal(result.status, 2);
  assert.equal(result.stdout, '');
});

test('an inherited Git override cannot redirect the requested checkout', t => {
  const f = fixture(t);
  const other = fixture(t);
  f.write('docs/record.md', 'metadata\n');
  other.write('core/t64_sample.c', 'int sample(void) { return 99; }\n');
  const env = { ...f.env, GIT_DIR: join(other.root, '.git'),
    GIT_WORK_TREE: other.root, GIT_INDEX_FILE: join(other.root, '.git/index') };
  const result = JSON.parse(execFileSync(process.execPath,
    [helper, '--repo', f.root, '--base', f.base], { encoding: 'utf8', env }));
  assert.equal(result.metadata_only, true);
  assert.deepEqual(result.changed.metadata, ['docs/record.md']);
  assert.deepEqual(result.changed.core_or_build_inputs, []);
});
