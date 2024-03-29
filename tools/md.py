import argparse
import mdformat
import os
import re
import subprocess
import sys

def file_type(file):
    if os.path.splitext(file)[1].lower() in ['.cpp', '.hpp', '.h']:
        return 'c++'
    
    if os.path.split(file)[1] == 'CMakeLists.txt':
        return 'CMake'

    raise Exception('unsupported file type: {}'.format(file))


def include(file, scope=None):
    lines = []
    with open(file, 'r') as f:
        if not scope:
            lines = f.readlines()
        else:
            in_scope = False
            for line in f.readlines():
                if line.strip().startswith(scope):
                    in_scope = not in_scope
                elif in_scope:
                    lines.append(line)

    if file_type(file) == 'c++':
        p = subprocess.run(["clang-format"], stdout=subprocess.PIPE, input=str("".join(lines)).encode("ascii"))
        lines = [line + "\n" for line in p.stdout.decode().split("\n")]
        if lines[-1].strip() == "":
            lines.pop()

    result = []
    result.append("Example code included from [{0}]({0}):\n".format(file))
    result.append("```{}\n".format(file_type(file)))
    result.extend(lines)
    result.append("```\n")
    
    return result


def process(lines):    
    stack = []
    result = []
    for i, line in enumerate(lines):
        m = re.search(r'<!--\s*{(.*)-->', line)
        if m:
            result.append(line)
            stack.append((i, m.group(1).strip(), []))
            continue
        
        m = re.search(r'<!--\s*}\s*-->', line)
        if m:
            data = stack.pop()
            (code, buffer) = (data[1].strip(), data[2])
            for l in eval(code):
                result.append(l)
            result.append(line)
            continue
        
        if stack:
            stack[-1][2].append(line)
        else:
            result.append(line)

    return result


def format(lines):
    return mdformat.text(''.join(lines), options={ 'number': True, 'wrap': 80, })


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("filenames", help="file(s) to process", nargs='+')
    parser.add_argument("--mode", choices=["verify", "update"], default="verify", help="'update' the file or 'verify' that it is updated")
    args = parser.parse_args()
    
    for file in args.filenames:
        with open(file, "r") as f:
            content = f.readlines()
    
        result = format(process(content))
        if result != ''.join(content):
            if args.mode == "update":
                with open(file, "w") as f:
                    f.write(result)
                print("{} updated".format(file))
            elif args.mode == "verify":
                print("{} requires update, run md.py in update mode".format(file))
                sys.exit(1)
        else:
            print("{} is up-to-date".format(file))

    sys.exit(0)