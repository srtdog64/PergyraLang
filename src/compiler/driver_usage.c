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
        "  pgy <source.pgy> --emit-llvm -o <out.ll>\n"
#ifdef PGY_LLVM_ENABLED
        "  (LLVM + --run): if -o ends with .o/.obj, executable target becomes .exe on Windows\n"
#endif
        "  pgy <source.pgy> --run        compile + run\n"
        "  pgy <source.pgy> --opt=dev|release   (default: release)\n"
        "  pgy <source.pgy> --runtime=default|none  runtime contract mode (none is beta-gated)\n"
        "  pgy <source.pgy> --error-format=json|text  (default: text; json for structured tooling)\n"
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
        "  pgy --rir    <source.pgy>     dump lowered RIR summary\n"
        "  pgy --air    <source.pgy>     dump AIR verification summary\n"
        "  pgy --air-json <source.pgy>   dump stable AIR graph JSON after MIR evidence\n"
        "  pgy --mir    <source.pgy>     dump lowered MIR summary\n"
        "  pgy --mir-json <source.pgy>   dump lossless MIR JSON (pgy.mir.v1)\n"
        "  pgy --hir     <source.pgy>     dump lowered HIR summary\n"
        "  pgy --hir-cfg <source.pgy>     dump HIR CFG view\n"
        "  pgy --hir-dom <source.pgy>     dump HIR dominance view\n"
        "  pgy --hir-ssa <source.pgy>     dump HIR SSA-prep view\n"
#ifdef PGY_LLVM_ENABLED
        "  default backend: LLVM\n"
        "  pgy <source.pgy> --backend=llvm   use LLVM native backend\n"
#else
        "  default backend: C\n"
#endif
#ifdef PGY_LLVM_ENABLED
        "  pgy <source.pgy> --emit-llvm      emit LLVM IR text\n"
#endif
        "  pgy --repl                    interactive REPL\n"
        "  pgy --help\n");
}
