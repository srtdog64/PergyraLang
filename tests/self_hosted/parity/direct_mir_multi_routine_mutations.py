#!/usr/bin/env python3
"""Create exact callable-identity negatives for the scalar multi-routine gate."""

import json
import sys


def direct_call_nodes(document):
    for routine in document.get("routines", []):
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                for lane in ("expr0_graph", "expr1_graph"):
                    graph = instruction.get(lane)
                    if not isinstance(graph, dict):
                        continue
                    for node in graph.get("nodes", []):
                        if node.get("call_target_kind") == "direct":
                            yield node


def option_int_instructions(document):
    for routine in document.get("routines", []):
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                if instruction.get("abi_type_name") == "Option<Int>":
                    yield instruction


def option_string_instructions(document):
    for routine in document.get("routines", []):
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                if instruction.get("abi_type_name") == "Option<String>":
                    yield instruction


def option_bool_instructions(document):
    for routine in document.get("routines", []):
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                if instruction.get("abi_type_name") == "Option<Bool>":
                    yield instruction


def nominal_instructions(document):
    declarations = document.get("decls", document.get("declarations", []))
    candidates = [
        declaration
        for declaration in declarations
        if declaration.get("kind") == "struct"
        and [field.get("type") for field in declaration.get("fields", [])]
        == ["Int", "Int"]
    ]
    if len(candidates) != 1:
        return
    nominal_name = candidates[0].get("name")
    for routine in document.get("routines", []):
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                if instruction.get("abi_type_name") == nominal_name:
                    yield instruction


def array_int_value_result_params(document):
    for routine in document.get("routines", []):
        for param in routine.get("params", []):
            if (
                param.get("type") == "Array<Int>"
                and param.get("carriage") == "value-result"
            ):
                yield param


def abi_value_params(document, type_name):
    for routine in document.get("routines", []):
        for param in routine.get("params", []):
            if (
                param.get("type") == type_name
                and param.get("carriage") == "value"
            ):
                yield param


def array_string_value_result_params(document):
    for routine in document.get("routines", []):
        for param in routine.get("params", []):
            if (
                param.get("type") == "Array<String>"
                and param.get("carriage") == "value-result"
            ):
                yield param


def array_string_value_params(document):
    for routine in document.get("routines", []):
        for param in routine.get("params", []):
            if (
                param.get("type") == "Array<String>"
                and param.get("carriage") == "value"
            ):
                yield param


def set_string_value_param(document):
    for routine in document.get("routines", []):
        if routine.get("name") != "ContainsPath":
            continue
        for param in routine.get("params", []):
            if param.get("name") == "paths" and param.get("type") == "Set<String>":
                return param
    return None


def set_string_value_result_param(document):
    for routine in document.get("routines", []):
        if routine.get("name") != "AddPath":
            continue
        for param in routine.get("params", []):
            if (
                param.get("name") == "paths"
                and param.get("type") == "Set<String>"
                and param.get("carriage") == "value-result"
            ):
                return param
    return None


def array_string_readonly_ref_param(document):
    for routine in document.get("routines", []):
        if routine.get("name") != "ContainsString":
            continue
        for param in routine.get("params", []):
            if (
                param.get("name") == "values"
                and param.get("type") == "Array<String>"
                and param.get("carriage") == "readonly-ref"
            ):
                return param
    return None


def entrypoint_void_return_instruction(document):
    for routine in document.get("routines", []):
        if routine.get("name") != "Main":
            continue
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                if (
                    instruction.get("kind") == "return"
                    and instruction.get("source_type") == "AST_RETURN_VOID"
                ):
                    return instruction
    return None


def namespace_internal_call_node(document):
    for node in direct_call_nodes(document):
        if node.get("call_target_name") == "InternalNames_Fact1":
            return node
    return None


def namespace_qualified_call_node(document):
    for routine in document.get("routines", []):
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                for lane in ("expr0_graph", "expr1_graph"):
                    graph = instruction.get(lane)
                    if not isinstance(graph, dict):
                        continue
                    for node in graph.get("nodes", []):
                        if (node.get("call_target_kind") == "namespace" and
                                node.get("call_target_name") ==
                                "InternalNames_Fact2"):
                            return node
    return None


