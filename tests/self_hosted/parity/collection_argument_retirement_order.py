#!/usr/bin/env python3
"""Structural placement ratchet only; executable identity units own behavior."""
from pathlib import Path
import re
import sys


def admission_order(source):
    source = re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"',
                    lambda match: ' ' * len(match[0]), source)

    def unique(pattern):
        matches = list(re.finditer(pattern, source))
        if len(matches) != 1:
            raise ValueError('missing/duplicate order boundary')
        return matches[0]

    def scope(pattern):
        opening = unique(pattern).end() - 1
        depth = 1
        for pos in range(opening + 1, len(source)):
            depth += (source[pos] == '{') - (source[pos] == '}')
            if depth == 0:
                return opening, pos
        raise ValueError('unbalanced order boundary')

    def inside(position, boundary):
        return boundary[0] < position < boundary[1]

    def depth(position):
        return source[:position].count('{') - source[:position].count('}')

    def call_end(position):
        opening = source.index('(', position)
        nesting = 1
        for pos in range(opening + 1, len(source)):
            nesting += (source[pos] == '(') - (source[pos] == ')')
            if nesting == 0:
                end = re.match(r'\s*;', source[pos + 1:])
                if end is None:
                    raise ValueError('missing statement terminator')
                return pos + 1 + end.end()
        raise ValueError('unbalanced admission call')

    try:
        owner = scope(r'func\s+SemanticAstCollectionOwnershipScanIntoVerdict\([\s\S]*?\)\s*->\s*Void\s*\{')
        roots = scope(r'while\s+root_slot\s*<\s*ArrayLength\(surfaces\.expression_graph\.roots\)\s*\{')
        present = scope(r'if\s+surfaces\.expression_graph\.has_roots\[root_slot\]\s*\{')
        events = scope(r'while\s+ArrayLength\(pending\)\s*>\s*0\s*\{')
        argument = scope(r'if\s+IsSome\(kind\)\s*&&\s*UnwrapOption\(kind\)\s*==\s*AstExpressionNodeCallArgument\(\)\s*\{')
        guard = scope(r'if\s*!argument_verdict\.ok\s*\{')
        commit = scope(r'if\s+exact_use\.retirement_source\s*>\s*0\s*\{')
        call = unique(r'SemanticAstCollectionCallArgumentVerdict\(').start()
        call_write = unique(r'MapSet\(\s*retired_arguments,\s*ToString\(\s*exact_use\.retirement_source\s*\),\s*exact_use\.retirement_site\s*\);').start()
        definition = unique(r'SemanticAstCollectionOwnedArgumentDefinitionAtSlot\(').start()
        definition_guard = scope(r'if\s+definition_use\.found\s*\{')
        definition_commit = scope(r'if\s+definition_use\.retirement_source\s*>\s*0\s*\{')
        definition_write = unique(r'MapSet\(\s*retired_arguments,\s*ToString\(\s*definition_use\.retirement_source\s*\),\s*definition_use\.retirement_site\s*\);').start()
        activation = unique(r'SemanticAstCollectionCompleteDefinition\(').start()
        commit_start = unique(r'if\s+exact_use\.retirement_source\s*>\s*0\s*\{').start()
        definition_commit_start = unique(r'if\s+definition_use\.retirement_source\s*>\s*0\s*\{').start()
        unique(r'let\s+retired_arguments:\s*HashMap<String,\s*Int>\s*=\s*MapNew\(\);')
        if len(re.findall(r'\bMapSet\s*\(\s*retired_arguments\s*,', source)) != 2:
            return False
        # These locally issued facts are immutable between issuance and use.
        # Reject assignments even when spacing or the RHS expression changes.
        if re.search(r'\b(?:argument_verdict|exact_use|definition_use)\b'
                     r'(?:\s*\.\s*\w+)*\s*=(?!=)', source):
            return False
        return (depth(roots[0]) == depth(owner[0]) + 1 and
                depth(present[0]) == depth(roots[0]) + 1 and
                depth(events[0]) == depth(present[0]) + 1 and
                depth(argument[0]) == depth(events[0]) + 1 and
                inside(argument[0], events) and inside(argument[1], events) and
                argument[0] < call < guard[0] < guard[1] < commit[0] < call_write < commit[1] < argument[1] and
                depth(guard[0]) == depth(commit[0]) == depth(argument[0]) + 1 and
                not source[call_end(call):unique(r'if\s*!argument_verdict\.ok\s*\{').start()].strip() and
                not source[guard[1] + 1:commit_start].strip() and
                '{' not in source[guard[0] + 1:guard[1]] and
                '{' not in source[commit[0] + 1:commit[1]] and
                re.fullmatch(r'\s*ArrayDrop\(pending\);\s*ArrayDrop\(prefixes\);\s*'
                             r'verdict\s*=\s*argument_verdict;\s*return;\s*',
                             source[guard[0] + 1:guard[1]]) is not None and
                events[1] < definition < definition_guard[0] < definition_guard[1] < definition_commit[0] < definition_write < definition_commit[1] < activation and
                depth(definition) == depth(definition_guard[0]) == depth(definition_commit[0]) == depth(present[0]) + 1 and
                not source[call_end(definition):unique(r'if\s+definition_use\.found\s*\{').start()].strip() and
                not source[definition_guard[1] + 1:definition_commit_start].strip() and
                '{' not in source[definition_guard[0] + 1:definition_guard[1]] and
                '{' not in source[definition_commit[0] + 1:definition_commit[1]] and
                'return;' in source[definition_guard[0]:definition_guard[1]])
    except ValueError:
        return False


