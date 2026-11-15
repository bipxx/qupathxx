"""Small structural queries shared by the source architecture contracts."""
import re
from gui_command_inventory import calls, code_mask

def compact(expression):
    # Preserve literal bytes; whitespace outside literals is not C++ structure.
    return ''.join(m[0] for m in re.finditer(r'"(?:[^"\\]|\\.)*"|//[^\n]*|/\*.*?\*/|\S', expression, re.S)
                   if not m[0].startswith(('//','/*')))

def has_call(source, name, *arguments):
    return any(len(args)>=len(arguments) and all(compact(a)==compact(b)
               for a,b in zip(args,arguments)) for args in calls(source,re.escape(name)))

def call_position(source, name, *arguments):
    positions=re.finditer(r'\b'+re.escape(name)+r'\s*\(',code_mask(source))
    for match,args in zip(positions,calls(source,re.escape(name))):
        if len(args)>=len(arguments) and all(compact(a)==compact(b) for a,b in zip(args,arguments)):
            return match.start()
    raise AssertionError(f'Missing call: {name}{arguments}')
