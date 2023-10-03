import re
import subprocess

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

    p = subprocess.run("clang-format", stdout=subprocess.PIPE, input=str("".join(lines)).encode("ascii"))
    lines = [line + "\n" for line in p.stdout.decode().split("\n")]

    result = []
    result.append("Example code included from [{0}]({0}):\n".format(file))
    result.append("```c++\n")
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


if __name__ == "__main__":
    file = "README.md"
    with open(file, "r") as f:
        content = f.readlines()
    
    result = process(content)
    if result != content:
        with open(file, "w") as f:
            f.write("".join(result))