import sys

with open('src/Parser.cpp') as f:
    content = f.read()

start = content.find('StmtPtr Parser::modelStatement()')
end = content.find('StmtPtr Parser::interfaceStatement()')
block = content[start:end]
lines = block.split('\n')
stack = []
in_str = False

for i, line in enumerate(lines):
    # filter out comments roughly
    line_code = line.split('//')[0]
    for j, c in enumerate(line_code):
        if c == '"':
            # handle escapes roughly
            if j == 0 or line_code[j-1] != '\\':
                in_str = not in_str
        if in_str:
            continue
        if c == '{':
            stack.append((i+1, line))
        elif c == '}':
            if stack:
                stack.pop()
            else:
                print(f'Unbalanced closing brace at {i+1}: {line}')

if stack:
    print('Unclosed braces:')
    for i, line in stack:
        print(f'{i}: {line}')
else:
    print('Perfectly balanced!')