def set_has_call_node(document):
    for routine in document.get("routines", []):
        if routine.get("name") != "ContainsPath":
            continue
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                for lane in ("expr0_graph", "expr1_graph"):
                    graph = instruction.get(lane)
                    if not isinstance(graph, dict):
                        continue
                    for node in graph.get("nodes", []):
                        if node.get("call_target_name") == "SetHas":
                            return node
    return None


def array_string_return_instructions(document):
    for routine in document.get("routines", []):
        if routine.get("return") != "Array<String>":
            continue
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                if (
                    instruction.get("kind") == "return"
                    and instruction.get("abi_type_name") == "Array<String>"
                ):
                    yield instruction


def owned_string_parameter(document):
    for routine in document.get("routines", []):
        if routine.get("name") != "ReleaseOwnedString":
            continue
        params = routine.get("params", [])
        if len(params) == 1:
            return params[0]
    return None


def owned_string_drop_call_node(document):
    for routine in document.get("routines", []):
        if routine.get("name") != "ReleaseOwnedString":
            continue
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                if instruction.get("expr0") == "ArrayDropOwnedStrings(owned_values)":
                    graph = instruction.get("expr0_graph", {})
                    for node in graph.get("nodes", []):
                        if node.get("call_target_name") == "ArrayDropOwnedStrings":
                            return node
    return None


def array_int_return_instructions(document):
    for routine in document.get("routines", []):
        if routine.get("return") != "Array<Int>":
            continue
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                if instruction.get("kind") == "return" and \
                        instruction.get("abi_type_name") == "Array<Int>":
                    yield instruction


def array_instructions(document, type_name):
    for routine in document.get("routines", []):
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                if instruction.get("abi_type_name") == type_name:
                    yield instruction


def logical_record_declarations(document):
    declarations = document.get("decls", document.get("declarations", []))
    return [
        declaration
        for declaration in declarations
        if declaration.get("kind") == "struct"
        and len(declaration.get("fields", [])) == 4
        and all(
            field.get("type") in {"Bool", "Int", "String"}
            for field in declaration.get("fields", [])
        )
    ]


def callable_referenced_logical_record(document):
    return_types = {
        routine.get("return") for routine in document.get("routines", [])
    }
    candidates = [
        declaration
        for declaration in logical_record_declarations(document)
        if declaration.get("name") in return_types
    ]
    candidates = [
        declaration
        for declaration in candidates
        if declaration.get("name") == "ProbeFact"
    ]
    if len(candidates) != 1:
        return None
    return candidates[0]


def logical_record_instructions(document, name):
    for routine in document.get("routines", []):
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                if instruction.get("abi_type_name") == name:
                    yield instruction


def logical_record_readonly_params(document):
    names = {
        declaration.get("name")
        for declaration in logical_record_declarations(document)
    }
    for routine in document.get("routines", []):
        for param in routine.get("params", []):
            if (
                param.get("type") in names
                and param.get("carriage") == "readonly-ref"
            ):
                yield param


def logical_record_value_params(document, type_name):
    for routine in document.get("routines", []):
        for param in routine.get("params", []):
            if (
                param.get("type") == type_name
                and param.get("carriage") == "value"
            ):
                yield param


def declaration_named(document, name):
    declarations = document.get("decls", document.get("declarations", []))
    return next(
        (declaration for declaration in declarations
         if declaration.get("name") == name),
        None,
    )


