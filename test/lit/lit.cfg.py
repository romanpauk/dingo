# Configuration for dingo lit tests.

import os
import platform
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
