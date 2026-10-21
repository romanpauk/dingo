# Configuration for dingo lit tests.

import os
import platform
import shlex
import subprocess
import lit.formats

config.name = 'dingo-lit-tests'
config.test_source_root = os.path.dirname(__file__)
config.test_exec_root = lit_config.params.get('DINGO_LIT_EXEC_ROOT')
if not config.test_exec_root:
    lit_config.fatal('DINGO_LIT_EXEC_ROOT must point at a build directory')
os.makedirs(config.test_exec_root, exist_ok=True)
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
cxx_id = os.path.basename(cxx).lower()
config.environment['DINGO_CXX_ID'] = cxx_id

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
    config.available_features.add('clang')
elif ('gcc' in compiler_version_text or 'g++' in compiler_version_text
      or 'free software foundation' in compiler_version_text):
    config.environment['DINGO_CXX_FAMILY'] = 'gcc'
    config.available_features.add('gcc')
elif cxx_id in ('cl', 'cl.exe') or 'microsoft' in compiler_version_text:
    config.environment['DINGO_CXX_FAMILY'] = 'msvc'
    config.available_features.add('msvc')

compiler_full_version = read_compiler_output('-dumpfullversion')
if compiler_full_version:
    config.environment['DINGO_CXX_VERSION_MAJOR'] = compiler_full_version.split('.', 1)[0]

config.substitutions.append(('%python', python))
if platform.system() == 'Windows':
    config.substitutions.append(
        ('%dingo_cxx', f'"{cxx}" /std:c++20 /DDINGO_IMPLEMENTATION_NEXT=1 '
                       f'/I"{os.path.join(project_root, "include", "next")}" '
                       f'/I"{os.path.join(project_root, "include")}"')
    )
else:
    next_includes = (
        f'-I"{os.path.join(project_root, "include", "next")}" '
        f'-I"{os.path.join(project_root, "include")}"'
    )

    def static_assert_message(standard):
        """Whether the compiler formats a user-generated static_assert
        message with `standard`, as the capability of the library detects it,
        or None when it does not take the standard."""
        probe = '#if !(defined(__cpp_static_assert) && __cpp_static_assert >= 202306L)\n#error\n#endif\n'
        try:
            subprocess.run(
                [*shlex.split(cxx), f'-std={standard}', '-fsyntax-only', '-x', 'c++', '-'],
                input=probe, text=True, check=True, capture_output=True,
            )
            return True
        except subprocess.CalledProcessError as error:
            return None if 'std' in error.stderr and 'invalid' in error.stderr.lower() else False
        except OSError:
            return None

    # The diagnostics of the next implementation per language tier: the
    # newest standard formats its messages, and C++17 prints the literal one
    # on a compiler that formats none there.
    formatted = next((std for std in ('c++26', 'c++2c') if static_assert_message(std)), None)
    if formatted:
        config.available_features.add('static-assert-message')
        config.substitutions.append(
            ('%dingo_next_cxx_formatted', f'{cxx} -std={formatted} {next_includes}')
        )
    if static_assert_message('c++17') is False:
        config.available_features.add('literal-static-assert')
        config.substitutions.append(
            ('%dingo_next_cxx_literal', f'{cxx} -std=c++17 {next_includes}')
        )
    config.substitutions.append(
        ('%dingo_next_cxx', f'{cxx} -std=c++20 {next_includes}')
    )
    config.substitutions.append(
        ('%dingo_cxx', f'{cxx} -std=c++20 -DDINGO_IMPLEMENTATION_NEXT=1 {next_includes}')
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