def phi_named(document, routine_name, local_name):
    for routine in document.get("routines", []):
        if routine.get("name") != routine_name:
            continue
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                if (instruction.get("kind") == "phi"
                        and instruction.get("name") == local_name):
                    return instruction
    return None


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    input_path, kind, output_path = sys.argv[1:]
    with open(input_path, encoding="utf-8") as source:
        document = json.load(source)

    if kind == "missing-call-target":
        node = next(direct_call_nodes(document), None)
        if node is None:
            raise SystemExit("fixture has no direct call target")
        node["call_target_syntax_id"] = 999999
    elif kind == "duplicate-routine-identity":
        routines = document.get("routines", [])
        if len(routines) < 2:
            raise SystemExit("fixture has fewer than two routines")
        routines[1]["source_syntax_id"] = routines[0]["source_syntax_id"]
    elif kind == "option-abi-layout":
        instruction = next(option_int_instructions(document), None)
        if instruction is None:
            raise SystemExit("fixture has no Option<Int> ABI instruction")
        fields = instruction.get("abi_layout", {}).get("fields", [])
        if len(fields) != 2:
            raise SystemExit("fixture has no two-field Option<Int> ABI")
        fields[1]["offset"] = 0
    elif kind == "option-string-abi-layout":
        instruction = next(option_string_instructions(document), None)
        if instruction is None:
            raise SystemExit("fixture has no Option<String> ABI instruction")
        fields = instruction.get("abi_layout", {}).get("fields", [])
        if len(fields) != 2:
            raise SystemExit("fixture has no two-field Option<String> ABI")
        fields[1]["offset"] = 4
    elif kind == "option-string-none-call-target":
        node = next((node for node in direct_call_nodes(document)
                     if node.get("call_target_name") ==
                     "OptionStringIsMissing"), None)
        if node is None:
            raise SystemExit("fixture has no nested Option<String> None call")
        node["call_target_syntax_id"] = 0
    elif kind == "option-bool-abi-layout":
        instruction = next(option_bool_instructions(document), None)
        if instruction is None:
            raise SystemExit("fixture has no Option<Bool> ABI instruction")
        fields = instruction.get("abi_layout", {}).get("fields", [])
        if len(fields) != 2:
            raise SystemExit("fixture has no two-field Option<Bool> ABI")
        fields[1]["offset"] = 0
    elif kind == "two-int-nominal-abi-layout":
        instruction = next(nominal_instructions(document), None)
        if instruction is None:
            raise SystemExit("fixture has no nominal ABI instruction")
        fields = instruction.get("abi_layout", {}).get("fields", [])
        if len(fields) != 2:
            raise SystemExit("fixture has no two-field nominal ABI")
        fields[1]["offset"] = 0
    elif kind == "array-int-value-result-abi-layout":
        param = next(array_int_value_result_params(document), None)
        if param is None:
            raise SystemExit("fixture has no value-result Array<Int> parameter")
        fields = param.get("abi_layout", {}).get("fields", [])
        if len(fields) != 4:
            raise SystemExit("fixture has no four-field Array<Int> ABI")
        fields[1]["offset"] = 0
    elif kind == "array-int-value-result-carriage":
        param = next(array_int_value_result_params(document), None)
        if param is None:
            raise SystemExit("fixture has no value-result Array<Int> parameter")
        param["carriage"] = "value"
    elif kind in {"array-int-value-abi-layout", "array-bool-value-abi-layout"}:
        type_name = "Array<Int>" if kind.startswith("array-int") else "Array<Bool>"
        param = next(abi_value_params(document, type_name), None)
        if param is None:
            raise SystemExit(f"fixture has no value {type_name} parameter")
        fields = param.get("abi_layout", {}).get("fields", [])
        if len(fields) != 4:
            raise SystemExit(f"fixture has no four-field {type_name} ABI")
        fields[1]["offset"] = 0
    elif kind in {"array-int-value-carriage", "array-bool-value-carriage"}:
        type_name = "Array<Int>" if kind.startswith("array-int") else "Array<Bool>"
        param = next(abi_value_params(document, type_name), None)
        if param is None:
            raise SystemExit(f"fixture has no value {type_name} parameter")
        param["carriage"] = "readonly-ref"
    elif kind in {"array-int-value-pass-shape", "array-bool-value-pass-shape"}:
        type_name = "Array<Int>" if kind.startswith("array-int") else "Array<Bool>"
        param = next(abi_value_params(document, type_name), None)
        if param is None:
            raise SystemExit(f"fixture has no value {type_name} parameter")
        param["pass"] = "indirect"
    elif kind.startswith("option-string-value-") or kind.startswith("option-int-value-"):
        type_name = "Option<String>" if kind.startswith("option-string") else "Option<Int>"
        param = next(abi_value_params(document, type_name), None)
        if param is None:
            raise SystemExit(f"fixture has no value {type_name} parameter")
        if kind.endswith("abi-layout"):
            fields = param.get("abi_layout", {}).get("fields", [])
            if len(fields) != 2:
                raise SystemExit(f"fixture has no two-field {type_name} ABI")
            fields[1]["offset"] = 0
        elif kind.endswith("carriage"):
            param["carriage"] = "readonly-ref"
        elif kind.endswith("pass-shape"):
            param["pass"] = "indirect"
        else:
            raise SystemExit(f"unknown ABI value mutation: {kind}")
    elif kind == "array-int-return-abi-layout":
        instruction = next(array_int_return_instructions(document), None)
        if instruction is None:
            raise SystemExit("fixture has no Array<Int> return")
        fields = instruction.get("abi_layout", {}).get("fields", [])
        if len(fields) != 4:
            raise SystemExit("fixture has no four-field Array<Int> return ABI")
        fields[1]["offset"] = 0
    elif kind == "array-int-return-kind":
        instruction = next(array_int_return_instructions(document), None)
        if instruction is None:
            raise SystemExit("fixture has no Array<Int> return")
        instruction["kind"] = "def"
    elif kind == "array-string-value-result-abi-layout":
        param = next(array_string_value_result_params(document), None)
        if param is None:
            raise SystemExit(
                "fixture has no value-result Array<String> parameter"
            )
        fields = param.get("abi_layout", {}).get("fields", [])
        if len(fields) != 4:
            raise SystemExit("fixture has no four-field Array<String> ABI")
        fields[1]["offset"] = 0
    elif kind == "array-string-value-result-carriage":
        param = next(array_string_value_result_params(document), None)
        if param is None:
            raise SystemExit(
                "fixture has no value-result Array<String> parameter"
            )
        param["carriage"] = "value"
    elif kind == "owned-string-push-carriage":
        routine = next((row for row in document.get("routines", [])
                        if row.get("name") == "AppendOwnedMarker"), None)
        if routine is None or not routine.get("params"):
            raise SystemExit("fixture has no owned String push routine")
        routine["params"][0]["carriage"] = "value"
    elif kind == "array-string-value-abi-layout":
        param = next(array_string_value_params(document), None)
        if param is None:
            raise SystemExit("fixture has no value Array<String> parameter")
        fields = param.get("abi_layout", {}).get("fields", [])
        if len(fields) != 4:
            raise SystemExit("fixture has no four-field Array<String> ABI")
        fields[1]["offset"] = 0
    elif kind == "array-string-value-carriage":
        param = next(array_string_value_params(document), None)
        if param is None:
            raise SystemExit("fixture has no value Array<String> parameter")
        param["carriage"] = "readonly-ref"
    elif kind == "array-string-value-pass-shape":
        param = next(array_string_value_params(document), None)
        if param is None:
            raise SystemExit("fixture has no value Array<String> parameter")
        param["pass"] = "indirect"
    elif kind.startswith("set-string-value-") and not kind.startswith(
        "set-string-value-result-"
    ):
        param = set_string_value_param(document)
        if param is None:
            raise SystemExit("fixture has no value Set<String> parameter")
        if kind.endswith("carriage"):
            param["carriage"] = "readonly-ref"
        elif kind.endswith("pass-shape"):
            param["pass"] = "indirect"
        elif kind.endswith("type"):
            param["type"] = "Set<Int>"
        elif kind.endswith("abi-required"):
            param["abi_layout_required"] = True
        else:
            raise SystemExit(f"unknown Set<String> value mutation: {kind}")
    elif kind.startswith("set-string-value-result-"):
        param = set_string_value_result_param(document)
        if param is None:
            raise SystemExit("fixture has no value-result Set<String> parameter")
        if kind.endswith("carriage"):
            param["carriage"] = "readonly-ref"
        elif kind.endswith("pass-shape"):
            param["pass"] = "indirect"
        elif kind.endswith("resource"):
            param["resource"] = "io"
        elif kind.endswith("type"):
            param["type"] = "Set<Int>"
        elif kind.endswith("abi-required"):
            param["abi_layout_required"] = True
        else:
            raise SystemExit(
                f"unknown Set<String> value-result mutation: {kind}"
            )
    elif kind.startswith("array-string-readonly-ref-"):
        param = array_string_readonly_ref_param(document)
        if param is None:
            raise SystemExit("fixture has no read-only Array<String> parameter")
        if kind.endswith("carriage"):
            param["carriage"] = "owner-handle"
        elif kind.endswith("pass-shape"):
            param["pass"] = "indirect"
        elif kind.endswith("resource"):
            param["resource"] = "io"
        elif kind.endswith("type"):
            param["type"] = "Array<Int>"
        elif kind.endswith("abi-required"):
            param["abi_layout_required"] = False
        else:
            raise SystemExit(
                f"unknown read-only Array<String> mutation: {kind}"
            )
    elif kind == "entrypoint-void-return-source":
        instruction = entrypoint_void_return_instruction(document)
        if instruction is None:
            raise SystemExit("fixture has no explicit entrypoint void return")
        instruction["source_type"] = "AST_RETURN"
    elif kind == "namespace-internal-call-syntax-id":
        node = namespace_internal_call_node(document)
        if node is None:
            raise SystemExit("fixture has no namespace-internal direct call")
        node["call_target_syntax_id"] = 0
    elif kind == "namespace-qualified-call-syntax-id":
        node = namespace_qualified_call_node(document)
        if node is None:
            raise SystemExit("fixture has no namespace-qualified call")
        node["call_target_syntax_id"] = 0
    elif kind == "set-string-has-call-target":
        node = set_has_call_node(document)
        if node is None:
            raise SystemExit("fixture has no SetHas call target")
        node["call_target_name"] = "ArrayLength"
    elif kind == "array-string-owned-return-abi-layout":
        instruction = next(array_string_return_instructions(document), None)
        if instruction is None:
            raise SystemExit("fixture has no owned Array<String> return")
        fields = instruction.get("abi_layout", {}).get("fields", [])
        if len(fields) != 4:
            raise SystemExit("fixture has no four-field Array<String> return ABI")
        fields[1]["offset"] = 0
    elif kind == "array-string-owned-return-kind":
        instruction = next(array_string_return_instructions(document), None)
        if instruction is None:
            raise SystemExit("fixture has no owned Array<String> return")
        instruction["kind"] = "def"
    elif kind == "owned-string-parameter-carriage":
        param = owned_string_parameter(document)
        if param is None:
            raise SystemExit("fixture has no owned String parameter")
        param["carriage"] = "value-result"
    elif kind == "owned-string-parameter-type":
        param = owned_string_parameter(document)
        if param is None:
            raise SystemExit("fixture has no owned String parameter")
        param["type"] = "Int"
    elif kind == "owned-string-parameter-pass":
        param = owned_string_parameter(document)
        if param is None:
            raise SystemExit("fixture has no owned String parameter")
        param["pass"] = "indirect"
    elif kind == "owned-string-drop-call-target":
        node = owned_string_drop_call_node(document)
        if node is None:
            raise SystemExit("fixture has no owned String drop call target")
        node["call_target_name"] = ""
    elif kind == "logical-record-field-order":
        declaration = callable_referenced_logical_record(document)
        if declaration is None:
            raise SystemExit("fixture has no unique referenced logical record")
        fields = declaration.get("fields", [])
        if len(fields) != 4:
            raise SystemExit("logical record does not have four fields")
        fields[0], fields[1] = fields[1], fields[0]
    elif kind == "logical-record-instruction-layout":
        declaration = callable_referenced_logical_record(document)
        if declaration is None:
            raise SystemExit("fixture has no unique referenced logical record")
        instruction = next(
            logical_record_instructions(document, declaration.get("name")),
            None,
        )
        if instruction is None:
            raise SystemExit("fixture has no logical-record instruction")
        instruction["abi_layout_required"] = True
    elif kind == "logical-record-cross-identity":
        node = next(
            (
                node
                for node in direct_call_nodes(document)
                if node.get("call_target_name") == "ArrayObjectTableFact"
            ),
            None,
        )
        if node is None:
            raise SystemExit("fixture has no array-table constructor")
        node["call_target_name"] = "ObjectTableFact"
    elif kind == "logical-record-local-declaration-identity":
        node = next(
            (
                node
                for node in direct_call_nodes(document)
                if node.get("call_target_name") == "LocalTableFact"
            ),
            None,
        )
        if node is None:
            raise SystemExit("fixture has no local-only record constructor")
        node["call_target_name"] = "UnusedFact"
    elif kind == "logical-record-readonly-carriage":
        param = next(logical_record_readonly_params(document), None)
        if param is None:
            raise SystemExit("fixture has no readonly logical-record parameter")
        param["carriage"] = "value"
    elif kind == "logical-record-recursive-cycle":
        declaration = declaration_named(document, "LeafFact")
        if declaration is None or len(declaration.get("fields", [])) != 4:
            raise SystemExit("fixture has no LeafFact declaration")
        declaration["fields"][1]["type"] = "DocumentFact"
    elif kind == "logical-record-nested-cross-identity":
        node = next(
            (
                node
                for node in direct_call_nodes(document)
                if node.get("call_target_name") == "AlternateLeafFact"
            ),
            None,
        )
        if node is None:
            raise SystemExit("fixture has no AlternateLeafFact constructor")
        node["call_target_name"] = "LeafFact"
    elif kind == "logical-record-value-phi-identity":
        phi = phi_named(document, "BuildDocumentFact", "root")
        if phi is None or len(phi.get("uses", [])) != 2:
            raise SystemExit("fixture has no root value phi")
        phi["uses"][0] = "alternate.7"
    elif kind == "logical-record-nested-array-bool-field-order":
        declaration = declaration_named(document, "ProgramIndex")
        if declaration is None or len(declaration.get("fields", [])) != 3:
            raise SystemExit("fixture has no ProgramIndex declaration")
        fields = declaration["fields"]
        fields[0], fields[1] = fields[1], fields[0]
    elif kind == "logical-record-nested-array-bool-cross-identity":
        node = next(
            (
                node
                for node in direct_call_nodes(document)
                if node.get("call_target_name") == "ProgramIndex"
            ),
            None,
        )
        if node is None:
            raise SystemExit("fixture has no ProgramIndex constructor")
        node["call_target_name"] = "WrongProgramIndex"
    elif kind == "logical-record-nested-array-bool-abi-layout":
        instruction = next(array_instructions(document, "Array<Bool>"), None)
        if instruction is None:
            raise SystemExit("fixture has no Array<Bool> instruction")
        fields = instruction.get("abi_layout", {}).get("fields", [])
        if len(fields) != 4:
            raise SystemExit("fixture has no four-field Array<Bool> ABI")
        fields[1]["offset"] = 0
    elif kind == "logical-record-nested-value-pass-shape":
        param = next(logical_record_value_params(document, "ProgramIndex"), None)
        if param is None:
            raise SystemExit("fixture has no value ProgramIndex parameter")
        param["pass"] = "indirect"
    elif kind == "logical-record-collection-cross-identity":
        node = next((node for node in direct_call_nodes(document)
                     if node.get("call_target_name") == "CollectionIndex"), None)
        if node is None:
            raise SystemExit("fixture has no CollectionIndex constructor")
        node["call_target_name"] = "WrongCollectionIndex"
    elif kind in {"logical-record-array-int-abi-layout",
                  "logical-record-array-string-abi-layout"}:
        type_name = ("Array<Int>" if kind.endswith("int-abi-layout")
                     else "Array<String>")
        instruction = next(array_instructions(document, type_name), None)
        if instruction is None:
            raise SystemExit(f"fixture has no {type_name} instruction")
        fields = instruction.get("abi_layout", {}).get("fields", [])
        if len(fields) != 4:
            raise SystemExit(f"fixture has no four-field {type_name} ABI")
        fields[1]["offset"] = 0
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output_path, "w", encoding="utf-8", newline="\n") as output:
        json.dump(document, output, separators=(",", ":"))
        output.write("\n")
if __name__ == "__main__":
    main()