def main():
    path = Path(sys.argv[1])
    source = path.read_text(encoding='utf-8')
    if not admission_order(source):
        raise SystemExit('retirement publication crossed an admission/activation boundary')
    commit = re.search(r'                    if exact_use\.retirement_source > 0 \{\n.*?\n                    \}', source, re.S)
    guard = re.search(r'                    if !argument_verdict\.ok \{\n.*?\n                    \}', source, re.S)
    if commit is None or guard is None:
        raise SystemExit('missing mutation anchor')
    early = source[:guard.start()] + commit[0] + '\n' + source[guard.start():commit.start()] + source[commit.end():]
    # Put the rejected-call write back inside the guard after removing the
    # original commit, rather than accepting an omitted/duplicate write.
    rejected = source[:commit.start()] + source[commit.end():]
    rejected = rejected.replace(guard[0], guard[0].replace('verdict = argument_verdict;', commit[0] + '\n                        verdict = argument_verdict;'))
    no_return = source.replace('verdict = argument_verdict; return;', 'verdict = argument_verdict;', 1)
    definition = re.search(r'            if definition_use\.retirement_source > 0 \{\n.*?\n            \}', source, re.S)
    if definition is None:
        raise SystemExit('missing definition mutation anchor')
    late = source[:definition.start()] + source[definition.end():]
    late = late.replace('        if !completed.ok', definition[0] + '\n        if !completed.ok', 1)
    wrong_site = source.replace('exact_use.retirement_source), exact_use.retirement_site', 'exact_use.retirement_source), 0', 1)
    wrong_definition_site = source.replace('definition_use.retirement_source), definition_use.retirement_site', 'definition_use.retirement_source), 0', 1)
    guarded_commit = source.replace(commit[0], '                    if false {\n' + commit[0] + '\n                    }', 1)
    guarded_return = source.replace('verdict = argument_verdict; return;', 'if MapSize(retired_arguments) > 0 { verdict = argument_verdict; return; }', 1)
    argument_return = source.replace(commit[0], '                    return;\n' + commit[0], 1)
    definition_return = source.replace(definition[0], '            return;\n' + definition[0], 1)
    argument_start = source.index('                if IsSome(kind) && UnwrapOption(kind) == AstExpressionNodeCallArgument() {')
    argument_end = source.index('                if step.completed_call >= 0 {', argument_start)
    argument_block = source[argument_start:argument_end]
    unreachable_argument = source.replace(argument_block, '                if false {\n' + argument_block + '                }\n', 1)
    definition_start = source.index('            let definition_use:')
    definition_end = source.index('\n        }\n        let completed:', definition_start)
    definition_block = source[definition_start:definition_end]
    unreachable_definition = source.replace(definition_block, '            if false {\n' + definition_block + '\n            }', 1)
    failed_write = source.replace('if !argument_verdict.ok {', 'if !argument_verdict.ok {\n'
                                 'MapSet (retired_arguments, ToString(0 + exact_use.retirement_source), 0 + exact_use.retirement_site);', 1)
    failed_definition_write = source.replace('if definition_use.found {', 'if definition_use.found {\n'
                                            'MapSet (retired_arguments, ToString(0 + definition_use.retirement_source), 0 + definition_use.retirement_site);', 1)
    overwrite_admission = source.replace('if !argument_verdict.ok {', 'argument_verdict.ok = true;\nif !argument_verdict.ok {', 1)
    overwrite_pending = source.replace('if !argument_verdict.ok {', 'exact_use.retirement_source = 1;\nif !argument_verdict.ok {', 1)
    overwrite_definition = source.replace('if definition_use.found {', 'definition_use.retirement_site = 1;\nif definition_use.found {', 1)
    for name, mutant in (('early', early), ('rejected', rejected), ('no-return', no_return), ('after-activation', late),
                         ('wrong-site', wrong_site), ('wrong-definition-site', wrong_definition_site),
                         ('guarded-commit', guarded_commit), ('guarded-return', guarded_return),
                         ('argument-return', argument_return), ('definition-return', definition_return),
                         ('unreachable-argument', unreachable_argument), ('unreachable-definition', unreachable_definition),
                         ('failed-write-expression', failed_write), ('failed-definition-write-expression', failed_definition_write),
                         ('overwrite-admission', overwrite_admission), ('overwrite-pending', overwrite_pending),
                         ('overwrite-definition', overwrite_definition)):
        if mutant == source:
            raise SystemExit(f'retirement order mutation not planted: {name}')
        if admission_order(mutant):
            raise SystemExit(f'retirement order mutant admitted: {name}')
    print('retirement-order: source placement + seventeen mutation refusals PASS (structural only)')


if __name__ == '__main__':
    main()
