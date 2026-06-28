# Configuration for dingo lit tests.

import os
import platform
import shlex
import subprocess
import lit.formats

config.name = 'dingo-lit-tests'
config.test_source_root = os.path.dirname(__file__)
project_root = os.path.abspath(os.path.join(config.test_source_root, '..', '..'))
config.available_features.add(platform.system().lower())
config.suffixes = ['.t', '.txt', '.cpp', '.hpp', '.h', '.cxx']
# Use lit's internal shell so builtin commands like `not` work on runners
# without a host `not` executable.
config.test_format = lit.formats.ShTest(False)
config.excludes = ['Output']

if platform.system() == 'Windows':
    python = os.environ.get('PYTHON', 'python')
else:
    python = os.environ.get('PYTHON', 'python3')

cxx = os.environ.get('CXX', 'cl' if platform.system() == 'Windows' else 'c++')
config.environment['DINGO_CXX_ID'] = os.path.basename(cxx)

def windows_msvc_target_arch(cxx_path):
    parts = cxx_path.replace('\\', '/').lower().split('/')
    for index, part in enumerate(parts[:-1]):
        if part.startswith('host') and parts[index + 1] in ('x64', 'x86', 'arm64'):
            return parts[index + 1]
    return ''

def can_run_built_executables():
    if platform.system() != 'Windows':
        return True

    target_arch = windows_msvc_target_arch(cxx)
    if not target_arch:
        return True

    host_arch = platform.machine().lower()
    if host_arch in ('amd64', 'x86_64'):
        return target_arch == 'x64'
    if host_arch in ('arm64', 'aarch64'):
        return target_arch == 'arm64'
    if host_arch in ('x86', 'i386', 'i686'):
        return target_arch == 'x86'
    return False

if can_run_built_executables():
    config.available_features.add('can-run-built-executables')

def read_compiler_output(*args):
    try:
        return subprocess.check_output(
            [*shlex.split(cxx), *args],
            stderr=subprocess.STDOUT,
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return ''

compiler_version_text = read_compiler_output('--version').lower()
if 'clang' in compiler_version_text:
    config.environment['DINGO_CXX_FAMILY'] = 'clang'
elif 'gcc' in compiler_version_text or 'g++' in compiler_version_text:
    config.environment['DINGO_CXX_FAMILY'] = 'gcc'

compiler_full_version = read_compiler_output('-dumpfullversion')
if compiler_full_version:
    config.environment['DINGO_CXX_VERSION_MAJOR'] = compiler_full_version.split('.', 1)[0]

config.substitutions.append(('%python', python))
if platform.system() == 'Windows':
    config.substitutions.append(
        ('%dingo_cxx', f'"{cxx}" /std:c++20 /I"{os.path.join(project_root, "include")}"')
    )
else:
    config.substitutions.append(
        ('%dingo_cxx', f'{cxx} -std=c++20 -I"{os.path.join(project_root, "include")}"')
    )
config.substitutions.append(('%dingo_project_root', project_root))
config.substitutions.append(('%dingo_lit_root', config.test_source_root))
if 'FILECHECK' in lit_config.params:
    config.substitutions.append(
        ('%filecheck', f"{lit_config.params['FILECHECK']} --dump-input=fail")
    )
elif 'UV' in lit_config.params:
    config.substitutions.append(
        ('%filecheck',
         f"{lit_config.params['UV']} run --locked filecheck --dump-input=fail")
    )
else:
    config.substitutions.append(('%filecheck', 'filecheck --dump-input=fail'))
