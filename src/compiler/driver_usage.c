/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "driver_app.h"

#include <stdio.h>

void
driver_print_usage(void)
{
    printf(
        "Usage:\n"
        "  pgy <source.pgy>              compile to native binary\n"
        "  pgy <source.pgy> -o <out>     name the emitted native binary\n"
        "  pgy <source.pgy> --emit-c     stop after generating C\n"
        "  pgy <source.pgy> --emit-c -o <out.c>\n"
        "  pgy --self-driver <source.pgy>  use the bounded Pergyra DRV-2 path\n"
        "  pgy <source.pgy> --emit-llvm -o <out.ll>\n"
#ifdef PGY_LLVM_ENABLED
        "  (LLVM + --run): if -o ends with .o/.obj, executable target becomes .exe on Windows\n"
#endif
        "  pgy <source.pgy> --run        compile + run\n"
        "  pgy <source.pgy> --opt=dev|release   (default: release)\n"
        "  pgy <source.pgy> --runtime=default|none  runtime contract mode (none is beta-gated)\n"
        "  pgy <source.pgy> --error-format=json|text  (default: text; json for structured tooling)\n"
        "  pgy init [name]              create pgy.toml, pgy.lock, and main.pgy\n"
        "  pgy check|build|run|test     run package commands from pgy.toml\n"
        "  pgy fmt [--check|--write]    format/check the package entry\n"
        "  pgy lint|prove|package       validate package evidence and write pgy.lock\n"
        "  pgy publish                  fail closed until registry publishing exists\n"
        "  pgy scaffold <kind> <target> create starter files\n"
        "  pgy new <project-dir>         scaffold a starter project\n"
        "\n"
        "Project design order:\n"
        "  intent -> world -> zone -> subject\n"
        "\n"
        "Host scaffold kinds:\n"
        "  subject  active host / who performs the contract\n"
        "  class    passive tool or thing with hosted func\n"
        "  object   passive view or state target\n"
        "  tobject  boundary packet\n"
        "  pgy --tokens <source.pgy>     dump token stream\n"
        "  pgy --ast    <source.pgy>     dump merged/normalized AST\n"
        "  pgy --dir    <source.pgy>     dump lowered DIR summary\n"
        "  pgy --native-pipeline --rir <source.pgy>  explicit native RIR summary\n"
        "  pgy --native-pipeline --rir-json <source.pgy>  explicit native RIR JSON\n"
        "  pgy --machine-manifest-json   dump native machine declaration JSON\n"
        "  pgy --native-pipeline --air <source.pgy>  explicit native AIR summary\n"
        "  pgy --native-pipeline --air-json <source.pgy>  explicit native AIR JSON\n"
        "  pgy --mir    <source.pgy>     dump lowered MIR summary\n"
        "  pgy --mir-json <source.pgy>   dump MIR fact JSON (pgy.mir.v1)\n"
        "  pgy --native-pipeline --hir <source.pgy>  explicit native HIR summary\n"
        "  pgy --native-pipeline --hir-cfg <source.pgy>  explicit native HIR CFG\n"
        "  pgy --native-pipeline --hir-dom <source.pgy>  explicit native HIR dominance\n"
        "  pgy --native-pipeline --hir-ssa <source.pgy>  explicit native HIR SSA-prep\n"
#ifdef PGY_LLVM_ENABLED
        "  default backend: LLVM\n"
        "  pgy <source.pgy> --backend=llvm   use LLVM native backend\n"
#else
        "  default backend: C\n"
#endif
#ifdef PGY_LLVM_ENABLED
        "  pgy <source.pgy> --emit-llvm      emit LLVM IR text\n"
#endif
        "  pgy <source.pgy> --native-pipeline  compile in-process instead of\n"
        "                                delegating to the self-host driver\n"
        "                                (bootstrap scaffolding and gates whose\n"
        "                                subject is the native pipeline; the\n"
        "                                same opt-out per harness is\n"
        "                                PGY_NATIVE_PIPELINE=1)\n"
        "  pgy --repl                    interactive REPL\n"
        "  pgy --help\n");
}
