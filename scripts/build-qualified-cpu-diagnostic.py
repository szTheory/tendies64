#!/usr/bin/env python3
"""Build and seal the already audited local CPU diagnostic without acquisition."""
from __future__ import annotations

import hashlib
import importlib.util
import json
import os
import pathlib
import shutil
import signal
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
ROOT = pathlib.Path(__file__).resolve().parents[1]
PRIVATE = ROOT / 'build/private-cpu-diagnostic'
SOURCE = PRIVATE / 'overlay-checkout-8aa0921'
PREFIX = PRIVATE / 'toolchain-prefix'
ADAPTER = ROOT / 'build/gate/runner/t64-cpu-diagnostic'


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def private(path, exists=True):
    path = pathlib.Path(os.path.abspath(path))
    relative = path.relative_to(PRIVATE)
    current = ROOT
    for part in PRIVATE.relative_to(ROOT).parts + relative.parts:
        current /= part
        if current.is_symlink():
            raise ValueError('private input or destination traverses a symlink')
    if not PRIVATE.is_dir() or PRIVATE.stat().st_mode & 0o077:
        raise ValueError('private root must exist with owner-only access')
    if exists and not path.is_file():
        raise ValueError('required private regular file is missing')
    return path


def load(path):
    path = private(path)
    if path.stat().st_size > 16 * 1024 * 1024:
        raise ValueError('private metadata exceeds its bound')
    return json.loads(path.read_text())


def write(path, value):
    path = private(path, False)
    with path.open('x') as stream:
        json.dump(value, stream, sort_keys=True, separators=(',', ':'), ensure_ascii=False)
        stream.write('\n')


def run(command, work, label, *, cwd=ROOT, env=None, timeout=30):
    start = time.monotonic()
    with (work / (label + '.stdout.log')).open('xb') as out, (work / (label + '.stderr.log')).open('xb') as err:
        process = subprocess.Popen(command, cwd=cwd, env=env, stdin=subprocess.DEVNULL,
                                   stdout=out, stderr=err, start_new_session=True)
        timed_out = False
        deadline = start + timeout
        # Keep the exited leader unreaped until cleanup: its reserved PID
        # prevents a reused process-group ID from naming somebody else's job.
        while os.waitid(os.P_PID, process.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT) is None:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                timed_out = True
                break
            time.sleep(min(0.01, remaining))
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except (ProcessLookupError, PermissionError):
            # Some hosts refuse signals to zombie-only groups. Reaping and
            # the disappearance check below still have to establish cleanup.
            pass
        returncode = process.wait(timeout=1)
        group_empty = False
        cleanup_deadline = time.monotonic() + 1
        while time.monotonic() < cleanup_deadline:
            try:
                os.killpg(process.pid, 0)
            except ProcessLookupError:
                group_empty = True
                break
            except PermissionError:
                pass
            time.sleep(0.01)
    write(work / (label + '.command.json'), {'argv': [str(x) for x in command], 'cwd': str(cwd), 'environment': env,
          'exit': returncode, 'timed_out': timed_out, 'process_group_empty': group_empty, 'seconds': round(time.monotonic() - start, 3)})
    if timed_out or returncode or not group_empty:
        raise ValueError(label + ' failed; inspect its private log')
    return (work / (label + '.stdout.log')).read_bytes()


def build():
    if os.environ.get('CI', '').lower() in ('true', '1') or os.environ.get('T64_REQUIRE_FIXTURES') == '1':
        raise ValueError('qualified diagnostic builds are local-only')
    os.umask(0o077)
    private(PRIVATE, False)
    work = pathlib.Path(tempfile.mkdtemp(prefix='qualified-build-', dir=PRIVATE))
    run(['sh', 'scripts/build-cpu-diagnostic.sh', '--prepare-source'], work, 'source')
    run(['python3', '-B', 'scripts/verify-cpu-diagnostic-qualification.py', str(PRIVATE / 'qualification.json'), str(ROOT)], work, 'qualification')
    run(['sh', 'scripts/build-cpu-diagnostic.sh', '--verify-toolchain'], work, 'toolchain')
    qualification = load(PRIVATE / 'qualification.json')
    # Revalidate the saved graph against current inputs; no graph generation,
    # dependency acquisition, tool installation, or independent package audit.
    review = load(PRIVATE / 'unit-graph-review-handler-snapshot-fix.json')
    command = ['python3', '-B', 'scripts/verify-cpu-diagnostic-unit-graph.py']
    for key, record in review['inputs'].items():
        path = ROOT / record['path']
        if key == 'source_closure':
            path = ROOT / qualification['assets']['closure_report']['path']
        command += ['--' + key.replace('_', '-'), str(path)]
    command += ['--output', str(work / 'unit-graph.json')]
    run(command, work, 'graph')
    runtime = [('lib/rustlib/aarch64-apple-darwin/bin/rust-lld', '6f44b61e91d6d7b6ba80bb75391587bb4fa832b248281bd67d519516cde43f98'),
               ('lib/libLLVM.dylib', '216146d4f374dea0eb0df654049025f82454aaa147af6d8388e5b37ff54084e1')]
    for relative, expected in runtime:
        if digest(private(PREFIX / relative)) != expected:
            raise ValueError('audited linker runtime bytes changed')
    locks = [SOURCE / 'Cargo.lock', PREFIX / 'lib/rustlib/src/rust/library/Cargo.lock']
    lock_hashes = [digest(private(path)) for path in locks]
    env = {'PATH': str(PREFIX / 'bin') + ':/usr/bin:/bin:/usr/sbin:/sbin',
           'HOME': str(PRIVATE / 'toolchain-home'), 'TMPDIR': str(PRIVATE / 'toolchain-tmp'),
           'CARGO_HOME': str(PRIVATE / 'cargo-home-attempt-03'),
           'CARGO_TARGET_DIR': str(PRIVATE / 'cargo-target-build-attempt-03'),
           'RUSTC': str(PREFIX / 'bin/rustc'), 'LC_ALL': 'C', 'LANG': 'C',
           'DYLD_LIBRARY_PATH': str(PREFIX / 'lib')}
    run([str(PREFIX / 'bin/cargo'), '--config', 'profile.release.panic="abort"', 'build', '--jobs', '4', '--release',
         '--target', 'mips-nintendo64-none.json', '--offline', '--locked', '--frozen'], work, 'cargo', cwd=SOURCE, env=env, timeout=600)
    if lock_hashes != [digest(path) for path in locks]:
        raise ValueError('qualified build changed a locked resolution')
    elf = work / 'diagnostic.elf'
    shutil.copyfile(private(PRIVATE / 'cargo-target-build-attempt-03/mips-nintendo64-none/release/n64-systemtest'), elf)
    tools = {row['name']: row for row in qualification['tools']['target_binutils']}
    objcopy = tools['objcopy']
    if digest(pathlib.Path(objcopy['path'])) != objcopy['sha256']:
        raise ValueError('qualified objcopy changed')
    payload, rom = work / 'base.payload.bin', work / 'base.z64'
    run([objcopy['path'], '-O', 'binary', str(elf), str(payload)], work, 'objcopy')
    if digest(pathlib.Path(objcopy['path'])) != objcopy['sha256']:
        raise ValueError('qualified objcopy changed during invocation')
    compiler = qualification['tools']['host_c_compiler']
    if digest(pathlib.Path(compiler['path'])) != compiler['sha256']:
        raise ValueError('qualified host compiler changed')
    packer = work / 'packer'
    run([compiler['path'], '-std=c17', '-Wall', '-Wextra', '-Werror', '-Wconversion', '-Wshadow', '-Wstrict-prototypes', '-pedantic',
         '-Itools/common', 'tools/fixture/t64_rom_pack.c', 'tools/common/t64_sha256.c', '-o', str(packer)], work, 'packer-build')
    run([str(packer), '--selftest'], work, 'packer-controls')
    run([str(packer), '--pack', '--cpu-profile', '--boot', str(ROOT / 'build/fixtures/ipl3_compat.z64'), '--payload', str(payload),
         '--output', str(rom), '--entry', '0x80000400', '--clock', '0x0000000f', '--crc2', '0x00000000'], work, 'pack')
    closure = ROOT / qualification['assets']['closure_report']['path']
    composition = work / 'composition.json'
    run(['python3', '-B', 'scripts/verify-cpu-diagnostic-build.py', '--elf', str(elf), '--payload', str(payload), '--rom', str(rom),
         '--boot', str(ROOT / 'build/fixtures/ipl3_compat.z64'), '--closure', str(closure), '--font-source', str(PRIVATE / 'nemu64-test/src/graphics/system_font.rs'),
         '--report', str(composition)], work, 'composition')
    if ADAPTER.is_symlink() or not ADAPTER.is_file() or not os.access(ADAPTER, os.X_OK):
        raise ValueError('production adapter path is missing, linked, or not executable')
    adapter_sha = digest(ADAPTER)
    runtime_identity = json.loads(run([str(ADAPTER), '--runtime-identity-json'], work, 'runtime'))
    if digest(ADAPTER) != adapter_sha:
        raise ValueError('production adapter changed during runtime identity inspection')
    if set(runtime_identity) != {'core_build_sha256', 'profile_sha256', 'source_revision'}:
        raise ValueError('runtime identity schema differs')
    projection = load(PRIVATE / 'source-projection.json')
    image = rom.read_bytes()
    offset = load(composition)['layout']['selector_rom_offset']
    if image[offset:offset + 4] != bytes(4):
        raise ValueError('base selector is not zero')
    identity = {'base_rom_sha256': digest(rom), 'core_build_sha256': runtime_identity['core_build_sha256'],
                'overlay_sha256': qualification['source']['overlay_sha256'], 'profile_sha256': runtime_identity['profile_sha256'],
                'source_revision': projection['source_revision'], 'tool_receipt_sha256': digest(PRIVATE / 'qualification.json')}
    registrations = []
    prefix = hashlib.sha256(image[:offset])
    tail = image[offset + 4:]
    for source_row in projection['registrations']:
        row = {key: source_row[key] for key in ('execution', 'group', 'index', 'lane', 'level', 'name', 'reason')}
        row['values'] = []
        for source_value in source_row['values']:
            value = dict(source_value)
            member_hash = prefix.copy()
            member_hash.update(((row['index'] << 16) | value['index']).to_bytes(4, 'big'))
            member_hash.update(tail)
            value['selected_rom_sha256'] = member_hash.hexdigest()
            row['values'].append(value)
        registrations.append(row)
    inventory = work / 'inventory.json'
    write(inventory, {'schema': 't64.cpu-inventory', 'schema_version': 3, 'identity': identity, 'registrations': registrations,
          'declared_counts': {'registrations': str(len(registrations)), 'values': str(sum(len(r['values']) for r in registrations)),
                             'required_values': str(sum(len(r['values']) for r in registrations if r['lane'] == 'required-base'))}})
    if ADAPTER.is_symlink() or digest(ADAPTER) != adapter_sha:
        raise ValueError('production adapter changed before inventory validation')
    run([str(ADAPTER), '--validate-inventory', str(inventory)], work, 'inventory')
    if digest(ADAPTER) != adapter_sha:
        raise ValueError('production adapter changed during inventory validation')
    artifacts = {'base_rom': rom, 'composition': composition, 'inventory': inventory, 'qualification': PRIVATE / 'qualification.json',
                 'source_projection': PRIVATE / 'source-projection.json', 'source_closure': closure}
    sealed = {'schema': 't64.cpu-qualified-build', 'schema_version': 1, 'status': 'qualified',
              'adapter': {'path': ADAPTER.relative_to(ROOT).as_posix(), 'sha256': adapter_sha},
              'artifacts': {key: {'path': path.relative_to(PRIVATE).as_posix(), 'sha256': digest(path)} for key, path in artifacts.items()},
              'selector': {'rom_offset': offset, 'width': 4, 'encoding': 'registration-index-u16be:value-index-u16be'}}
    seal = PRIVATE / 'qualified-build.json'
    private(seal, False)
    temporary = work / 'qualified-build.json'
    write(temporary, sealed)
    if seal.exists():
        previous = private(seal).read_bytes()
        with (work / 'previous-qualified-build.json').open('xb') as stream:
            stream.write(previous)
    os.replace(temporary, seal)
    print('PASS: qualified source build, actual composition, and complete inventory sealed privately')


def startup():
    """Exercise one required member through the same checked execution path."""
    os.umask(0o077)
    spec = importlib.util.spec_from_file_location('t64_cpu_run', ROOT / 'scripts/run-cpu-diagnostic.py')
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    module.refuse_public_execution()
    context = module.load_qualified_inputs(ADAPTER)
    member = module.required_members(context.inventory)[0]
    work = module.create_run_directory(context.private_root)
    module.create_manifest(context, work)
    variant, batch = work / 'startup.z64', work / 'batches/startup.json'
    module.write_variant(variant, context, member)
    command = [str(ADAPTER), '--run-local', '--local-only', str(context.private_root / context.inventory_path),
               str(variant), str(len(context.base_bytes)), member['selected_rom_sha256'], str(module.TICK_LIMIT),
               str(member['registration_index']), str(member['value_index']), str(batch)]
    start = time.monotonic()
    try:
        code, stdout, stderr = module.invoke_adapter(
            context, *command[1:], timeout=module.RUN_TIMEOUT,
            maximum=module.MAX_GUEST_OUTPUT)
        module.write_exclusive(work / 'startup.stdout.log', context.private_root, stdout)
        module.write_exclusive(work / 'startup.stderr.log', context.private_root, stderr)
        complete = code in (0, 2) and module.read_batch_if_present(batch, context.private_root) is not None
        write(work / 'startup-result.json', {'schema': 't64.cpu-startup-qualification', 'schema_version': 1,
              'qualified_build_sha256': context.qualified_build_sha256, 'inventory_sha256': context.inventory_sha256,
              'member': member, 'command': command, 'exit': code, 'startup_complete': complete,
              'seconds': round(time.monotonic() - start, 3), 'whole_inventory_complete': False})
        if not complete:
            raise ValueError('actual required startup did not complete; inspect the private attempt')
        print('PASS: actual required startup completed; whole-inventory qualification remains separate')
    finally:
        if variant.exists() and not variant.is_symlink():
            variant.unlink()


if __name__ == '__main__':
    os.chdir(ROOT)
    try:
        if sys.argv[1:] == ["--qualify-startup"]:
            startup()
        elif not sys.argv[1:]:
            build()
        else:
            raise ValueError("unknown build/startup mode")
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        print('qualified build refused: ' + str(error), file=sys.stderr)
        raise SystemExit(4)
