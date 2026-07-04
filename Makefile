# Pergyra Programming Language
#
# Copyright (c) 2025 Pergyra Language Project
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the Pergyra Language Project nor the names of
#    its contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

ifneq ($(or $(filter Windows_NT,$(OS)),$(MSYSTEM)),)
# Prefer the active MSYS2 bash when a MinGW/MSYS2 toolchain is driving the
# build. Git Bash can run POSIX shell scripts, but it is not the same runtime
# boundary as MSYS2 MinGW and can strip compiler-definition quotes before gcc
# sees them. Git Bash remains a fallback for local direct targets, but
# ci-windows fail-fasts unless it sees the MSYS2 runtime boundary.
PGY_WINDOWS_MSYS_BASH_CANDIDATES := \
    /usr/bin/bash.exe \
    /usr/bin/bash \
    C:/msys64/usr/bin/bash.exe \
    C:/msys64/usr/bin/bash
PGY_WINDOWS_GIT_BASH_CANDIDATES := \
    C:/Progra~1/Git/bin/bash.exe \
    C:/Progra~2/Git/bin/bash.exe \
    C:/Progra~1/Git/usr/bin/bash.exe \
    C:/Progra~2/Git/usr/bin/bash.exe
PGY_WINDOWS_BASH := $(firstword $(wildcard $(PGY_WINDOWS_MSYS_BASH_CANDIDATES) $(PGY_WINDOWS_GIT_BASH_CANDIDATES)))
ifneq ($(PGY_WINDOWS_BASH),)
BASH := $(PGY_WINDOWS_BASH)
SHELL := $(BASH)
endif
PGY_WINDOWS_BASH_IS_MSYS := $(if $(filter $(PGY_WINDOWS_MSYS_BASH_CANDIDATES),$(PGY_WINDOWS_BASH)),1,0)
endif

CC      = gcc
CC_DUMP_MACHINE := $(shell $(CC) -dumpmachine 2>/dev/null || echo unknown)
CC_MACHINE := $(CC_DUMP_MACHINE)
ifneq ($(findstring darwin,$(CC_DUMP_MACHINE)),)
OPENMP_FLAGS =
THREAD_LINK_LIB =
PLATFORM_CFLAGS = -D_DARWIN_C_SOURCE -D_XOPEN_SOURCE=700
PLATFORM_CFLAGS += -Wno-deprecated-declarations
else ifeq ($(or $(findstring mingw,$(CC_DUMP_MACHINE)),$(MSYSTEM)),)
OPENMP_FLAGS = -fopenmp
THREAD_LINK_LIB = -lpthread
PLATFORM_CFLAGS = -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE
else
OPENMP_FLAGS =
THREAD_LINK_LIB = -lwinpthread
# MinGW gcc links the MSVCRT printf by default, which rejects the C99
# %zu / %ju / %jd / %td specifiers. Defining __USE_MINGW_ANSI_STDIO=1
# routes printf/fprintf/sprintf/snprintf through MinGW's C99-conformant
# __mingw_vfprintf wrapper so diagnostic messages (e.g. "expects %zu
# argument(s), got %zu") print correctly and -Wformat validation passes.
# This is MinGW's own documented opt-in - no effect on Linux/macOS.
PLATFORM_CFLAGS = -D__USE_MINGW_ANSI_STDIO=1
endif
# Auto-enable PGY_ZONE_THREADSAFE on hosted systems. The default Pergyra
# build uses a threaded runtime (parallel/spawn/channel), so the rwlock
# macros need to be real, not no-ops.
#
# - Linux / MinGW: detected via the non-empty THREAD_LINK_LIB.
# - macOS:         pthread is in libSystem, so THREAD_LINK_LIB is empty
#                  yet threading is still active; gate on darwin too.
#
# The atomic generation counter (in pgy_runtime_zone_result_option_inline.h)
# is the minimum safety net; the rwlock becomes the structural lock for
# the rest of the zone state.
#
# Embedded / explicit single-threaded targets can override with
#   make PGY_ZONE_THREADSAFE=0
# which suppresses the define. Otherwise we always set it.
PGY_ZONE_THREADSAFE ?= 1
ifeq ($(PGY_ZONE_THREADSAFE),1)
ifneq ($(strip $(THREAD_LINK_LIB)),)
PLATFORM_CFLAGS += -DPGY_ZONE_THREADSAFE
else ifneq ($(findstring darwin,$(CC_DUMP_MACHINE)),)
PLATFORM_CFLAGS += -DPGY_ZONE_THREADSAFE
endif
endif
TMPDIR ?= /tmp
export TMPDIR
TMPDIR_CI := $(subst \,/,$(TMPDIR))
CFLAGS  = -Wall -Wextra -Werror=implicit-function-declaration -Werror=implicit-int -std=c11 -O2 -g $(OPENMP_FLAGS) $(PLATFORM_CFLAGS) -I$(SRC_DIR)
DEPFLAGS = -MMD -MP -MT $@
ASMFLAGS = -f elf64
NASM    := $(shell command -v nasm 2>/dev/null)
ENABLE_ASM_FASTPATH ?= 0
CI_LINUX_CC := $(or $(shell command -v cc 2>/dev/null),$(shell command -v gcc 2>/dev/null),gcc)
CI_WINDOWS_CROSS_CC := $(shell command -v x86_64-w64-mingw32-gcc 2>/dev/null)
CI_WINDOWS_CC := $(if $(MSYSTEM),$(CC),$(or $(CI_WINDOWS_CROSS_CC),$(CC)))
CI_WINDOWS_CC_MACHINE := $(shell $(CI_WINDOWS_CC) -dumpmachine 2>/dev/null || echo unknown)
CI_WINDOWS_RUNNABLE := $(if $(and $(MSYSTEM),$(filter 1,$(PGY_WINDOWS_BASH_IS_MSYS))),1,0)
CI_LINUX_BUILD_DIR := $(TMPDIR_CI)/pgy-ci-linux-build
CI_LINUX_BIN_DIR   := $(TMPDIR_CI)/pgy-ci-linux-bin
CI_WINDOWS_BUILD_DIR := $(TMPDIR_CI)/pgy-ci-windows-build
CI_WINDOWS_BIN_DIR   := $(TMPDIR_CI)/pgy-ci-windows-bin
# CI #391 follow-up: under MSYS2/UCRT64 with gcc 16.1.0, putting an MSYS path
# (e.g. /tmp/...) into the link response file makes the native ld.exe fail to
# locate the .o files compiled to the same MSYS path -- gcc converts the path
# for its own write but ld does not auto-convert when reading the .rsp. Force
# CI_WINDOWS_BUILD_DIR / CI_WINDOWS_BIN_DIR to the cygpath -m mixed form
# (Windows native drive letter with forward slashes) so the .rsp content is
# already native and ld picks the files up regardless of MSYS path handling.
ifneq ($(MSYSTEM),)
CI_WINDOWS_BUILD_DIR := $(shell cygpath -m "$(CI_WINDOWS_BUILD_DIR)" 2>/dev/null)
CI_WINDOWS_BIN_DIR   := $(shell cygpath -m "$(CI_WINDOWS_BIN_DIR)" 2>/dev/null)
endif
CI_MACOS_CC := $(or $(shell command -v cc 2>/dev/null),$(CC))
CI_MACOS_BUILD_DIR := $(TMPDIR_CI)/pgy-ci-macos-build
CI_MACOS_BIN_DIR   := $(TMPDIR_CI)/pgy-ci-macos-bin
PGY_BACKEND_COMPARE_SHARD_TOTAL ?= 0
PGY_BACKEND_COMPARE_SHARD_INDEX ?= 0
PGY_BACKEND_COMPARE_PRECHECK ?= 1
PGY_BACKEND_COMPARE_CASES ?=
PGY_BACKEND_COMPARE_START_INDEX ?= 0
PGY_BACKEND_COMPARE_MAX_CASES ?= 0
CI_BACKEND_COMPARE_SHARD_TOTAL ?= 20
CI_BACKEND_COMPARE_SHARD_INDEX ?= 0
ifeq ($(strip $(BASH)),)
BASH := $(shell command -v bash 2>/dev/null)
endif
ifeq ($(strip $(BASH)),)
BASH := bash
endif
ifeq ($(strip $(SHELL)),)
SHELL := $(BASH)
endif
pgy_mkdir_p = $(BASH) -c "mkdir -p $(1)"
pgy_touch_ref = $(BASH) -c "touch -c -r $(1) $(2)"
LLVM_CONFIG := $(shell command -v llvm-config 2>/dev/null || command -v llvm-config-20 2>/dev/null || command -v llvm-config-19 2>/dev/null || command -v llvm-config-18 2>/dev/null || command -v llvm-config-17 2>/dev/null || command -v llvm-config-16 2>/dev/null || command -v llvm-config-15 2>/dev/null)
WINDOWS_LLVM_READY := $(shell if [ -n "$(LLVM_CONFIG)" ] && "$(LLVM_CONFIG)" --libs core >/dev/null 2>&1; then echo 1; else echo 0; fi)
LLD := $(shell command -v ld.lld 2>/dev/null || command -v lld 2>/dev/null)
PROJECT_ROOT := $(CURDIR)
CFLAGS  += -DPGY_PROJECT_ROOT=\"$(PROJECT_ROOT)\"
CFLAGS  += -DPGY_SRC_DIR=\"$(PROJECT_ROOT)/src\"
CFLAGS  += -DPGY_RUNTIME_DIR=\"$(PROJECT_ROOT)/src/runtime\"
CFLAGS  += -DPGY_RUNTIME_LIB_C=\"$(PROJECT_ROOT)/src/runtime/pgy_runtime_lib.c\"
# Permanent wiring for the LLVM runtime-bitcode inliner: point the backend at the
# committed runtime bitcode so `--backend=llvm` folds runtime primitives
# (Substring, ...) by default, closing the ~1.7x gap vs the C backend (measured
# 1.67x -> 0.86x on the self-hosted lexer). A missing/stale .bc is a silent
# no-op (back to external calls); PGY_RUNTIME_BC (env) overrides this path for a
# relocated binary. Regenerate with scripts/build_runtime_bc.sh.
CFLAGS  += -DPGY_RUNTIME_LIB_BC=\"$(PROJECT_ROOT)/src/runtime/pgy_runtime_lib.bc\"
LLVM_MONOLITHIC_SONAME := $(firstword $(wildcard /usr/lib/llvm-*/lib/libLLVM.so.1 /usr/lib/llvm-*/lib/libLLVM.so.* /usr/lib/x86_64-linux-gnu/libLLVM.so.* /usr/lib/x86_64-linux-gnu/libLLVM-*.so /lib/x86_64-linux-gnu/libLLVM.so.* /lib/x86_64-linux-gnu/libLLVM-*.so))
LLVM_MONOLITHIC_DIR := $(dir $(LLVM_MONOLITHIC_SONAME))
LLVM_MONOLITHIC_NAME := $(notdir $(LLVM_MONOLITHIC_SONAME))
LLVM_INCLUDEDIR_FALLBACK := $(or $(shell for d in /usr/lib/llvm-*/include /usr/include third_party; do if [ -f "$$d/llvm-c/Core.h" ]; then echo "$$d"; break; fi; done),third_party)

ifneq ($(or $(findstring mingw,$(CC_MACHINE)),$(MSYSTEM)),)
EXEEXT := .exe
else
EXEEXT :=
endif

ifeq ($(EXEEXT),.exe)
PGY_WINDOWS_NATIVE_RUNTIME_PATH := /c/LLVM/bin:/c/Program Files/LLVM/bin:/c/ProgramData/mingw64/mingw64/bin:/c/msys64/ucrt64/bin:/c/msys64/clang64/bin:/c/msys64/mingw64/bin
ifneq ($(and $(MSYSTEM_PREFIX),$(filter 1,$(PGY_WINDOWS_BASH_IS_MSYS))),)
PGY_WINDOWS_NATIVE_RUNTIME_PATH := $(MSYSTEM_PREFIX)/bin:$(PGY_WINDOWS_NATIVE_RUNTIME_PATH)
endif
define pgy_run_native
PATH="$(PGY_WINDOWS_NATIVE_RUNTIME_PATH):$$PATH" $(1)
endef
else
define pgy_run_native
$(1)
endef
endif

# -----------------------------------------------------------------
# LLVM backend (enabled by default)
#   make                         - build with LLVM native backend (default)
#   make LLVM_ENABLED=0          - build without LLVM (C transpiler only)
# -----------------------------------------------------------------
LLVM_DIR     = third_party
LLVM_INSTALL = C:/Program Files/LLVM
LLVM_ENABLED ?= 1
STDLIB_BACKENDS ?= $(if $(filter 0,$(LLVM_ENABLED)),c,c llvm)
STAGE4_DETERMINISM_BACKENDS ?= $(if $(filter 0,$(LLVM_ENABLED)),c,c llvm)
FILESYSTEM_WALK_BACKENDS ?= $(if $(filter 0,$(LLVM_ENABLED)),c,c llvm)
SELFHOST_PARSER_BACKENDS ?= $(if $(filter 0,$(LLVM_ENABLED)),c,c llvm)
SELFHOST_SEMANTIC_BACKENDS ?= $(if $(filter 0,$(LLVM_ENABLED)),c,c llvm)
SELFHOST_CODEGEN_BACKENDS ?= $(if $(filter 0,$(LLVM_ENABLED)),c,c llvm)
RUNTIME_PANIC_CODEGEN_BACKENDS ?= $(if $(filter 0,$(LLVM_ENABLED)),c,c llvm)
SLOT_CONTRACT_BACKENDS ?= $(if $(filter 0,$(LLVM_ENABLED)),c,c llvm)
AIR_NONIMPACT_BACKENDS ?= $(if $(filter 0,$(LLVM_ENABLED)),c,c llvm)
AIR_NONIMPACT_CASE_LIMIT ?=
AIR_NONIMPACT_SHARD_COUNT ?=
AIR_NONIMPACT_SHARD_INDEX ?=
MEMORY_CONCURRENCY_BACKENDS ?= $(if $(filter 0,$(LLVM_ENABLED)),c,c llvm)
CC_TAG       := $(shell printf '%s' "$(CC_MACHINE)" | tr -c 'A-Za-z0-9_.-' '_')
CONFIG_STAMP = $(BUILD_DIR)/.config_llvm_$(LLVM_ENABLED)_$(CC_TAG).stamp

ifeq ($(origin BUILD_DIR), undefined)
  ifneq ($(filter /mnt/%,$(PROJECT_ROOT)),)
    ifeq ($(findstring mingw,$(CC_MACHINE)),)
      BUILD_DIR := $(TMPDIR)/pgy-$(notdir $(PROJECT_ROOT))-build
      BIN_DIR   := $(TMPDIR)/pgy-$(notdir $(PROJECT_ROOT))-bin
    endif
  endif
endif

ifneq ($(LLVM_ENABLED),0)
  ifneq ($(LLVM_CONFIG),)
    LLVM_INCLUDEDIR := $(shell $(LLVM_CONFIG) --includedir 2>/dev/null)
    LLVM_LIBDIR     := $(shell $(LLVM_CONFIG) --libdir 2>/dev/null)
    LLVM_LIBS       := $(shell $(LLVM_CONFIG) --libs --system-libs 2>/dev/null)
    CFLAGS  += -DPGY_LLVM_ENABLED -I$(LLVM_INCLUDEDIR)
    LDFLAGS_LLVM = -L$(LLVM_LIBDIR) $(LLVM_LIBS)
  else ifneq ($(LLVM_MONOLITHIC_SONAME),)
    CFLAGS  += -DPGY_LLVM_ENABLED -I$(LLVM_INCLUDEDIR_FALLBACK)
    LDFLAGS_LLVM = -L$(LLVM_MONOLITHIC_DIR) -l:$(LLVM_MONOLITHIC_NAME)
  else ifneq ($(findstring darwin,$(CC_MACHINE)),)
    # macOS: never fall through to the $(LLVM_INSTALL) Windows path.
    # If neither LLVM_CONFIG nor LLVM_MONOLITHIC_SONAME points at a
    # real macOS LLVM, disable the LLVM backend rather than poisoning
    # LDFLAGS with a path that does not exist on Darwin. ci-macos
    # already passes LLVM_ENABLED=0 to its sub-makes; sub-makes that
    # omit LLVM_ENABLED would otherwise inherit the Windows fallback
    # and the link command would fail looking for LLVM-C.
  else
    CFLAGS  += -DPGY_LLVM_ENABLED -I$(LLVM_DIR)
    LDFLAGS_LLVM = -L"$(LLVM_INSTALL)/lib" -lLLVM-C
  endif
endif

# -----------------------------------------------------------------
# Directories
# -----------------------------------------------------------------
SRC_DIR      = src
BUILD_DIR   ?= build
BIN_DIR     ?= bin
REPO_BIN_DIR := $(PROJECT_ROOT)/bin
LEXER_DIR    = $(SRC_DIR)/lexer
PARSER_DIR   = $(SRC_DIR)/parser
RUNTIME_DIR  = $(SRC_DIR)/runtime
ASYNC_DIR    = $(RUNTIME_DIR)/async
SEMANTIC_DIR = $(SRC_DIR)/semantic
CODEGEN_DIR  = $(SRC_DIR)/codegen
COMPILER_DIR = $(SRC_DIR)/compiler
COMMON_DIR   = $(SRC_DIR)/common

# -----------------------------------------------------------------
# Source groups
# -----------------------------------------------------------------
COMMON_SOURCES   = $(COMMON_DIR)/arena.c \
                   $(COMMON_DIR)/diagnostic_layer.c \
                   $(COMMON_DIR)/env_flags.c \
                   $(COMMON_DIR)/intent_observability_names.c \
                   $(COMMON_DIR)/match_variant_policy.c \
                   $(COMMON_DIR)/numeric_parse.c \
                   $(COMMON_DIR)/pgy_builtin_type_table.c \
                   $(COMMON_DIR)/squiggle_class.c \
                   $(COMMON_DIR)/worker_boundary_storage_policy.c
LEXER_SOURCES    = $(LEXER_DIR)/lexer.c \
                   $(LEXER_DIR)/lexer_keywords.c \
                   $(LEXER_DIR)/lexer_token_debug.c
PARSER_SOURCES   = $(PARSER_DIR)/ast.c \
                   $(PARSER_DIR)/ast_analysis.c \
                   $(PARSER_DIR)/ast_identifier_ref_analysis.c \
                   $(PARSER_DIR)/ast_identity.c \
                   $(PARSER_DIR)/ast_thread_pool_analysis.c \
                   $(PARSER_DIR)/ast_async_constructors.c \
                   $(PARSER_DIR)/ast_destroy.c \
                   $(PARSER_DIR)/ast_destroy_domain.c \
                   $(PARSER_DIR)/ast_clone.c \
                   $(PARSER_DIR)/ast_constructors.c \
                   $(PARSER_DIR)/ast_decl_accessors.c \
                   $(PARSER_DIR)/ast_func_accessors.c \
                   $(PARSER_DIR)/ast_domain_accessors.c \
                   $(PARSER_DIR)/ast_block_match_event_accessors.c \
                   $(PARSER_DIR)/ast_role_type_accessors.c \
                   $(PARSER_DIR)/ast_expr_control_accessors.c \
                   $(PARSER_DIR)/ast_async_lambda_accessors.c \
                   $(PARSER_DIR)/ast_domain_accessors_world.c \
                   $(PARSER_DIR)/ast_domain_constructors.c \
                   $(PARSER_DIR)/ast_world_constructors.c \
                   $(PARSER_DIR)/ast_intent_constructors.c \
                   $(PARSER_DIR)/ast_zone_constructors.c \
                   $(PARSER_DIR)/ast_domain_tail_constructors.c \
                   $(PARSER_DIR)/ast_intent_step_accessors.c \
                   $(PARSER_DIR)/ast_intent_step_mutators.c \
                   $(PARSER_DIR)/ast_zone_accessors.c \
                   $(PARSER_DIR)/ast_print.c \
                   $(PARSER_DIR)/ast_print_domain.c \
                   $(PARSER_DIR)/ast_print_event.c \
                   $(PARSER_DIR)/ast_print_expr.c \
                   $(PARSER_DIR)/ast_print_generics.c \
                   $(PARSER_DIR)/ast_print_inline.c \
                   $(PARSER_DIR)/ast_print_intent.c \
                   $(PARSER_DIR)/ast_print_misc.c \
                   $(PARSER_DIR)/ast_print_world.c \
                   $(PARSER_DIR)/ast_print_zone.c \
                   $(PARSER_DIR)/parser.c \
                   $(PARSER_DIR)/parser_decl_hints.c \
                   $(PARSER_DIR)/parser_doc.c \
                   $(PARSER_DIR)/parser_enum.c \
                   $(PARSER_DIR)/parser_export.c \
                   $(PARSER_DIR)/parser_expr.c \
                   $(PARSER_DIR)/parser_expr_call_args.c \
                   $(PARSER_DIR)/parser_expr_lambda.c \
                   $(PARSER_DIR)/parser_expr_map_literal.c \
                   $(PARSER_DIR)/parser_expr_postfix.c \
                   $(PARSER_DIR)/parser_expr_string.c \
                   $(PARSER_DIR)/parser_expr_util.c \
                   $(PARSER_DIR)/parser_pin.c \
                   $(PARSER_DIR)/parser_stmt.c \
                   $(PARSER_DIR)/parser_statement_dispatch.c \
                   $(PARSER_DIR)/parser_name_tokens.c \
                   $(PARSER_DIR)/parser_type.c \
                   $(PARSER_DIR)/parser_zone_context.c \
                   $(PARSER_DIR)/parser_decl.c \
                   $(PARSER_DIR)/parser_decl_clause.c \
                   $(PARSER_DIR)/parser_decl_function_clause.c \
                   $(PARSER_DIR)/parser_decl_start.c \
                   $(PARSER_DIR)/parser_intent.c \
                   $(PARSER_DIR)/parser_intent_bindings.c \
                   $(PARSER_DIR)/parser_intent_defaults.c \
                   $(PARSER_DIR)/parser_intent_step.c \
                   $(PARSER_DIR)/parser_domain.c \
                   $(PARSER_DIR)/parser_domain_event.c \
                   $(PARSER_DIR)/parser_domain_projection.c \
                   $(PARSER_DIR)/parser_domain_relation_effect.c \
                   $(PARSER_DIR)/parser_domain_roster.c \
                   $(PARSER_DIR)/parser_domain_world.c \
                   $(PARSER_DIR)/parser_domain_zone.c \
                   $(PARSER_DIR)/parser_async.c
RUNTIME_SOURCES  = $(RUNTIME_DIR)/slot_manager.c \
                   $(RUNTIME_DIR)/slot_manager_core_ops.c \
                   $(RUNTIME_DIR)/slot_manager_storage.c \
                   $(RUNTIME_DIR)/slot_manager_pin.c \
                   $(RUNTIME_DIR)/slot_manager_query_lock.c \
                   $(RUNTIME_DIR)/slot_manager_secure_ops.c \
                   $(RUNTIME_DIR)/slot_type_utils.c \
                   $(RUNTIME_DIR)/slot_pool.c \
                   $(RUNTIME_DIR)/slot_pool_linked_list.c \
                   $(RUNTIME_DIR)/slot_pool_perf.c \
                   $(RUNTIME_DIR)/slot_security.c \
                   $(RUNTIME_DIR)/slot_security_fingerprint.c \
                   $(RUNTIME_DIR)/slot_security_crypto.c \
                   $(RUNTIME_DIR)/slot_security_memory.c \
                   $(RUNTIME_DIR)/slot_security_platform.c \
                   $(RUNTIME_DIR)/slot_security_sealed_payload.c \
                   $(RUNTIME_DIR)/slot_manager_security_stats.c \
                   $(RUNTIME_DIR)/slot_manager_scope.c \
                   $(RUNTIME_DIR)/party_runtime.c \
                   $(RUNTIME_DIR)/party_runtime_scheduler.c \
                   $(RUNTIME_DIR)/party_runtime_stats.c \
                   $(RUNTIME_DIR)/party_runtime_dispatch.c \
                   $(RUNTIME_DIR)/world_roster.c \
                   $(RUNTIME_DIR)/world_roster_async.c \
                   $(RUNTIME_DIR)/world_roster_lookup.c \
                   $(RUNTIME_DIR)/world_roster_plan_stats.c
ASYNC_SOURCES    = $(ASYNC_DIR)/concurrent_queue.c \
                   $(ASYNC_DIR)/async_scope.c \
                   $(ASYNC_DIR)/async_scope_patterns.c \
                   $(ASYNC_DIR)/fiber.c \
                   $(ASYNC_DIR)/scheduler_fiber_ops.c \
                   $(ASYNC_DIR)/scheduler.c
RUNTIME_SOURCES  += $(ASYNC_SOURCES)
RUNTIME_ASM_SOURCES = $(RUNTIME_DIR)/slot_asm.s
SEMANTIC_SOURCES = $(SEMANTIC_DIR)/type_system.c \
                   $(SEMANTIC_DIR)/boundary_witness.c \
                   $(SEMANTIC_DIR)/type_system_slot.c \
                   $(SEMANTIC_DIR)/type_system_tuple.c \
                   $(SEMANTIC_DIR)/type_system_compat.c \
                   $(SEMANTIC_DIR)/type_effects.c \
                   $(SEMANTIC_DIR)/type_infer.c \
                   $(SEMANTIC_DIR)/type_env.c \
                   $(SEMANTIC_DIR)/symbol_table.c \
                   $(SEMANTIC_DIR)/type_checker.c \
                   $(SEMANTIC_DIR)/type_checker_context_helpers.c \
                   $(SEMANTIC_DIR)/type_checker_namespace_decl.c \
                   $(SEMANTIC_DIR)/type_checker_unsafe_block.c \
                   $(SEMANTIC_DIR)/type_checker_assignment.c \
                   $(SEMANTIC_DIR)/type_checker_assignment_path.c \
                   $(SEMANTIC_DIR)/type_checker_diag.c \
                   $(SEMANTIC_DIR)/type_checker_generic_diag.c \
                   $(SEMANTIC_DIR)/type_checker_generic_support.c \
                   $(SEMANTIC_DIR)/type_checker_generic_contracts.c \
                   $(SEMANTIC_DIR)/type_checker_generic_effective_args.c \
                   $(SEMANTIC_DIR)/type_checker_generic_validation.c \
                   $(SEMANTIC_DIR)/type_checker_type_constraint.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_helpers.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_core.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_validate.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_collect.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_labels.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_domain.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_world.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_zone.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_zone_inventory.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_zone_commands.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_zone_tail.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_intent.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_inventory.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_body.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_decl.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_decl_participants.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_metadata_dead_end.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_metadata_constructed.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_metadata_alias.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_metadata_diagnostics.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_metadata_storage.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_metadata_index.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_metadata.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_stage_domain.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_stage_domain_label.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_stage_lookup.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_stage_stats.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_stage_signature.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_stage_alias.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_stage_nominal.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_stage_systemic.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_stage_domain_decl.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_worklist.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_stage.c \
                   $(SEMANTIC_DIR)/type_checker_type_alias.c \
                   $(SEMANTIC_DIR)/type_checker_class_decl.c \
                   $(SEMANTIC_DIR)/type_checker_enum_decl.c \
                   $(SEMANTIC_DIR)/type_checker_host_overlay.c \
                   $(SEMANTIC_DIR)/type_checker_host_helpers.c \
                   $(SEMANTIC_DIR)/type_checker_host_index.c \
                   $(SEMANTIC_DIR)/type_checker_host_lookup.c \
                   $(SEMANTIC_DIR)/type_checker_host_resource.c \
                   $(SEMANTIC_DIR)/type_checker_program.c \
                   $(SEMANTIC_DIR)/type_checker_program_stats.c \
                   $(SEMANTIC_DIR)/type_checker_func_decl.c \
                   $(SEMANTIC_DIR)/type_checker_func_types.c \
                   $(SEMANTIC_DIR)/type_checker_func_param_contract.c \
                   $(SEMANTIC_DIR)/type_checker_func_action_contract.c \
                   $(SEMANTIC_DIR)/type_checker_relation_decl.c \
                   $(SEMANTIC_DIR)/type_checker_effect_decl.c \
                   $(SEMANTIC_DIR)/type_checker_zone_decl.c \
                   $(SEMANTIC_DIR)/type_checker_zone_decl_authority.c \
                   $(SEMANTIC_DIR)/type_checker_zone_lifecycle.c \
                   $(SEMANTIC_DIR)/type_checker_zone_maintenance.c \
                   $(SEMANTIC_DIR)/type_checker_zone_shape.c \
                   $(SEMANTIC_DIR)/type_checker_zone_projection_rules.c \
                   $(SEMANTIC_DIR)/type_checker_zone_state.c \
                   $(SEMANTIC_DIR)/type_checker_ability_decl.c \
                   $(SEMANTIC_DIR)/type_checker_world_decl.c \
                   $(SEMANTIC_DIR)/type_checker_world_helpers.c \
                   $(SEMANTIC_DIR)/type_checker_world_state.c \
                   $(SEMANTIC_DIR)/type_checker_domain_slots.c \
                   $(SEMANTIC_DIR)/type_checker_intent_decl.c \
                   $(SEMANTIC_DIR)/type_checker_intent_action_contract.c \
                   $(SEMANTIC_DIR)/type_checker_intent_ability.c \
                   $(SEMANTIC_DIR)/type_checker_intent_on_inference.c \
                   $(SEMANTIC_DIR)/type_checker_intent_binding_context.c \
                   $(SEMANTIC_DIR)/type_checker_intent_bindings.c \
                   $(SEMANTIC_DIR)/type_checker_intent_types.c \
                   $(SEMANTIC_DIR)/type_checker_intent_contract_summary.c \
                   $(SEMANTIC_DIR)/type_checker_intent_helpers.c \
                   $(SEMANTIC_DIR)/type_checker_intent_authority.c \
                   $(SEMANTIC_DIR)/type_checker_intent_participants.c \
                   $(SEMANTIC_DIR)/type_checker_intent_transfer.c \
                   $(SEMANTIC_DIR)/type_checker_intent_role_fields.c \
                   $(SEMANTIC_DIR)/type_checker_intent_control.c \
                   $(SEMANTIC_DIR)/type_checker_role_decl.c \
                   $(SEMANTIC_DIR)/type_checker_party_decl.c \
                   $(SEMANTIC_DIR)/type_checker_roster_decl.c \
                   $(SEMANTIC_DIR)/type_checker_async_decl.c \
                   $(SEMANTIC_DIR)/type_checker_async_channel.c \
                   $(SEMANTIC_DIR)/type_checker_event.c \
                   $(SEMANTIC_DIR)/type_checker_bind_stmt.c \
                   $(SEMANTIC_DIR)/type_checker_qubit.c \
                   $(SEMANTIC_DIR)/type_checker_ability_ref.c \
                   $(SEMANTIC_DIR)/type_checker_stdlib_use.c \
                   $(SEMANTIC_DIR)/type_checker_module_contract.c \
                   $(SEMANTIC_DIR)/type_checker_loop_control.c \
                   $(SEMANTIC_DIR)/type_checker_module_contract_diag.c \
                   $(SEMANTIC_DIR)/type_checker_ability_fields.c \
                   $(SEMANTIC_DIR)/type_checker_ability_match.c \
                   $(SEMANTIC_DIR)/type_checker_ability_where.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_classify.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_diag_context.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_diag.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_diag_helper_call.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_diag_constructor.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_assign.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_array_store.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_boundaries.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_call.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_destructure.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_let.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_let_helpers.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_let_view.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_let_slot_claim.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_param_summary.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_return.c \
                    $(SEMANTIC_DIR)/type_checker_channel_transport.c \
                    $(SEMANTIC_DIR)/type_checker_visibility.c \
                    $(SEMANTIC_DIR)/type_checker_builtins_projection.c \
                    $(SEMANTIC_DIR)/type_checker_resolution_retired.c \
                    $(SEMANTIC_DIR)/type_checker_type_helpers.c \
                   $(SEMANTIC_DIR)/type_checker_expr_host.c \
                   $(SEMANTIC_DIR)/type_checker_expr_call.c \
                   $(SEMANTIC_DIR)/type_checker_expr_enum.c \
                   $(SEMANTIC_DIR)/type_checker_expr_lambda.c \
                   $(SEMANTIC_DIR)/type_checker_expr.c \
                   $(SEMANTIC_DIR)/type_checker_lambda_capture.c \
                   $(SEMANTIC_DIR)/type_checker_expr_names.c \
                   $(SEMANTIC_DIR)/type_checker_expr_ops.c \
                   $(SEMANTIC_DIR)/type_checker_reflect.c \
                   $(SEMANTIC_DIR)/type_checker_builtins.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_intent_observability.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_nominal.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_ownership_nominal.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_query.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_query_channel.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_channel_state.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_state_tools.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_query_domain.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_query_world.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_resolve.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_secure_token.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_device_slot.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_slotops.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_slotops_view.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_cancel.c \
                   $(SEMANTIC_DIR)/type_checker_collection_policy.c \
                   $(SEMANTIC_DIR)/type_checker_collection_mutation_contract.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_stdlib_scalar.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_stdlib_map.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_stdlib_collections.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_stdlib_channel_transport.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_stdlib_variant.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_stdlib_body.c \
                   $(SEMANTIC_DIR)/type_checker_domain_role_lookup.c \
                   $(SEMANTIC_DIR)/type_checker_domain_projection.c \
                   $(SEMANTIC_DIR)/type_checker_domain_projection_fields.c \
                   $(SEMANTIC_DIR)/type_checker_overlay_common.c \
                   $(SEMANTIC_DIR)/type_checker_projection_path.c \
                   $(SEMANTIC_DIR)/type_checker_world_embedding.c \
                   $(SEMANTIC_DIR)/type_checker_decls_domain_helpers.c \
                   $(SEMANTIC_DIR)/type_checker_domain_contracts.c \
                   $(SEMANTIC_DIR)/type_checker_call_constructor.c \
                   $(SEMANTIC_DIR)/type_checker_call_contract_helpers.c \
                   $(SEMANTIC_DIR)/type_checker_call_generic_where.c \
                   $(SEMANTIC_DIR)/type_checker_helpers_effects.c \
                   $(SEMANTIC_DIR)/type_checker_helpers_resources.c \
                   $(SEMANTIC_DIR)/type_checker_helpers_late.c \
                   $(SEMANTIC_DIR)/type_checker_slot_view_active.c \
                   $(SEMANTIC_DIR)/type_checker_slot_view_boundary.c \
                   $(SEMANTIC_DIR)/type_checker_flow_effects.c \
                   $(SEMANTIC_DIR)/type_checker_flow_resources.c \
                   $(SEMANTIC_DIR)/type_checker_flow_loop_control.c \
                   $(SEMANTIC_DIR)/type_checker_flow_loop_snapshot.c \
                   $(SEMANTIC_DIR)/type_checker_flow_loops.c \
                   $(SEMANTIC_DIR)/type_checker_flow_branch.c \
                   $(SEMANTIC_DIR)/type_checker_flow_parallel.c \
                   $(SEMANTIC_DIR)/type_checker_flow_statement_kinds.c \
                   $(SEMANTIC_DIR)/type_checker_flow_match_coverage.c \
                   $(SEMANTIC_DIR)/type_checker_flow_match.c \
                   $(SEMANTIC_DIR)/type_checker_flow.c \
                   $(SEMANTIC_DIR)/slot_analyzer.c \
                   $(SEMANTIC_DIR)/slot_analyzer_builtin.c \
                   $(SEMANTIC_DIR)/slot_analyzer_lookup.c \
                   $(SEMANTIC_DIR)/slot_analyzer_access.c \
                   $(SEMANTIC_DIR)/slot_analyzer_escape.c \
                   $(SEMANTIC_DIR)/slot_analyzer_summary.c \
                   $(SEMANTIC_DIR)/lifecycle_state.c \
                   $(SEMANTIC_DIR)/lifecycle_analyze.c \
                   $(SEMANTIC_DIR)/capability_analyze.c \
                   $(SEMANTIC_DIR)/semantic.c
CODEGEN_SOURCES  = $(CODEGEN_DIR)/transpiler_allocator_builtin_emit.c \
                   $(CODEGEN_DIR)/transpiler_async_parallel_emit.c \
                   $(CODEGEN_DIR)/transpiler_call_constructor_result_emit.c \
                   $(CODEGEN_DIR)/transpiler_call_subject_arg_policy.c \
                   $(CODEGEN_DIR)/transpiler_call_result_option_builtin_emit.c \
                   $(CODEGEN_DIR)/transpiler_class_constructor_emit.c \
                   $(CODEGEN_DIR)/transpiler_constructor_channel_guard.c \
                   $(CODEGEN_DIR)/transpiler_context.c \
                   $(CODEGEN_DIR)/codegen_channel_runtime_abi.c \
                   $(CODEGEN_DIR)/codegen_hashmap_key_policy.c \
                   $(CODEGEN_DIR)/codegen_match_subject_lookup.c \
                   $(CODEGEN_DIR)/codegen_match_variant_policy.c \
                   $(CODEGEN_DIR)/codegen_scalar_arithmetic_policy.c \
                   $(CODEGEN_DIR)/codegen_slot_type_policy.c \
                   $(CODEGEN_DIR)/domain_frontier_policy.c \
                   $(CODEGEN_DIR)/domain_frontier_graph.c \
                   $(CODEGEN_DIR)/host_decl_compat.c \
                   $(CODEGEN_DIR)/intent_binding_metadata_view.c \
                   $(CODEGEN_DIR)/intent_observability_usage.c \
                   $(CODEGEN_DIR)/transpiler_intent_observability_builtin_emit.c \
                   $(CODEGEN_DIR)/thread_pool_usage.c \
                   $(CODEGEN_DIR)/transpiler_channel_type_query.c \
                   $(CODEGEN_DIR)/transpiler_class_decl_emit.c \
                   $(CODEGEN_DIR)/transpiler_collection_runtime_suffix.c \
                   $(CODEGEN_DIR)/transpiler_block_intent_helpers.c \
                   $(CODEGEN_DIR)/transpiler_block_intent_rebind_helpers.c \
                   $(CODEGEN_DIR)/transpiler_block_emit.c \
                   $(CODEGEN_DIR)/transpiler_defer_emit.c \
                   $(CODEGEN_DIR)/transpiler_entry.c \
                   $(CODEGEN_DIR)/transpiler_symbols.c \
                   $(CODEGEN_DIR)/transpiler_decl_lookup.c \
                   $(CODEGEN_DIR)/transpiler_decl_field_view.c \
                   $(CODEGEN_DIR)/transpiler_decl_slot_view.c \
                   $(CODEGEN_DIR)/transpiler_decl_zone_refresh_view.c \
                   $(CODEGEN_DIR)/transpiler_decl_role_roster_slot_view.c \
                   $(CODEGEN_DIR)/transpiler_decl_method_view.c \
                   $(CODEGEN_DIR)/transpiler_decl_host_lookup.c \
                   $(CODEGEN_DIR)/transpiler_destructure_emit.c \
                   $(CODEGEN_DIR)/transpiler_domain_constructor_emit.c \
                   $(CODEGEN_DIR)/transpiler_domain_ability_emit.c \
                   $(CODEGEN_DIR)/transpiler_domain_nominal_emit.c \
                   $(CODEGEN_DIR)/transpiler_domain_provenance_emit.c \
                   $(CODEGEN_DIR)/transpiler_domain_role_include_emit.c \
                   $(CODEGEN_DIR)/transpiler_enum_constructor_emit.c \
                   $(CODEGEN_DIR)/transpiler_domain_role_ability_emit.c \
                   $(CODEGEN_DIR)/transpiler_domain_role_ability_mir_emit.c \
                   $(CODEGEN_DIR)/transpiler_domain_role_methods_emit.c \
                   $(CODEGEN_DIR)/transpiler_domain_role_ability_names.c \
                   $(CODEGEN_DIR)/transpiler_roster_decl_emit.c \
                   $(CODEGEN_DIR)/transpiler_enum.c \
                   $(CODEGEN_DIR)/transpiler_enum_decl_emit.c \
                   $(CODEGEN_DIR)/transpiler_enum_method_names.c \
                   $(CODEGEN_DIR)/transpiler_event_emit.c \
                   $(CODEGEN_DIR)/transpiler_event_builtin_emit.c \
                   $(CODEGEN_DIR)/transpiler_extern.c \
                   $(CODEGEN_DIR)/transpiler_expr_stdlib_builtin.c \
                   $(CODEGEN_DIR)/transpiler_expr_stdlib_builtin_policy.c \
                   $(CODEGEN_DIR)/transpiler_expr_type_infer_call_policy.c \
                   $(CODEGEN_DIR)/transpiler_expr_builtin_dispatch.c \
                   $(CODEGEN_DIR)/transpiler_expr_core_builtins_emit.c \
                   $(CODEGEN_DIR)/transpiler_expr_call_member_emit.c \
                   $(CODEGEN_DIR)/transpiler_expr_call_spawn_emit.c \
                   $(CODEGEN_DIR)/transpiler_expr_core_emit.c \
                   $(CODEGEN_DIR)/transpiler_expr_call_type_infer.c \
                   $(CODEGEN_DIR)/transpiler_expr_composite_literal_emit.c \
                   $(CODEGEN_DIR)/transpiler_expr_dispatch_emit.c \
                   $(CODEGEN_DIR)/transpiler_expr_dispatch_operand.c \
                   $(CODEGEN_DIR)/transpiler_host_field_identifier.c \
                   $(CODEGEN_DIR)/transpiler_expr_call_user_emit.c \
                   $(CODEGEN_DIR)/transpiler_expr_domain_query_builtin.c \
                   $(CODEGEN_DIR)/transpiler_expr_io_builtin.c \
                   $(CODEGEN_DIR)/transpiler_expr_array_access_emit.c \
                   $(CODEGEN_DIR)/transpiler_expr_literal_emit.c \
                   $(CODEGEN_DIR)/transpiler_expr_party_instance_emit.c \
                   $(CODEGEN_DIR)/transpiler_expr_projection_builtin.c \
                   $(CODEGEN_DIR)/transpiler_expr_stdlib_channel_builtin.c \
                   $(CODEGEN_DIR)/transpiler_expr_stdlib_collection_builtin.c \
                   $(CODEGEN_DIR)/transpiler_expr_stdlib_collection_support.c \
                   $(CODEGEN_DIR)/transpiler_expr_stdlib_map_builtin.c \
                   $(CODEGEN_DIR)/transpiler_expr_stdlib_queue_builtin.c \
                   $(CODEGEN_DIR)/transpiler_expr_type_infer.c \
                   $(CODEGEN_DIR)/transpiler_expr_stdlib_misc_builtin.c \
                   $(CODEGEN_DIR)/transpiler_expr_stdlib_scalar_builtin.c \
                   $(CODEGEN_DIR)/transpiler_expr_stdlib_scalar_unary.c \
                   $(CODEGEN_DIR)/transpiler_expr_unary_emit.c \
                   $(CODEGEN_DIR)/transpiler_format.c \
                   $(CODEGEN_DIR)/transpiler_func_class_flow_emit.c \
                   $(CODEGEN_DIR)/transpiler_func_flow_policy.c \
                   $(CODEGEN_DIR)/transpiler_future_type_query.c \
                   $(CODEGEN_DIR)/transpiler_control_flow_emit.c \
                   $(CODEGEN_DIR)/transpiler_statement_dispatch.c \
                   $(CODEGEN_DIR)/transpiler_match_bindings.c \
                   $(CODEGEN_DIR)/transpiler_match_emit.c \
                   $(CODEGEN_DIR)/transpiler_specialization_registry.c \
                   $(CODEGEN_DIR)/transpiler_specialization_scan.c \
                   $(CODEGEN_DIR)/transpiler_specialization_type_name_scan.c \
                   $(CODEGEN_DIR)/transpiler_tuple_specialization_registry.c \
                   $(CODEGEN_DIR)/transpiler_type_result_mapping_helpers.c \
                   $(CODEGEN_DIR)/transpiler_func_forward_policy.c \
                   $(CODEGEN_DIR)/transpiler_func_forward_emit.c \
                   $(CODEGEN_DIR)/transpiler_func_forward_metadata.c \
                   $(CODEGEN_DIR)/transpiler_generic_binding_query.c \
                   $(CODEGEN_DIR)/transpiler_hosted_method_body_emit.c \
                   $(CODEGEN_DIR)/transpiler_generic_class_naming.c \
                   $(CODEGEN_DIR)/transpiler_generic_class_specialization_emit.c \
                   $(CODEGEN_DIR)/transpiler_generic_param_query.c \
                   $(CODEGEN_DIR)/transpiler_generic_specialization_emit.c \
                   $(CODEGEN_DIR)/transpiler_intent_cleanup_emit.c \
                   $(CODEGEN_DIR)/transpiler_intent_emit.c \
                   $(CODEGEN_DIR)/transpiler_intent_failure_emit.c \
                   $(CODEGEN_DIR)/transpiler_intent_emit_metadata_helpers.c \
                   $(CODEGEN_DIR)/transpiler_intent_prologue_emit.c \
                   $(CODEGEN_DIR)/transpiler_intent_context.c \
                   $(CODEGEN_DIR)/transpiler_host_self_policy.c \
                   $(CODEGEN_DIR)/transpiler_intent_participant.c \
                   $(CODEGEN_DIR)/transpiler_intent_zone_binding_emit.c \
                   $(CODEGEN_DIR)/transpiler_intent_zone_slot.c \
                   $(CODEGEN_DIR)/transpiler_inventory_view.c \
                   $(CODEGEN_DIR)/transpiler_let_box_emit.c \
                   $(CODEGEN_DIR)/transpiler_let_channel_emit.c \
                   $(CODEGEN_DIR)/transpiler_let_collection_emit.c \
                   $(CODEGEN_DIR)/transpiler_let_emit.c \
                   $(CODEGEN_DIR)/transpiler_let_slot_emit.c \
                   $(CODEGEN_DIR)/transpiler_let_type_register_emit.c \
                   $(CODEGEN_DIR)/transpiler_lambda_emit.c \
                   $(CODEGEN_DIR)/transpiler_log_normalize.c \
                   $(CODEGEN_DIR)/transpiler_log_builtin_emit.c \
                   $(CODEGEN_DIR)/transpiler_mangled_name.c \
                   $(CODEGEN_DIR)/transpiler_misc_decl.c \
                   $(CODEGEN_DIR)/transpiler_mir_expr_ssa.c \
                   $(CODEGEN_DIR)/transpiler_mir_assignment_emit.c \
                   $(CODEGEN_DIR)/transpiler_mir_block_emit.c \
                   $(CODEGEN_DIR)/transpiler_mir_block_emit_helpers.c \
                   $(CODEGEN_DIR)/transpiler_mir_block_schedule_emit.c \
                   $(CODEGEN_DIR)/transpiler_mir_resource_hook_emit.c \
                   $(CODEGEN_DIR)/transpiler_mir_resource_op_emit.c \
                   $(CODEGEN_DIR)/transpiler_mir_self_field_slots.c \
                   $(CODEGEN_DIR)/transpiler_mir_stmt_emit.c \
                   $(CODEGEN_DIR)/transpiler_mir_ssa_contract.c \
                   $(CODEGEN_DIR)/transpiler_mir_effective_type.c \
                   $(CODEGEN_DIR)/transpiler_mir_emission_mapping_contract.c \
                   $(CODEGEN_DIR)/transpiler_mir_emission_contract.c \
                   $(CODEGEN_DIR)/transpiler_mir_emit_state.c \
                   $(CODEGEN_DIR)/transpiler_mir_func_emit.c \
                   $(CODEGEN_DIR)/transpiler_mir_func_ssa_locals_emit.c \
                   $(CODEGEN_DIR)/transpiler_mir_local_binding.c \
                   $(CODEGEN_DIR)/transpiler_mir_local_type_lookup.c \
                   $(CODEGEN_DIR)/transpiler_mir_ssa_local_facts.c \
                   $(CODEGEN_DIR)/transpiler_parallel_capture.c \
                   $(CODEGEN_DIR)/transpiler_mir_inventory_intent_alias_collect.c \
                   $(CODEGEN_DIR)/transpiler_mir_inventory_intent_collect.c \
                   $(CODEGEN_DIR)/transpiler_mir_intent_query.c \
                   $(CODEGEN_DIR)/transpiler_mir_cfg_policy.c \
                   $(CODEGEN_DIR)/transpiler_mir_cfg_control_emit.c \
                   $(CODEGEN_DIR)/transpiler_mir_destructure_emit.c \
                   $(CODEGEN_DIR)/transpiler_mir_match_pattern_emit.c \
                   $(CODEGEN_DIR)/transpiler_mir_match_payload_emit.c \
                   $(CODEGEN_DIR)/transpiler_mir_match_region_emit.c \
                   $(CODEGEN_DIR)/transpiler_mir_match_condition_emit.c \
                   $(CODEGEN_DIR)/transpiler_mir_pin_emit.c \
                   $(CODEGEN_DIR)/transpiler_mir_pending_uses.c \
                   $(CODEGEN_DIR)/transpiler_mir_phi_emit.c \
                   $(CODEGEN_DIR)/transpiler_mir_preserved_let_emit.c \
                   $(CODEGEN_DIR)/transpiler_mir_resource_op_core.c \
                   $(CODEGEN_DIR)/transpiler_mir_resource_name.c \
                   $(CODEGEN_DIR)/transpiler_mir_reason.c \
                   $(CODEGEN_DIR)/transpiler_mir_reason_classifier.c \
                   $(CODEGEN_DIR)/transpiler_mir_signature.c \
                   $(CODEGEN_DIR)/transpiler_mir_ssa_entry.c \
                   $(CODEGEN_DIR)/transpiler_mir_ssa_lookup.c \
                   $(CODEGEN_DIR)/transpiler_mir_ssa_map.c \
                   $(CODEGEN_DIR)/transpiler_mir_terminator_emit.c \
                   $(CODEGEN_DIR)/transpiler_mir_ssa_names.c \
                   $(CODEGEN_DIR)/transpiler_mir_ssa_utils.c \
                   $(CODEGEN_DIR)/transpiler_nominal.c \
                   $(CODEGEN_DIR)/transpiler_option_context.c \
                   $(CODEGEN_DIR)/transpiler_operator.c \
                   $(CODEGEN_DIR)/transpiler_overlay_host_fields.c \
                   $(CODEGEN_DIR)/transpiler_overlay_projection.c \
                   $(CODEGEN_DIR)/transpiler_overlay_zone_bind.c \
                   $(CODEGEN_DIR)/transpiler_overlay_zone_relation_bind.c \
                   $(CODEGEN_DIR)/transpiler_domain_receiver_query.c \
                   $(CODEGEN_DIR)/transpiler_projection_field_path.c \
                   $(CODEGEN_DIR)/transpiler_projection_method_invalidation.c \
                   $(CODEGEN_DIR)/transpiler_projection_sync.c \
                   $(CODEGEN_DIR)/transpiler_relation_effect_emit.c \
                   $(CODEGEN_DIR)/transpiler_projection.c \
                   $(CODEGEN_DIR)/transpiler_projection_emit.c \
                   $(CODEGEN_DIR)/transpiler_role_ability.c \
                   $(CODEGEN_DIR)/transpiler_slot_builtin_emit.c \
                   $(CODEGEN_DIR)/transpiler_spawn_channel_emit.c \
                   $(CODEGEN_DIR)/transpiler_slot_target.c \
                   $(CODEGEN_DIR)/transpiler_thread_pool.c \
                   $(CODEGEN_DIR)/transpiler_select.c \
                   $(CODEGEN_DIR)/transpiler_type_alias.c \
                   $(CODEGEN_DIR)/transpiler_type_declarator.c \
                   $(CODEGEN_DIR)/transpiler_type_mapping.c \
                   $(CODEGEN_DIR)/transpiler_type_name_utils.c \
                   $(CODEGEN_DIR)/transpiler_type_render.c \
                   $(CODEGEN_DIR)/transpiler_type_require.c \
                   $(CODEGEN_DIR)/transpiler_zone_decl_emit.c \
                   $(CODEGEN_DIR)/transpiler_zone_frontier_emit.c \
                   $(CODEGEN_DIR)/transpiler_zone_methods_emit.c \
                   $(CODEGEN_DIR)/transpiler_zone_specialization_emit.c \
                   $(CODEGEN_DIR)/transpiler_zone_struct_emit.c \
                   $(CODEGEN_DIR)/transpiler_world_select_event_emit.c \
                   $(CODEGEN_DIR)/transpiler.c
COMPILER_SOURCES = $(COMPILER_DIR)/compiler.c \
                   $(COMPILER_DIR)/compiler_result.c \
                   $(COMPILER_DIR)/compiler_process.c \
                   $(COMPILER_DIR)/compiler_llvm.c \
                   $(COMPILER_DIR)/dir.c \
                   $(COMPILER_DIR)/dir_storage.c \
                   $(COMPILER_DIR)/dir_collect.c \
                   $(COMPILER_DIR)/dir_collect_intent.c \
                   $(COMPILER_DIR)/dir_collect_domain.c \
                   $(COMPILER_DIR)/dir_validate.c \
                   $(COMPILER_DIR)/io_boundary_builtin.c \
                   $(COMPILER_DIR)/air_names.c \
                   $(COMPILER_DIR)/air_drift.c \
                   $(COMPILER_DIR)/air.c \
                   $(COMPILER_DIR)/air_boundary.c \
                   $(COMPILER_DIR)/air_boundary_walk.c \
                   $(COMPILER_DIR)/air_dump.c \
                   $(COMPILER_DIR)/air_dump_json.c \
                   $(COMPILER_DIR)/air_vocabulary.c \
                   $(COMPILER_DIR)/air_erasure_squiggle.c \
                   $(COMPILER_DIR)/execution_lane.c \
                   $(COMPILER_DIR)/air_execution_lane.c \
                   $(COMPILER_DIR)/air_boundary_evidence_policy.c \
                   $(COMPILER_DIR)/air_evidence_node.c \
                   $(COMPILER_DIR)/air_evidence_ast.c \
                   $(COMPILER_DIR)/air_evidence_hir.c \
                   $(COMPILER_DIR)/air_evidence_mir.c \
                   $(COMPILER_DIR)/air_evidence_mir_facts.c \
                   $(COMPILER_DIR)/air_evidence_mir_pin.c \
                   $(COMPILER_DIR)/air_evidence_runtime.c \
                   $(COMPILER_DIR)/air_evidence_dag.c \
                   $(COMPILER_DIR)/air_evidence_rir.c \
                   $(COMPILER_DIR)/air_evidence_rir_match.c \
                   $(COMPILER_DIR)/air_evidence_rir_propagation.c \
                   $(COMPILER_DIR)/air_evidence_rir_boundary.c \
                   $(COMPILER_DIR)/air_validate_global_evidence.c \
                   $(COMPILER_DIR)/air_validate_boundary_summary.c \
                   $(COMPILER_DIR)/air_validate_summary_counters.c \
                   $(COMPILER_DIR)/air_validate_boundary_evidence.c \
                   $(COMPILER_DIR)/air_validate_evidence.c \
                   $(COMPILER_DIR)/air_validate.c \
                   $(COMPILER_DIR)/air_verify_global.c \
                   $(COMPILER_DIR)/air_verify.c \
                   $(COMPILER_DIR)/air_verify_provenance.c \
                   $(COMPILER_DIR)/rir.c \
                   $(COMPILER_DIR)/rir_names.c \
                    $(COMPILER_DIR)/rir_public_surface.c \
                    $(COMPILER_DIR)/rir_validation.c \
                    $(COMPILER_DIR)/rir_validation_dir.c \
                    $(COMPILER_DIR)/rir_flow_state.c \
                    $(COMPILER_DIR)/rir_flow.c \
                    $(COMPILER_DIR)/rir_facts.c \
                   $(COMPILER_DIR)/rir_scope_storage.c \
                   $(COMPILER_DIR)/rir_builder.c \
                   $(COMPILER_DIR)/rir_builder_walk.c \
                   $(COMPILER_DIR)/rir_builder_intent.c \
                   $(COMPILER_DIR)/hir_analysis.c \
                   $(COMPILER_DIR)/hir_cfg.c \
                   $(COMPILER_DIR)/hir_cfg_phi.c \
                   $(COMPILER_DIR)/hir_lower_cfg_blocks.c \
                   $(COMPILER_DIR)/hir_lower_cfg.c \
                   $(COMPILER_DIR)/hir_lower_intent_cfg.c \
                   $(COMPILER_DIR)/hir_routine_cfg.c \
                   $(COMPILER_DIR)/hir_callgraph.c \
                   $(COMPILER_DIR)/mir.c \
                   $(COMPILER_DIR)/mir_branch_source_facts.c \
                   $(COMPILER_DIR)/mir_signature_metadata.c \
                   $(COMPILER_DIR)/mir_source_shape.c \
                   $(COMPILER_DIR)/mir_source_lifecycle_shape.c \
                   $(COMPILER_DIR)/mir_source_node_name.c \
                   $(COMPILER_DIR)/mir_source_local_types.c \
                   $(COMPILER_DIR)/mir_source_local_expr_call_facts.c \
                   $(COMPILER_DIR)/mir_source_local_expr_types.c \
                   $(COMPILER_DIR)/mir_source_inventory_build.c \
                   $(COMPILER_DIR)/mir_names.c \
                   $(COMPILER_DIR)/mir_lifecycle.c \
                   $(COMPILER_DIR)/mir_json_dump.c \
                   $(COMPILER_DIR)/mir_base_helpers.c \
                   $(COMPILER_DIR)/mir_program_inventory.c \
                   $(COMPILER_DIR)/mir_public_surface.c \
                   $(COMPILER_DIR)/mir_lower_population.c \
                   $(COMPILER_DIR)/mir_program_validate.c \
                   $(COMPILER_DIR)/mir_call_fact.c \
                   $(COMPILER_DIR)/mir_validation.c \
                   $(COMPILER_DIR)/mir_cleanup_fact_names.c \
                   $(COMPILER_DIR)/mir_cfg_contract_pin.c \
                   $(COMPILER_DIR)/mir_cfg_contract_control.c \
                   $(COMPILER_DIR)/mir_cfg_contract_cleanup_fact.c \
                   $(COMPILER_DIR)/mir_cfg_contract_roots.c \
                   $(COMPILER_DIR)/mir_cfg_contract_cleanup_roots.c \
                   $(COMPILER_DIR)/mir_cfg_contract_cleanup_root_membership.c \
                   $(COMPILER_DIR)/mir_cfg_contract_edges.c \
                   $(COMPILER_DIR)/mir_cfg_contract_validate.c \
                   $(COMPILER_DIR)/mir_cfg_contract_validate_cleanup.c \
                   $(COMPILER_DIR)/mir_abi_layout.c \
                   $(COMPILER_DIR)/mir_surface_usage.c \
                   $(COMPILER_DIR)/mir_fact_validate.c \
                   $(COMPILER_DIR)/mir_fact_surface_validate.c \
                   $(COMPILER_DIR)/mir_fact_terminator_validate.c \
                   $(COMPILER_DIR)/mir_ability_ref.c \
                   $(COMPILER_DIR)/mir_decl_header_authority.c \
                   $(COMPILER_DIR)/mir_decl_header_refresh.c \
                   $(COMPILER_DIR)/mir_decl_header_zone_state.c \
                   $(COMPILER_DIR)/mir_decl_header_zone_state_validate.c \
                   $(COMPILER_DIR)/mir_decl_header_shape.c \
                   $(COMPILER_DIR)/decl_field_model.c \
                   $(COMPILER_DIR)/mir_decl_header_fields.c \
                   $(COMPILER_DIR)/mir_decl_header_variants.c \
                   $(COMPILER_DIR)/mir_decl_header_role_validate.c \
                   $(COMPILER_DIR)/mir_decl_header_validate.c \
                   $(COMPILER_DIR)/mir_decl_header_access.c \
                   $(COMPILER_DIR)/mir_decl_header_zone_access.c \
                   $(COMPILER_DIR)/mir_decl_method_projection.c \
                   $(COMPILER_DIR)/mir_decl_headers.c \
                   $(COMPILER_DIR)/mir_stmt_population.c \
                   $(COMPILER_DIR)/mir_stmt_population_resource_ops.c \
                   $(COMPILER_DIR)/mir_stmt_population_source.c \
                   $(COMPILER_DIR)/mir_stmt_source_inventory.c \
                   $(COMPILER_DIR)/mir_stmt_source.c \
                   $(COMPILER_DIR)/mir_non_cfg_stmt_population.c \
                   $(COMPILER_DIR)/mir_ssa_rename.c \
                   $(COMPILER_DIR)/mir_ssa_use_edges.c \
                   $(COMPILER_DIR)/mir_liveness_dce.c \
                   $(COMPILER_DIR)/mir_liveness_summary.c \
                   $(COMPILER_DIR)/mir_dce.c \
                   $(COMPILER_DIR)/mir_cleanup.c \
                   $(COMPILER_DIR)/mir_intent.c \
                   $(COMPILER_DIR)/mir_intent_fact.c \
                   $(COMPILER_DIR)/mir_type_helpers.c \
                   $(COMPILER_DIR)/hir.c \
                   $(COMPILER_DIR)/hir_routines.c \
                   $(COMPILER_DIR)/hir_destroy.c \
                   $(COMPILER_DIR)/hir_public.c \
                   $(COMPILER_DIR)/hir_validate.c \
                   $(COMPILER_DIR)/module_loader.c \
                   $(COMPILER_DIR)/module_normalizer.c \
                   $(COMPILER_DIR)/module_normalizer_refs.c \
                   $(COMPILER_DIR)/module_normalizer_domain_refs.c \
                   $(COMPILER_DIR)/module_normalizer_scope.c \
                   $(COMPILER_DIR)/module_normalizer_shadow.c \
                   $(COMPILER_DIR)/import_stack.c \
                   $(COMPILER_DIR)/import_resolver.c \
                   $(COMPILER_DIR)/import_resolver_paths.c \
                   $(COMPILER_DIR)/driver_app.c \
                   $(COMPILER_DIR)/driver_usage.c \
                   $(COMPILER_DIR)/driver_diag.c \
                   $(COMPILER_DIR)/driver_scaffold.c \
                   $(COMPILER_DIR)/driver_scaffold_project.c \
                   $(COMPILER_DIR)/compiler_toolchain.c \
                   $(COMPILER_DIR)/compiler_runtime_cache.c \
                   $(COMPILER_DIR)/runtime_none_contract.c \
                   $(COMPILER_DIR)/forin_desugar.c \
                   $(COMPILER_DIR)/path_utils.c \
                   $(COMPILER_DIR)/llvm_runner.c \
                   $(COMPILER_DIR)/c_runner.c \
                   $(COMPILER_DIR)/repl.c \
                   $(COMPILER_DIR)/fmt_io.c \
                   $(COMPILER_DIR)/fmt_layout.c \
                   $(COMPILER_DIR)/fmt.c \
                   $(COMPILER_DIR)/pkg.c \
                   $(COMPILER_DIR)/pkg_lock.c \
                   $(COMPILER_DIR)/pkg_manifest.c \
                   $(COMPILER_DIR)/debugger.c \
                   $(COMPILER_DIR)/propagation_graph.c \
                   $(COMPILER_DIR)/propagation_graph_build.c

# LLVM backend sources (only compiled when LLVM_ENABLED=1)
ifneq ($(LLVM_ENABLED),0)
  LLVM_BACKEND_SOURCES = $(CODEGEN_DIR)/llvm_backend.c \
                   $(CODEGEN_DIR)/llvm_backend_ast_type.c \
                   $(CODEGEN_DIR)/llvm_backend_forward_declare.c \
                   $(CODEGEN_DIR)/llvm_backend_type_render.c \
                   $(CODEGEN_DIR)/llvm_backend_type_map.c \
                   $(CODEGEN_DIR)/llvm_backend_type_map_generics.c \
                        $(CODEGEN_DIR)/llvm_backend_type_registry.c \
                        $(CODEGEN_DIR)/llvm_boundary_slot_param.c \
                        $(CODEGEN_DIR)/llvm_debug_flags.c \
                        $(CODEGEN_DIR)/llvm_type.c \
                   $(CODEGEN_DIR)/llvm_api.c \
                   $(CODEGEN_DIR)/llvm_backend_generic.c \
                   $(CODEGEN_DIR)/llvm_pipeline.c \
                   $(CODEGEN_DIR)/llvm_main_wrapper.c \
                         $(CODEGEN_DIR)/llvm_intent.c \
                         $(CODEGEN_DIR)/llvm_intent_emit_support.c \
                         $(CODEGEN_DIR)/llvm_intent_setup.c \
                         $(CODEGEN_DIR)/llvm_intent_cleanup.c \
                         $(CODEGEN_DIR)/llvm_intent_step_context.c \
                         $(CODEGEN_DIR)/llvm_intent_trace.c \
                         $(CODEGEN_DIR)/llvm_registry.c \
                         $(CODEGEN_DIR)/llvm_registry_aux.c \
                         $(CODEGEN_DIR)/llvm_registry_arrays.c \
                         $(CODEGEN_DIR)/llvm_registry_collections.c \
                         $(CODEGEN_DIR)/llvm_registry_resources.c \
                         $(CODEGEN_DIR)/llvm_registry_resource_types.c \
                         $(CODEGEN_DIR)/llvm_error.c \
                          $(CODEGEN_DIR)/llvm_register.c \
                          $(CODEGEN_DIR)/llvm_runtime.c \
                          $(CODEGEN_DIR)/llvm_runtime_attrs.c \
                          $(CODEGEN_DIR)/llvm_runtime_bitcode_freshness.c \
                          $(CODEGEN_DIR)/llvm_runtime_aggregate_return.c \
                          $(CODEGEN_DIR)/llvm_runtime_core_builtin_decl.c \
                          $(CODEGEN_DIR)/llvm_runtime_require.c \
                          $(CODEGEN_DIR)/llvm_runtime_raw_collections.c \
                          $(CODEGEN_DIR)/llvm_runtime_channels.c \
                          $(CODEGEN_DIR)/llvm_runtime_secure_slot_decl.c \
                          $(CODEGEN_DIR)/llvm_runtime_task_memory_decl.c \
                        $(CODEGEN_DIR)/llvm_event.c \
                        $(CODEGEN_DIR)/llvm_mir_contract.c \
                        $(CODEGEN_DIR)/llvm_mir_signature.c \
                        $(CODEGEN_DIR)/llvm_mir_emit.c \
                        $(CODEGEN_DIR)/llvm_mir_await_emit.c \
                        $(CODEGEN_DIR)/llvm_mir_bind_emit.c \
                        $(CODEGEN_DIR)/llvm_mir_block_emit.c \
                        $(CODEGEN_DIR)/llvm_mir_lifecycle_emit.c \
                        $(CODEGEN_DIR)/llvm_mir_host_field.c \
                        $(CODEGEN_DIR)/llvm_mir_pin_region.c \
                        $(CODEGEN_DIR)/llvm_mir_resource_claim.c \
                        $(CODEGEN_DIR)/llvm_mir_resource_view.c \
                        $(CODEGEN_DIR)/llvm_mir_source_def_copy.c \
                        $(CODEGEN_DIR)/llvm_mir_source_resource_defs.c \
                        $(CODEGEN_DIR)/llvm_mir_store_coercion.c \
                        $(CODEGEN_DIR)/llvm_mir_async_fact.c \
                        $(CODEGEN_DIR)/llvm_mir_local_emit.c \
                        $(CODEGEN_DIR)/llvm_mir_local_array_registry.c \
                        $(CODEGEN_DIR)/llvm_mir_local_expected_type.c \
                        $(CODEGEN_DIR)/llvm_mir_local_element_type.c \
                        $(CODEGEN_DIR)/llvm_mir_local_type_lookup.c \
                        $(CODEGEN_DIR)/llvm_mir_scope_bind.c \
                        $(CODEGEN_DIR)/llvm_mir_slice_fact.c \
                        $(CODEGEN_DIR)/llvm_mir_param_emit.c \
                        $(CODEGEN_DIR)/llvm_mir_type_helpers.c \
                        $(CODEGEN_DIR)/llvm_mir_vars.c \
                        $(CODEGEN_DIR)/llvm_mir_phi.c \
                        $(CODEGEN_DIR)/llvm_mir_cfg_control.c \
                        $(CODEGEN_DIR)/llvm_mir_match_pattern.c \
                        $(CODEGEN_DIR)/llvm_mir_match_region.c \
                        $(CODEGEN_DIR)/llvm_mir_match_condition.c \
                        $(CODEGEN_DIR)/llvm_mir_for_in_control.c \
                        $(CODEGEN_DIR)/llvm_mir_loop_control.c \
                        $(CODEGEN_DIR)/llvm_intent_mir_meta.c \
                        $(CODEGEN_DIR)/llvm_intent_zone.c \
                        $(CODEGEN_DIR)/llvm_intent_zone_bind.c \
                        $(CODEGEN_DIR)/llvm_intent_effect.c \
                        $(CODEGEN_DIR)/llvm_intent_flow.c \
                        $(CODEGEN_DIR)/llvm_intent_forward.c \
                        $(CODEGEN_DIR)/llvm_inventory_internal.c \
                        $(CODEGEN_DIR)/llvm_inventory_decl_lookup.c \
                        $(CODEGEN_DIR)/llvm_inventory_field_view.c \
                        $(CODEGEN_DIR)/llvm_inventory_slot_view.c \
                        $(CODEGEN_DIR)/llvm_inventory_zone_refresh_view.c \
                        $(CODEGEN_DIR)/llvm_inventory_zone_state_view.c \
                        $(CODEGEN_DIR)/llvm_inventory_role_roster_slot_view.c \
                        $(CODEGEN_DIR)/llvm_inventory_host_methods.c \
                        $(CODEGEN_DIR)/llvm_expr.c \
                        $(CODEGEN_DIR)/llvm_expr_aggregate.c \
                        $(CODEGEN_DIR)/llvm_expr_array_access.c \
                        $(CODEGEN_DIR)/llvm_expr_emit_support.c \
                        $(CODEGEN_DIR)/llvm_expr_aggregate_utils.c \
                        $(CODEGEN_DIR)/llvm_expr_assignment_member_projection.c \
                        $(CODEGEN_DIR)/llvm_expr_assignment_projection.c \
                        $(CODEGEN_DIR)/llvm_expr_await_task.c \
                        $(CODEGEN_DIR)/llvm_expr_banner_string_helpers.c \
                        $(CODEGEN_DIR)/llvm_expr_boundary_projection_helpers.c \
                        $(CODEGEN_DIR)/llvm_expr_common.c \
                        $(CODEGEN_DIR)/llvm_expr_array_calls.c \
                        $(CODEGEN_DIR)/llvm_expr_array_raw_nominal_calls.c \
                        $(CODEGEN_DIR)/llvm_expr_allocator_calls.c \
                        $(CODEGEN_DIR)/llvm_expr_box_array_calls.c \
                        $(CODEGEN_DIR)/llvm_expr_call_args.c \
                        $(CODEGEN_DIR)/llvm_expr_call_collections_map_exports.c \
                        $(CODEGEN_DIR)/llvm_expr_call_collections_extended.c \
                        $(CODEGEN_DIR)/llvm_expr_call_list_extended.c \
                        $(CODEGEN_DIR)/llvm_expr_call_collections_require.c \
                        $(CODEGEN_DIR)/llvm_expr_call_dispatch.c \
                        $(CODEGEN_DIR)/llvm_expr_call_intent_policy.c \
                        $(CODEGEN_DIR)/llvm_expr_call_errors.c \
                        $(CODEGEN_DIR)/llvm_expr_call_hosted.c \
                        $(CODEGEN_DIR)/llvm_expr_call_variable.c \
                        $(CODEGEN_DIR)/llvm_expr_call_queue_extended.c \
                        $(CODEGEN_DIR)/llvm_expr_call_methods_domain_slice.c \
                        $(CODEGEN_DIR)/llvm_expr_call_methods_vtable_dispatch.c \
                        $(CODEGEN_DIR)/llvm_expr_call_methods_world_effect_sync.c \
                        $(CODEGEN_DIR)/llvm_expr_call_projection_sync.c \
                        $(CODEGEN_DIR)/llvm_channel_target.c \
                        $(CODEGEN_DIR)/llvm_expr_channel.c \
                        $(CODEGEN_DIR)/llvm_expr_collection_base_calls.c \
                        $(CODEGEN_DIR)/llvm_expr_constructor_calls.c \
                        $(CODEGEN_DIR)/llvm_expr_constructor_channel_guard.c \
                        $(CODEGEN_DIR)/llvm_expr_event_calls.c \
                        $(CODEGEN_DIR)/llvm_expr_helpers.c \
                        $(CODEGEN_DIR)/llvm_expr_domain_query_calls.c \
                        $(CODEGEN_DIR)/llvm_expr_domain_query_utils.c \
                        $(CODEGEN_DIR)/llvm_expr_projection_path_helpers.c \
                        $(CODEGEN_DIR)/llvm_expr_host_spawn_literal_helpers.c \
                        $(CODEGEN_DIR)/llvm_expr_identifier_slot_helpers.c \
                        $(CODEGEN_DIR)/llvm_expr_call_inline_policy.c \
                        $(CODEGEN_DIR)/llvm_member_call_emit.c \
                        $(CODEGEN_DIR)/llvm_member_call_specialize.c \
                        $(CODEGEN_DIR)/llvm_member_call_support.c \
                        $(CODEGEN_DIR)/llvm_expr_intent_observability_calls.c \
                        $(CODEGEN_DIR)/llvm_expr_log_calls.c \
                        $(CODEGEN_DIR)/llvm_expr_math_calls.c \
                        $(CODEGEN_DIR)/llvm_expr_member_access.c \
                        $(CODEGEN_DIR)/llvm_expr_member_lvalue.c \
                        $(CODEGEN_DIR)/llvm_expr_rc_calls.c \
                        $(CODEGEN_DIR)/llvm_expr_result_option_calls.c \
                        $(CODEGEN_DIR)/llvm_expr_scalar_core.c \
                        $(CODEGEN_DIR)/llvm_expr_unary_core.c \
                        $(CODEGEN_DIR)/llvm_expr_slot_device_calls.c \
                        $(CODEGEN_DIR)/llvm_expr_slot_runtime_utils.c \
                        $(CODEGEN_DIR)/llvm_expr_spawn_names.c \
                        $(CODEGEN_DIR)/llvm_expr_spawn_generic.c \
                        $(CODEGEN_DIR)/llvm_expr_spawn_call_helpers.c \
                        $(CODEGEN_DIR)/llvm_expr_spawn_worker_boundary.c \
                        $(CODEGEN_DIR)/llvm_expr_stdlib_scalar_io_calls.c \
                        $(CODEGEN_DIR)/llvm_expr_string_coerce.c \
                        $(CODEGEN_DIR)/llvm_expr_task_calls.c \
                        $(CODEGEN_DIR)/llvm_expr_task_channel_policy.c \
                        $(CODEGEN_DIR)/llvm_expr_task_channel_calls.c \
                        $(CODEGEN_DIR)/llvm_stmt.c \
                        $(CODEGEN_DIR)/llvm_stmt_emit_support.c \
                        $(CODEGEN_DIR)/llvm_stmt_defer_scope.c \
                        $(CODEGEN_DIR)/llvm_stmt_array_type_infer.c \
                        $(CODEGEN_DIR)/llvm_stmt_type_infer.c \
                        $(CODEGEN_DIR)/llvm_stmt_type_infer_call.c \
                        $(CODEGEN_DIR)/llvm_stmt_source_local_fallback.c \
                        $(CODEGEN_DIR)/llvm_stmt_type_infer_nominal.c \
                        $(CODEGEN_DIR)/llvm_stmt_type_infer_await.c \
                        $(CODEGEN_DIR)/llvm_stmt_type_infer_helpers.c \
                        $(CODEGEN_DIR)/llvm_stmt_let_callable.c \
                        $(CODEGEN_DIR)/llvm_stmt_lambda_type.c \
                        $(CODEGEN_DIR)/llvm_stmt_let_collection_policy.c \
                        $(CODEGEN_DIR)/llvm_stmt_let_collections.c \
                        $(CODEGEN_DIR)/llvm_stmt_let_helpers.c \
                        $(CODEGEN_DIR)/llvm_stmt_let_slots.c \
                        $(CODEGEN_DIR)/llvm_stmt_let_resources.c \
                        $(CODEGEN_DIR)/llvm_stmt_let_names.c \
                        $(CODEGEN_DIR)/llvm_stmt_let_with.c \
                        $(CODEGEN_DIR)/llvm_stmt_destructure.c \
                        $(CODEGEN_DIR)/llvm_stmt_with.c \
                        $(CODEGEN_DIR)/llvm_stmt_loop_match.c \
                        $(CODEGEN_DIR)/llvm_stmt_match.c \
                        $(CODEGEN_DIR)/llvm_stmt_parallel_async.c \
                        $(CODEGEN_DIR)/llvm_stmt_select.c \
                        $(CODEGEN_DIR)/llvm_stmt_parallel_names.c \
                        $(CODEGEN_DIR)/llvm_stmt_type_render.c \
                        $(CODEGEN_DIR)/llvm_stmt_zone_action.c \
                        $(CODEGEN_DIR)/llvm_decl.c \
                        $(CODEGEN_DIR)/llvm_decl_authority.c \
                        $(CODEGEN_DIR)/llvm_decl_routines.c \
                        $(CODEGEN_DIR)/llvm_domain_method_helpers.c \
                        $(CODEGEN_DIR)/llvm_domain_method_emit.c \
                        $(CODEGEN_DIR)/llvm_domain_event.c \
                        $(CODEGEN_DIR)/llvm_domain_lookup.c \
                        $(CODEGEN_DIR)/llvm_domain_projection_count.c \
                        $(CODEGEN_DIR)/llvm_domain_projection_value_helpers.c \
                        $(CODEGEN_DIR)/llvm_domain_projection_sync_helpers.c \
                        $(CODEGEN_DIR)/llvm_domain_projection_sync_body_helpers.c \
                        $(CODEGEN_DIR)/llvm_domain_role_lookup.c \
                        $(CODEGEN_DIR)/llvm_domain_role_emit.c \
                        $(CODEGEN_DIR)/llvm_domain_sync_frontier.c \
                        $(CODEGEN_DIR)/llvm_domain_zone_frontier_state.c \
                        $(CODEGEN_DIR)/llvm_domain_zone_sync.c \
                        $(CODEGEN_DIR)/llvm_domain_zone_sync_clauses.c \
                        $(CODEGEN_DIR)/llvm_domain_zone_sync_relations.c \
                        $(CODEGEN_DIR)/llvm_domain_zone_bind_lowering.c \
                        $(CODEGEN_DIR)/llvm_domain_world_sync_directives.c \
                        $(CODEGEN_DIR)/llvm_domain_world_frontier_derived.c \
                        $(CODEGEN_DIR)/llvm_domain_world_frontier.c \
                        $(CODEGEN_DIR)/llvm_domain_world_frontier_zones.c \
                        $(CODEGEN_DIR)/llvm_domain_world_sync.c \
                         $(CODEGEN_DIR)/llvm_domain_forward.c \
                         $(CODEGEN_DIR)/llvm_domain_forward_ability.c \
                         $(CODEGEN_DIR)/llvm_domain_forward_role.c \
                         $(CODEGEN_DIR)/llvm_domain_struct_fields.c \
                         $(CODEGEN_DIR)/llvm_domain_struct_register.c \
                         $(CODEGEN_DIR)/llvm_domain_struct_register_fields.c \
                         $(CODEGEN_DIR)/llvm_domain.c
  RUNTIME_LIB_SOURCES  = $(RUNTIME_DIR)/pgy_runtime_lib.c
  LLVM_BACKEND_OBJECTS = $(LLVM_BACKEND_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
  RUNTIME_LIB_OBJECTS  = $(RUNTIME_LIB_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
else
  LLVM_BACKEND_OBJECTS =
  RUNTIME_LIB_OBJECTS  =
endif

# Test & driver sources
MAIN_SOURCE             = $(SRC_DIR)/main.c
PARSER_TEST_SOURCE      = $(SRC_DIR)/test_parser.c
TEST_DATASTRUCTURES_SRC = $(SRC_DIR)/test_datastructures.c
TEST_SECURITY_SRC       = $(SRC_DIR)/test_security.c
TEST_SEMANTIC_SRC       = $(SRC_DIR)/test_semantic.c
TEST_TRANSPILE_SRC      = $(SRC_DIR)/test_transpile.c
TEST_MEMORY_SRC         = $(SRC_DIR)/test_memory_layout.c
TEST_ABI_SRC            = $(SRC_DIR)/test_abi_spec.c
TEST_ABI_PIPELINE_SRC   = $(SRC_DIR)/test_abi_pipeline.c
TEST_CONCURRENCY_SRC    = $(SRC_DIR)/test_concurrency.c
TEST_DIR_SRC            = $(SRC_DIR)/test_dir.c
TEST_AIR_SRC            = $(SRC_DIR)/test_air.c
TEST_RIR_SRC            = $(SRC_DIR)/test_rir.c
TEST_MIR_SRC            = $(SRC_DIR)/test_mir.c
TEST_HIR_SRC            = $(SRC_DIR)/test_hir.c
DRIVER_SRC              = $(SRC_DIR)/pgy_driver.c
LSP_SRC                 = $(SRC_DIR)/lsp/pgy_lsp.c \
                          $(SRC_DIR)/lsp/pgy_lsp_protocol.c \
                          $(SRC_DIR)/lsp/pgy_lsp_features.c \
                          $(SRC_DIR)/lsp/pgy_lsp_hover.c \
                          $(SRC_DIR)/lsp/pgy_lsp_diagnostics.c

# -----------------------------------------------------------------
# Object files
# -----------------------------------------------------------------
COMMON_OBJECTS   = $(COMMON_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
LEXER_OBJECTS    = $(LEXER_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
PARSER_OBJECTS   = $(PARSER_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
RUNTIME_OBJECTS  = $(RUNTIME_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
ifneq ($(ENABLE_ASM_FASTPATH),1)
RUNTIME_ASM_OBJECTS =
else ifeq ($(NASM),)
RUNTIME_ASM_OBJECTS =
else
RUNTIME_ASM_OBJECTS = $(RUNTIME_ASM_SOURCES:$(SRC_DIR)/%.s=$(BUILD_DIR)/%.o)
endif
SEMANTIC_OBJECTS = $(SEMANTIC_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
CODEGEN_OBJECTS  = $(CODEGEN_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
COMPILER_OBJECTS = $(COMPILER_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

BUILD_SOURCE_INVENTORY_SOURCES = \
                   $(COMMON_SOURCES) \
                   $(LEXER_SOURCES) \
                   $(PARSER_SOURCES) \
                   $(RUNTIME_SOURCES) \
                   $(SEMANTIC_SOURCES) \
                   $(CODEGEN_SOURCES) \
                   $(COMPILER_SOURCES) \
                   $(LLVM_BACKEND_SOURCES) \
                   $(RUNTIME_LIB_SOURCES) \
                   $(MAIN_SOURCE) \
                   $(PARSER_TEST_SOURCE) \
                   $(TEST_DATASTRUCTURES_SRC) \
                   $(TEST_SECURITY_SRC) \
                   $(TEST_SEMANTIC_SRC) \
                   $(TEST_TRANSPILE_SRC) \
                   $(TEST_MEMORY_SRC) \
                   $(TEST_ABI_SRC) \
                   $(TEST_ABI_PIPELINE_SRC) \
                   $(TEST_CONCURRENCY_SRC) \
                   $(TEST_DIR_SRC) \
                   $(TEST_AIR_SRC) \
                   $(TEST_RIR_SRC) \
                   $(TEST_MIR_SRC) \
                   $(TEST_HIR_SRC) \
                   $(DRIVER_SRC) \
                   $(LSP_SRC)

BUILD_CONTRACT_INVENTORY_FILES = \
                   $(RUNTIME_DIR)/pgy_runtime_lib_array_map_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_channel_int_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_channel_string_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_channel_string_result_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_device_slot_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_dir_walk_core.h \
                   $(RUNTIME_DIR)/pgy_runtime_dir_walk_inline.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_dir_walk_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_io_string_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_process_args_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_process_exit.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_qubit_state_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_raw_array_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_raw_collection_common_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_raw_map_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_raw_map_key_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_raw_queue_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_raw_set_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_intent_active_index_exports.c \
                   $(RUNTIME_DIR)/pgy_runtime_lib_intent_trace_events_exports.c \
                   $(RUNTIME_DIR)/pgy_runtime_lib_mir_trace_exports.c \
                   $(RUNTIME_DIR)/pgy_runtime_lib_set_intent_trace_exports.c \
                   $(RUNTIME_DIR)/pgy_runtime_lib_secure_slot_exports.h \
                   tests/build_source_inventory_smoke.sh \
                   tests/dogfood_webgl_smoke.sh \
                   tests/wasm_backend_parity_smoke.sh \
                   tests/runtime_frontier_policy_smoke.sh

MAIN_OBJECT            = $(BUILD_DIR)/main.o
PARSER_TEST_OBJECT     = $(BUILD_DIR)/test_parser.o
TEST_DATASTRUCTURES_OBJ = $(BUILD_DIR)/test_datastructures.o
TEST_SECURITY_OBJ      = $(BUILD_DIR)/test_security.o
TEST_SEMANTIC_OBJ      = $(BUILD_DIR)/test_semantic.o
TEST_TRANSPILE_OBJ     = $(BUILD_DIR)/test_transpile.o
TEST_MEMORY_OBJ        = $(BUILD_DIR)/test_memory_layout.o
TEST_ABI_OBJ           = $(BUILD_DIR)/test_abi_spec.o
TEST_ABI_PIPELINE_OBJ  = $(BUILD_DIR)/test_abi_pipeline.o
TEST_CONCURRENCY_OBJ   = $(BUILD_DIR)/test_concurrency.o
TEST_DIR_OBJ           = $(BUILD_DIR)/test_dir.o
TEST_AIR_OBJ           = $(BUILD_DIR)/test_air.o
TEST_RIR_OBJ           = $(BUILD_DIR)/test_rir.o
TEST_MIR_OBJ           = $(BUILD_DIR)/test_mir.o
TEST_HIR_OBJ           = $(BUILD_DIR)/test_hir.o
DRIVER_OBJ             = $(BUILD_DIR)/pgy_driver.o
LSP_OBJECTS            = $(LSP_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Common frontend objects used by many targets
FRONTEND_OBJECTS = $(COMMON_OBJECTS) $(LEXER_OBJECTS) $(PARSER_OBJECTS) \
                   $(SEMANTIC_OBJECTS) $(CODEGEN_OBJECTS) $(COMPILER_OBJECTS) \
                   $(LLVM_BACKEND_OBJECTS) $(RUNTIME_LIB_OBJECTS)
SEMANTIC_LINK_SUPPORT = $(BUILD_DIR)/compiler/import_stack.o \
                        $(BUILD_DIR)/compiler/import_resolver.o \
                        $(BUILD_DIR)/compiler/import_resolver_paths.o \
                        $(BUILD_DIR)/compiler/module_normalizer.o \
                        $(BUILD_DIR)/compiler/module_normalizer_refs.o \
                        $(BUILD_DIR)/compiler/module_normalizer_domain_refs.o \
                        $(BUILD_DIR)/compiler/module_normalizer_scope.o \
                        $(BUILD_DIR)/compiler/module_normalizer_shadow.o \
                        $(BUILD_DIR)/compiler/decl_field_model.o \
                        $(BUILD_DIR)/compiler/path_utils.o
DIR_CORE_OBJECTS = $(BUILD_DIR)/compiler/dir.o \
                   $(BUILD_DIR)/compiler/dir_storage.o \
                   $(BUILD_DIR)/compiler/dir_collect.o \
                   $(BUILD_DIR)/compiler/dir_collect_intent.o \
                   $(BUILD_DIR)/compiler/dir_collect_domain.o \
                   $(BUILD_DIR)/compiler/dir_validate.o
HIR_CORE_OBJECTS = $(BUILD_DIR)/compiler/hir_analysis.o \
                   $(BUILD_DIR)/compiler/hir_cfg.o \
                   $(BUILD_DIR)/compiler/hir_cfg_phi.o \
                   $(BUILD_DIR)/compiler/hir_lower_cfg_blocks.o \
                   $(BUILD_DIR)/compiler/hir_lower_cfg.o \
                   $(BUILD_DIR)/compiler/hir_lower_intent_cfg.o \
                   $(BUILD_DIR)/compiler/hir_routine_cfg.o \
                   $(BUILD_DIR)/compiler/hir_callgraph.o \
                   $(BUILD_DIR)/compiler/hir.o \
                   $(BUILD_DIR)/compiler/hir_routines.o \
                   $(BUILD_DIR)/compiler/hir_destroy.o \
                   $(BUILD_DIR)/compiler/hir_public.o \
                   $(BUILD_DIR)/compiler/hir_validate.o
RIR_CORE_OBJECTS = $(BUILD_DIR)/compiler/rir.o \
                   $(BUILD_DIR)/compiler/io_boundary_builtin.o \
                   $(BUILD_DIR)/compiler/rir_names.o \
                    $(BUILD_DIR)/compiler/rir_public_surface.o \
                    $(BUILD_DIR)/compiler/rir_validation.o \
                    $(BUILD_DIR)/compiler/rir_validation_dir.o \
                    $(BUILD_DIR)/compiler/rir_flow_state.o \
                    $(BUILD_DIR)/compiler/rir_flow.o \
                   $(BUILD_DIR)/compiler/rir_facts.o \
                   $(BUILD_DIR)/compiler/rir_scope_storage.o \
                   $(BUILD_DIR)/compiler/rir_builder.o \
                   $(BUILD_DIR)/compiler/rir_builder_walk.o \
                   $(BUILD_DIR)/compiler/rir_builder_intent.o
AIR_CORE_OBJECTS = $(BUILD_DIR)/compiler/air_names.o \
                   $(BUILD_DIR)/compiler/air_drift.o \
                   $(BUILD_DIR)/compiler/io_boundary_builtin.o \
                   $(BUILD_DIR)/compiler/air.o \
                   $(BUILD_DIR)/compiler/air_boundary.o \
                   $(BUILD_DIR)/compiler/air_boundary_walk.o \
                   $(BUILD_DIR)/compiler/air_dump.o \
                   $(BUILD_DIR)/compiler/air_dump_json.o \
                   $(BUILD_DIR)/compiler/air_vocabulary.o \
                   $(BUILD_DIR)/compiler/air_erasure_squiggle.o \
                   $(BUILD_DIR)/compiler/execution_lane.o \
                   $(BUILD_DIR)/compiler/air_execution_lane.o \
                   $(BUILD_DIR)/compiler/air_boundary_evidence_policy.o \
                   $(BUILD_DIR)/compiler/air_evidence_node.o \
                   $(BUILD_DIR)/compiler/air_evidence_ast.o \
                   $(BUILD_DIR)/compiler/air_evidence_hir.o \
                   $(BUILD_DIR)/compiler/air_evidence_mir.o \
                   $(BUILD_DIR)/compiler/air_evidence_mir_facts.o \
                   $(BUILD_DIR)/compiler/air_evidence_mir_pin.o \
                   $(BUILD_DIR)/compiler/air_evidence_runtime.o \
                   $(BUILD_DIR)/compiler/air_evidence_dag.o \
                   $(BUILD_DIR)/compiler/mir_program_inventory.o \
                   $(BUILD_DIR)/compiler/mir_signature_metadata.o \
                   $(BUILD_DIR)/compiler/mir_source_shape.o \
                   $(BUILD_DIR)/compiler/mir_source_lifecycle_shape.o \
                   $(BUILD_DIR)/compiler/mir_source_node_name.o \
                   $(BUILD_DIR)/compiler/mir_source_local_types.o \
                   $(BUILD_DIR)/compiler/mir_source_local_expr_call_facts.o \
                   $(BUILD_DIR)/compiler/mir_source_local_expr_types.o \
                   $(BUILD_DIR)/compiler/mir_decl_header_access.o \
                   $(BUILD_DIR)/compiler/mir_type_helpers.o \
                   $(BUILD_DIR)/compiler/mir_intent_fact.o \
                   $(BUILD_DIR)/compiler/mir_stmt_source.o \
                   $(BUILD_DIR)/compiler/mir_cleanup_fact_names.o \
                   $(BUILD_DIR)/compiler/mir_cfg_contract_pin.o \
                   $(BUILD_DIR)/compiler/mir_cfg_contract_control.o \
                   $(BUILD_DIR)/compiler/mir_cfg_contract_cleanup_fact.o \
                   $(BUILD_DIR)/compiler/mir_cfg_contract_cleanup_root_membership.o \
                   $(BUILD_DIR)/compiler/air_evidence_rir.o \
                   $(BUILD_DIR)/compiler/air_evidence_rir_match.o \
                   $(BUILD_DIR)/compiler/air_evidence_rir_propagation.o \
                   $(BUILD_DIR)/compiler/air_evidence_rir_boundary.o \
                   $(BUILD_DIR)/compiler/air_validate_global_evidence.o \
                   $(BUILD_DIR)/compiler/air_validate_boundary_summary.o \
                   $(BUILD_DIR)/compiler/air_validate_summary_counters.o \
                   $(BUILD_DIR)/compiler/air_validate_boundary_evidence.o \
                   $(BUILD_DIR)/compiler/air_validate_evidence.o \
                   $(BUILD_DIR)/compiler/air_validate.o \
                   $(BUILD_DIR)/compiler/air_verify_global.o \
                   $(BUILD_DIR)/compiler/air_verify.o \
                   $(BUILD_DIR)/compiler/air_verify_provenance.o
MIR_CORE_OBJECTS = $(BUILD_DIR)/compiler/mir.o \
                   $(BUILD_DIR)/compiler/mir_branch_source_facts.o \
                   $(BUILD_DIR)/compiler/mir_signature_metadata.o \
                   $(BUILD_DIR)/compiler/mir_source_shape.o \
                   $(BUILD_DIR)/compiler/mir_source_lifecycle_shape.o \
                   $(BUILD_DIR)/compiler/mir_source_node_name.o \
                   $(BUILD_DIR)/compiler/mir_source_local_types.o \
                   $(BUILD_DIR)/compiler/mir_source_local_expr_call_facts.o \
                   $(BUILD_DIR)/compiler/mir_source_local_expr_types.o \
                   $(BUILD_DIR)/compiler/mir_source_inventory_build.o \
                   $(BUILD_DIR)/compiler/mir_names.o \
                   $(BUILD_DIR)/compiler/mir_lifecycle.o \
                   $(BUILD_DIR)/compiler/mir_json_dump.o \
                   $(BUILD_DIR)/compiler/mir_base_helpers.o \
                   $(BUILD_DIR)/compiler/mir_program_inventory.o \
                   $(BUILD_DIR)/compiler/mir_public_surface.o \
                   $(BUILD_DIR)/compiler/mir_lower_population.o \
                   $(BUILD_DIR)/compiler/mir_program_validate.o \
                   $(BUILD_DIR)/compiler/mir_call_fact.o \
                   $(BUILD_DIR)/compiler/mir_validation.o \
                   $(BUILD_DIR)/compiler/mir_cleanup_fact_names.o \
                   $(BUILD_DIR)/compiler/mir_cfg_contract_pin.o \
                   $(BUILD_DIR)/compiler/mir_cfg_contract_control.o \
                   $(BUILD_DIR)/compiler/mir_cfg_contract_cleanup_fact.o \
                   $(BUILD_DIR)/compiler/mir_cfg_contract_roots.o \
                   $(BUILD_DIR)/compiler/mir_cfg_contract_cleanup_roots.o \
                   $(BUILD_DIR)/compiler/mir_cfg_contract_cleanup_root_membership.o \
                   $(BUILD_DIR)/compiler/mir_cfg_contract_edges.o \
                   $(BUILD_DIR)/compiler/mir_cfg_contract_validate.o \
                   $(BUILD_DIR)/compiler/mir_cfg_contract_validate_cleanup.o \
                   $(BUILD_DIR)/compiler/mir_abi_layout.o \
                   $(BUILD_DIR)/compiler/mir_surface_usage.o \
                   $(BUILD_DIR)/compiler/mir_fact_validate.o \
                   $(BUILD_DIR)/compiler/mir_fact_surface_validate.o \
                   $(BUILD_DIR)/compiler/mir_fact_terminator_validate.o \
                   $(BUILD_DIR)/compiler/mir_ability_ref.o \
                   $(BUILD_DIR)/compiler/mir_decl_header_authority.o \
                   $(BUILD_DIR)/compiler/mir_decl_header_refresh.o \
                   $(BUILD_DIR)/compiler/mir_decl_header_zone_state.o \
                   $(BUILD_DIR)/compiler/mir_decl_header_zone_state_validate.o \
                   $(BUILD_DIR)/compiler/mir_decl_header_shape.o \
                   $(BUILD_DIR)/compiler/mir_decl_header_fields.o \
                   $(BUILD_DIR)/compiler/mir_decl_header_variants.o \
                   $(BUILD_DIR)/compiler/mir_decl_header_role_validate.o \
                   $(BUILD_DIR)/compiler/mir_decl_header_validate.o \
                   $(BUILD_DIR)/compiler/mir_decl_header_access.o \
                   $(BUILD_DIR)/compiler/mir_decl_header_zone_access.o \
                   $(BUILD_DIR)/compiler/mir_decl_method_projection.o \
                   $(BUILD_DIR)/compiler/mir_decl_headers.o \
                   $(BUILD_DIR)/compiler/mir_stmt_population.o \
                   $(BUILD_DIR)/compiler/mir_stmt_population_resource_ops.o \
                   $(BUILD_DIR)/compiler/mir_stmt_population_source.o \
                   $(BUILD_DIR)/compiler/mir_stmt_source_inventory.o \
                   $(BUILD_DIR)/compiler/mir_stmt_source.o \
                   $(BUILD_DIR)/compiler/mir_non_cfg_stmt_population.o \
                   $(BUILD_DIR)/compiler/mir_ssa_rename.o \
                   $(BUILD_DIR)/compiler/mir_ssa_use_edges.o \
                   $(BUILD_DIR)/compiler/mir_liveness_dce.o \
                   $(BUILD_DIR)/compiler/mir_liveness_summary.o \
                   $(BUILD_DIR)/compiler/mir_dce.o \
                   $(BUILD_DIR)/compiler/mir_cleanup.o \
                   $(BUILD_DIR)/compiler/mir_intent.o \
                   $(BUILD_DIR)/compiler/mir_intent_fact.o \
                   $(BUILD_DIR)/compiler/mir_type_helpers.o

ALL_BUILD_OBJECTS = $(sort \
                   $(FRONTEND_OBJECTS) \
                   $(MAIN_OBJECT) \
                   $(PARSER_TEST_OBJECT) \
                   $(TEST_DATASTRUCTURES_OBJ) \
                   $(TEST_SECURITY_OBJ) \
                   $(TEST_SEMANTIC_OBJ) \
                   $(TEST_TRANSPILE_OBJ) \
                   $(TEST_MEMORY_OBJ) \
                   $(TEST_ABI_OBJ) \
                   $(TEST_ABI_PIPELINE_OBJ) \
                   $(TEST_CONCURRENCY_OBJ) \
                   $(TEST_DIR_OBJ) \
                   $(TEST_AIR_OBJ) \
                   $(TEST_RIR_OBJ) \
                   $(TEST_MIR_OBJ) \
                   $(TEST_HIR_OBJ) \
                   $(DRIVER_OBJ) \
                   $(LSP_OBJECTS) \
                   $(SEMANTIC_LINK_SUPPORT) \
                   $(DIR_CORE_OBJECTS) \
                   $(HIR_CORE_OBJECTS) \
                   $(RIR_CORE_OBJECTS) \
                   $(AIR_CORE_OBJECTS) \
                   $(MIR_CORE_OBJECTS) \
                   $(RUNTIME_ASM_OBJECTS))
BUILD_ARTIFACT_GLOBS = $(BUILD_DIR)/*.o $(BUILD_DIR)/*/*.o $(BUILD_DIR)/*/*/*.o \
                       $(BUILD_DIR)/*.d $(BUILD_DIR)/*/*.d $(BUILD_DIR)/*/*/*.d
ALL_DEP_FILES = $(wildcard $(BUILD_DIR)/*.d) \
                $(wildcard $(BUILD_DIR)/*/*.d) \
                $(wildcard $(BUILD_DIR)/*/*/*.d)

# -----------------------------------------------------------------
# Executables
# -----------------------------------------------------------------
LEXER_TEST          = $(BIN_DIR)/lexer_test$(EXEEXT)
PARSER_TEST         = $(BIN_DIR)/test_parser$(EXEEXT)
DATASTRUCTURES_TEST = $(BIN_DIR)/test_datastructures$(EXEEXT)
SECURITY_TEST       = $(BIN_DIR)/test_security$(EXEEXT)
SEMANTIC_TEST       = $(BIN_DIR)/test_semantic$(EXEEXT)
TRANSPILE_TEST      = $(BIN_DIR)/test_transpile$(EXEEXT)
MEMORY_TEST         = $(BIN_DIR)/test_memory_layout$(EXEEXT)
ABI_TEST            = $(BIN_DIR)/test_abi_spec$(EXEEXT)
ABI_PIPELINE_TEST   = $(BIN_DIR)/test_abi_pipeline$(EXEEXT)
CONCURRENCY_TEST    = $(BIN_DIR)/test_concurrency$(EXEEXT)
DIR_TEST            = $(BIN_DIR)/test_dir$(EXEEXT)
AIR_TEST            = $(BIN_DIR)/test_air$(EXEEXT)
RIR_TEST            = $(BIN_DIR)/test_rir$(EXEEXT)
MIR_TEST            = $(BIN_DIR)/test_mir$(EXEEXT)
HIR_TEST            = $(BIN_DIR)/test_hir$(EXEEXT)
PGY                 = $(BIN_DIR)/pgy$(EXEEXT)
PGY_LSP             = $(BIN_DIR)/pgy-lsp$(EXEEXT)
ifeq ($(EXEEXT),.exe)
RUNTIME_OBJ_EXT := .obj
else
RUNTIME_OBJ_EXT := .o
endif
ABI_PERF_RUNTIME_RELEASE_OBS0 = $(TMPDIR)/pgy_runtime_cache_release_obs0$(RUNTIME_OBJ_EXT)
ABI_PERF_RUNTIME_RELEASE_OBS1 = $(TMPDIR)/pgy_runtime_cache_release_obs1$(RUNTIME_OBJ_EXT)
ifneq ($(LLD),)
ABI_PERF_LINKER_ENV = PGY_USE_LLD=1
else
ABI_PERF_LINKER_ENV =
endif

# -----------------------------------------------------------------
# Default target - build user-facing binaries only. Test binaries are explicit
# through all-with-tests / test-* targets so normal builds stay cheap.
# -----------------------------------------------------------------
all: $(PGY) $(PGY_LSP)

FRONTEND_TEST_BINARIES = $(LEXER_TEST) $(PARSER_TEST) $(DATASTRUCTURES_TEST) \
	$(SECURITY_TEST) $(SEMANTIC_TEST) $(TRANSPILE_TEST) $(MEMORY_TEST) \
	$(ABI_TEST) $(ABI_PIPELINE_TEST) $(CONCURRENCY_TEST) $(DIR_TEST) \
	$(AIR_TEST) $(RIR_TEST) $(MIR_TEST) $(HIR_TEST)

all-with-tests: $(PGY) $(PGY_LSP) $(FRONTEND_TEST_BINARIES)

ifeq ($(EXEEXT),.exe)
pgy: $(PGY) $(REPO_BIN_DIR)/pgy$(EXEEXT)
else
pgy: $(PGY) $(REPO_BIN_DIR)/pgy$(EXEEXT) $(REPO_BIN_DIR)/pgy.exe
endif
llvm:
	$(MAKE) LLVM_ENABLED=1 all

# Generate the LLVM runtime bitcode that the --backend=llvm runtime-inliner
# consumes (see PGY_RUNTIME_LIB_BC). Requires clang matching the linked libLLVM;
# the artifact is machine-local and gitignored (it bakes absolute paths), so
# regenerate it after a runtime change. A missing .bc is a silent no-op, so this
# target is optional -- it only unlocks the LLVM runtime-inlining speedup.
runtime-bc:
	"$(BASH)" scripts/build_runtime_bc.sh

# -----------------------------------------------------------------
# Build rules
# -----------------------------------------------------------------

ifneq ($(filter 3.%,$(MAKE_VERSION)),)
define pgy_link
@printf '%s\n' $^ > "$(BUILD_DIR)/$(notdir $@).rsp"
$(CC) $(CFLAGS) -o $@ @$(BUILD_DIR)/$(notdir $@).rsp $(1)
@rm -f "$(BUILD_DIR)/$(notdir $@).rsp"
endef
else
define pgy_link
$(file >$(BUILD_DIR)/$(notdir $@).rsp,$^)
$(CC) $(CFLAGS) -o $@ @$(BUILD_DIR)/$(notdir $@).rsp $(1)
@rm -f "$(BUILD_DIR)/$(notdir $@).rsp"
endef
endif

# pgy compiler driver
$(PGY): $(FRONTEND_OBJECTS) $(DRIVER_OBJ) | $(BIN_DIR)
	@$(call pgy_mkdir_p,$(dir $@))
	$(call pgy_link,$(LDFLAGS_LLVM) $(THREAD_LINK_LIB) -lm)

$(REPO_BIN_DIR)/pgy$(EXEEXT): $(PGY) | $(REPO_BIN_DIR)
	@if [ "$(abspath $<)" != "$(abspath $@)" ]; then cp -f "$<" "$@"; fi

ifneq ($(EXEEXT),.exe)
# Always keep bin/pgy.exe in sync with the canonical $(PGY) build, even on
# hosts where EXEEXT is empty (e.g. WSL building for Windows-side workflows).
# Without this, PowerShell-launched commands silently use a stale .exe and the
# semantic/codegen behavior diverges from the freshly built binary. Listed as
# dev pain point #1 in memory: project_dev_pain_points.md.
$(REPO_BIN_DIR)/pgy.exe: $(PGY) | $(REPO_BIN_DIR)
	@if [ "$(abspath $<)" != "$(abspath $@)" ]; then cp -f "$<" "$@"; fi
endif

# Lexer smoke-test (original main.c)
$(LEXER_TEST): $(LEXER_OBJECTS) $(MAIN_OBJECT) | $(BIN_DIR)
	@$(call pgy_mkdir_p,$(dir $@))
	$(call pgy_link,)

# Parser test
$(PARSER_TEST): $(COMMON_OBJECTS) $(LEXER_OBJECTS) $(PARSER_OBJECTS) $(PARSER_TEST_OBJECT) | $(BIN_DIR)
	@$(call pgy_mkdir_p,$(dir $@))
	$(call pgy_link,)

# Data structures test
$(DATASTRUCTURES_TEST): $(BUILD_DIR)/runtime/slot_pool.o \
                         $(BUILD_DIR)/runtime/slot_pool_linked_list.o \
                         $(BUILD_DIR)/runtime/slot_pool_perf.o \
                         $(TEST_DATASTRUCTURES_OBJ) | $(BIN_DIR)
	@$(call pgy_mkdir_p,$(dir $@))
	$(call pgy_link,$(THREAD_LINK_LIB))

# Security test
$(SECURITY_TEST): $(RUNTIME_OBJECTS) $(RUNTIME_ASM_OBJECTS) \
                   $(TEST_SECURITY_OBJ) | check-security-toolchain $(BIN_DIR)
	@$(call pgy_mkdir_p,$(dir $@))
ifeq ($(EXEEXT),.exe)
	$(call pgy_link,$(THREAD_LINK_LIB) -lbcrypt -ladvapi32 -liphlpapi)
else
	$(call pgy_link,$(THREAD_LINK_LIB) -lssl -lcrypto)
endif

# Semantic analyzer test
$(SEMANTIC_TEST): $(FRONTEND_OBJECTS) $(TEST_SEMANTIC_OBJ) | $(BIN_DIR)
	@$(call pgy_mkdir_p,$(dir $@))
	$(call pgy_link,$(LDFLAGS_LLVM) $(THREAD_LINK_LIB) -lm)

# C backend test
$(TRANSPILE_TEST): $(FRONTEND_OBJECTS) $(TEST_TRANSPILE_OBJ) | $(BIN_DIR)
	@$(call pgy_mkdir_p,$(dir $@))
	$(call pgy_link,$(LDFLAGS_LLVM) $(THREAD_LINK_LIB) -lm)

# Memory layout test (runtime-only, no frontend)
$(MEMORY_TEST): $(TEST_MEMORY_OBJ) | $(BIN_DIR)
	@$(call pgy_mkdir_p,$(dir $@))
	$(call pgy_link,)

# ABI spec validation test (runtime-only, includes pgy_runtime.h for cross-check)
$(ABI_TEST): $(TEST_ABI_OBJ) | $(BIN_DIR)
	@$(call pgy_mkdir_p,$(dir $@))
	$(call pgy_link,)

# ABI pipeline integration test (frontend + backend + produced binary)
$(ABI_PIPELINE_TEST): $(FRONTEND_OBJECTS) $(TEST_ABI_PIPELINE_OBJ) | $(BIN_DIR)
	@$(call pgy_mkdir_p,$(dir $@))
	$(call pgy_link,$(LDFLAGS_LLVM) $(THREAD_LINK_LIB) -lm)

# Concurrency runtime test
$(CONCURRENCY_TEST): $(TEST_CONCURRENCY_OBJ) | $(BIN_DIR)
	@$(call pgy_mkdir_p,$(dir $@))
	$(call pgy_link,$(THREAD_LINK_LIB))

# DIR lowering test
$(DIR_TEST): $(COMMON_OBJECTS) $(LEXER_OBJECTS) $(PARSER_OBJECTS) $(SEMANTIC_OBJECTS) $(SEMANTIC_LINK_SUPPORT) $(DIR_CORE_OBJECTS) $(TEST_DIR_OBJ) | $(BIN_DIR)
	@$(call pgy_mkdir_p,$(dir $@))
	$(call pgy_link,$(THREAD_LINK_LIB))

# AIR synthesis and drift test
$(AIR_TEST): $(COMMON_OBJECTS) $(LEXER_OBJECTS) $(PARSER_OBJECTS) $(SEMANTIC_OBJECTS) $(SEMANTIC_LINK_SUPPORT) $(DIR_CORE_OBJECTS) $(HIR_CORE_OBJECTS) $(RIR_CORE_OBJECTS) $(AIR_CORE_OBJECTS) $(TEST_AIR_OBJ) | $(BIN_DIR)
	@$(call pgy_mkdir_p,$(dir $@))
	$(call pgy_link,$(THREAD_LINK_LIB))

# RIR lowering test
$(RIR_TEST): $(COMMON_OBJECTS) $(LEXER_OBJECTS) $(PARSER_OBJECTS) $(SEMANTIC_OBJECTS) $(SEMANTIC_LINK_SUPPORT) $(DIR_CORE_OBJECTS) $(HIR_CORE_OBJECTS) $(RIR_CORE_OBJECTS) $(TEST_RIR_OBJ) | $(BIN_DIR)
	@$(call pgy_mkdir_p,$(dir $@))
	$(call pgy_link,$(THREAD_LINK_LIB))

# MIR lowering test
$(MIR_TEST): $(COMMON_OBJECTS) $(LEXER_OBJECTS) $(PARSER_OBJECTS) $(SEMANTIC_OBJECTS) $(SEMANTIC_LINK_SUPPORT) $(HIR_CORE_OBJECTS) $(RIR_CORE_OBJECTS) $(MIR_CORE_OBJECTS) $(TEST_MIR_OBJ) | $(BIN_DIR)
	@$(call pgy_mkdir_p,$(dir $@))
	$(call pgy_link,$(THREAD_LINK_LIB))

# HIR lowering test
$(HIR_TEST): $(COMMON_OBJECTS) $(LEXER_OBJECTS) $(PARSER_OBJECTS) $(SEMANTIC_OBJECTS) $(SEMANTIC_LINK_SUPPORT) $(HIR_CORE_OBJECTS) $(TEST_HIR_OBJ) | $(BIN_DIR)
	@$(call pgy_mkdir_p,$(dir $@))
	$(call pgy_link,$(THREAD_LINK_LIB))

# LSP server
$(PGY_LSP): $(COMMON_OBJECTS) $(LEXER_OBJECTS) $(PARSER_OBJECTS) $(SEMANTIC_OBJECTS) $(SEMANTIC_LINK_SUPPORT) $(DIR_CORE_OBJECTS) $(HIR_CORE_OBJECTS) $(RIR_CORE_OBJECTS) $(AIR_CORE_OBJECTS) $(LSP_OBJECTS) | $(BIN_DIR)
	@$(call pgy_mkdir_p,$(dir $@))
	$(call pgy_link,$(THREAD_LINK_LIB))

$(REPO_BIN_DIR)/pgy-lsp$(EXEEXT): $(PGY_LSP) | $(REPO_BIN_DIR)
	@if [ "$(abspath $<)" != "$(abspath $@)" ]; then cp -f "$<" "$@"; fi

# -----------------------------------------------------------------
# Compilation rules
# -----------------------------------------------------------------

# C sources
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(CONFIG_STAMP) | $(BUILD_DIR)
	@$(call pgy_mkdir_p,$(dir $@))
	$(CC) $(CFLAGS) $(DEPFLAGS) -MF $(@:.o=.d) -c -o $@ $<
	@-sed -i -E 's#[A-Za-z]:/$(notdir $(PROJECT_ROOT))/##g; s#([A-Za-z]):/#\1\\:/#g' $(@:.o=.d) 2>/dev/null || true

# Assembly sources
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s | $(BUILD_DIR)
	@$(call pgy_mkdir_p,$(dir $@))
	$(NASM) $(ASMFLAGS) -o $@ $<

# -----------------------------------------------------------------
# Directory creation
# -----------------------------------------------------------------
$(BUILD_DIR):
	$(call pgy_mkdir_p,$(BUILD_DIR) \
		$(BUILD_DIR)/common \
		$(BUILD_DIR)/lexer \
		$(BUILD_DIR)/parser \
		$(BUILD_DIR)/semantic \
		$(BUILD_DIR)/codegen \
		$(BUILD_DIR)/compiler \
		$(BUILD_DIR)/runtime \
		$(BUILD_DIR)/runtime/async \
		$(BUILD_DIR)/lsp)
	$(call pgy_touch_ref,Makefile,$(BUILD_DIR))

$(CONFIG_STAMP): | $(BUILD_DIR)
	rm -f $(BUILD_DIR)/.config_llvm_*.stamp
	rm -f $(BUILD_ARTIFACT_GLOBS)
	printf "LLVM_ENABLED=%s\nCC=%s\nCC_MACHINE=%s\n" \
		"$(LLVM_ENABLED)" "$(CC)" "$(CC_MACHINE)" > $@

$(BIN_DIR):
	$(call pgy_mkdir_p,$(BIN_DIR))
	$(call pgy_touch_ref,Makefile,$(BIN_DIR))

$(REPO_BIN_DIR):
	$(call pgy_mkdir_p,$(REPO_BIN_DIR))

# -----------------------------------------------------------------
# Test execution targets
# -----------------------------------------------------------------
test: $(LEXER_TEST)
	@echo "=== Lexer Test ==="
	$(call pgy_run_native,$(LEXER_TEST))

test-parser: $(PARSER_TEST)
	@echo "=== Parser Test ==="
	$(call pgy_run_native,$(PARSER_TEST))

test-datastructures: $(DATASTRUCTURES_TEST)
	@echo "=== Data Structures Test ==="
	$(call pgy_run_native,$(DATASTRUCTURES_TEST))

test-security: $(SECURITY_TEST)
	@echo "=== Security Test ==="
	$(call pgy_run_native,$(SECURITY_TEST))

test-semantic: $(SEMANTIC_TEST)
	@echo "=== Semantic Analyzer Test ==="
	$(call pgy_run_native,$(SEMANTIC_TEST))

test-transpile: $(TRANSPILE_TEST)
	@echo "=== C Backend Test ==="
	@$(call pgy_mkdir_p,$(abspath $(BUILD_DIR)/tmp))
	PGY_TEST_TMPDIR="$(abspath $(BUILD_DIR)/tmp)" $(call pgy_run_native,$(TRANSPILE_TEST))

test-memory: $(MEMORY_TEST)
	@echo "=== Memory Layout Test ==="
	$(call pgy_run_native,$(MEMORY_TEST))

test-lifecycle:
	@echo "=== Domain-Lifecycle Engine Unit Test ==="
	$(CC) -std=c11 -Wall -Wextra \
	    $(SEMANTIC_DIR)/lifecycle_state.c \
	    $(SEMANTIC_DIR)/test_lifecycle_state.c \
	    -o $(BUILD_DIR)/test_lifecycle$(EXEEXT)
	$(call pgy_run_native,$(BUILD_DIR)/test_lifecycle$(EXEEXT))

# Secure-slot scope safety regression (double-release, leak, capacity growth).
test-slot-scope: $(RUNTIME_OBJECTS) $(RUNTIME_ASM_OBJECTS)
	@echo "=== Secure Slot Scope Safety Test ==="
	$(CC) -std=c11 -Wall -I$(SRC_DIR) -I$(RUNTIME_DIR) \
	    $(RUNTIME_DIR)/test_slot_scope_safety.c \
	    $(RUNTIME_OBJECTS) $(RUNTIME_ASM_OBJECTS) \
	    -o $(BUILD_DIR)/test_slot_scope$(EXEEXT) \
	    $(if $(filter .exe,$(EXEEXT)),$(THREAD_LINK_LIB) -lbcrypt -ladvapi32 -liphlpapi,$(THREAD_LINK_LIB) -lssl -lcrypto)
	$(call pgy_run_native,$(BUILD_DIR)/test_slot_scope$(EXEEXT))

# Content capability gate regression (manifest grant + fail-closed denial).
test-capability: $(RUNTIME_OBJECTS) $(RUNTIME_ASM_OBJECTS)
	@echo "=== Capability Gate Test ==="
	$(CC) -std=c11 -Wall -I$(SRC_DIR) -I$(RUNTIME_DIR) \
	    $(RUNTIME_DIR)/test_capability_gate.c \
	    $(RUNTIME_OBJECTS) $(RUNTIME_ASM_OBJECTS) \
	    -o $(BUILD_DIR)/test_capability$(EXEEXT) \
	    $(if $(filter .exe,$(EXEEXT)),$(THREAD_LINK_LIB) -lbcrypt -ladvapi32 -liphlpapi,$(THREAD_LINK_LIB) -lssl -lcrypto)
	$(call pgy_run_native,$(BUILD_DIR)/test_capability$(EXEEXT))

# Resource budget gate (the quantitative sandbox axis): a per-kind budget the
# loader imposes; metered ops charge their kind and panic fail-closed on overrun.
test-budget: $(RUNTIME_OBJECTS) $(RUNTIME_ASM_OBJECTS)
	@echo "=== Resource Budget Gate Test ==="
	$(CC) -std=c11 -Wall -I$(SRC_DIR) -I$(RUNTIME_DIR) \
	    $(RUNTIME_DIR)/test_budget_gate.c \
	    $(RUNTIME_OBJECTS) $(RUNTIME_ASM_OBJECTS) \
	    -o $(BUILD_DIR)/test_budget$(EXEEXT) \
	    $(if $(filter .exe,$(EXEEXT)),$(THREAD_LINK_LIB) -lbcrypt -ladvapi32 -liphlpapi,$(THREAD_LINK_LIB) -lssl -lcrypto)
	$(call pgy_run_native,$(BUILD_DIR)/test_budget$(EXEEXT))

# Static capability manifest gate (items 1 + 2): the program's used-capability
# set is computed and emitted (--capability-manifest), and any function that
# uses a capability whose effect family it did not declare fails the gate.
test-capability-manifest: $(PGY)
	@echo "=== Capability Manifest Gate ==="
	PGY_BIN="$(abspath $(PGY))" $(BASH) tests/capability/run_manifest.sh

# Runtime sandbox-enforcement gate (the dynamic half): the host imposes a gate
# out-of-band (PGY_CAP_GRANT / PGY_BUDGET_ALLOC_BYTES) and the program fail-closes
# at run time IDENTICALLY on the C and LLVM backends. Regression guard for the
# single-instance gate-state fix (multi-instance g_pgy_budget/g_pgy_cap_granted
# silently disabled the LLVM gate until unified). LLVM cases self-skip if the
# backend cannot launch, so the C path is always covered.
test-capability-runtime: $(PGY)
	@echo "=== Sandbox Runtime-Enforcement Gate (C/LLVM parity) ==="
	PGY_BIN="$(abspath $(PGY))" $(BASH) tests/capability/run_runtime_enforce.sh

# Machine-neutral falsification marker (docs/semantics/18 Acceptance Rule,
# first exercise). The capability-machine projection consumes AIR-only facts:
# effect inventory, per-op capability masks, slot identity, and authority
# contract requirements. NOT in test-all -- it remains a progress marker until
# the projection is promoted, but local RED must still fail this target.
machine-neutral-status: $(PGY)
	@echo "=== Machine-Neutral Projection Status (AIR-only fact projection marker) ==="
	@{ command -v python3 >/dev/null 2>&1 && py=python3 || py=python; } ; \
	if command -v "$$py" >/dev/null 2>&1; then \
		"$$py" tests/machine_neutral/capability_projection_gate.py --pgy "$(abspath $(PGY))"; \
	else \
		PGY_BIN="$(abspath $(PGY))" $(BASH) tests/machine_neutral/capability_projection_shell_gate.sh; \
	fi

# Source-level SoT guard: the budget charge for a metered kind must appear in
# BOTH the C-inline and LLVM-export twin of each allocation path, or the backends
# diverge in enforcement (the silent C/LLVM gap class). Pure textual -- no
# compiler -- so it is always load-bearing, even where the runtime fixtures skip.
test-budget-twin-parity:
	@echo "=== Budget-Charge Twin Parity (source SoT) ==="
	$(BASH) tests/capability/run_budget_twin_parity.sh

# Channel runtime twin op-set SoT guard (target #4 dual-backend unification):
# every op the C-inline channel twin exposes must have a matching LLVM export, or
# a program using that op compiles on C but diverges/fails on LLVM. Catches
# op-set drift the behavioral compare_backends fixtures cannot reach. Pure
# textual -- always load-bearing.
test-channel-twin-parity:
	@echo "=== Channel Twin Op-Set Parity (source SoT) ==="
	$(BASH) tests/capability/run_channel_twin_op_parity.sh

# Single entry point for the whole content-sandbox gate family (capability +
# resource budget, the qualitative + quantitative sandbox axes, external
# red-team R6). Runs the C unit gates, the source-level twin-parity SoT guards,
# and both the static (declared>=used) and dynamic (runtime fail-close, C/LLVM
# parity) enforcement gates. Wire this into CI to keep the sandbox enforcement
# continuously protected.
test-sandbox-gates: test-capability test-budget test-budget-twin-parity test-channel-twin-parity test-capability-manifest test-capability-runtime
	@echo "=== Sandbox gate family (capability + budget) PASS ==="

test-abi: $(ABI_TEST) $(PGY)
	@echo "=== ABI Spec Validation ==="
	$(call pgy_run_native,$(ABI_TEST))
	@echo "=== ABI Pipeline Smoke ==="
	PGY_BIN="$(abspath $(PGY))" \
	PGY_ABI_PIPELINE_BACKENDS="$(if $(filter 1,$(LLVM_ENABLED)),c llvm,c)" \
	$(BASH) tests/abi_pipeline_smoke.sh

$(ABI_PERF_RUNTIME_RELEASE_OBS0): $(RUNTIME_DIR)/pgy_runtime_lib.c $(RUNTIME_DIR)/pgy_runtime.h
	@$(call pgy_mkdir_p,$(dir $@))
	$(CC) $(CFLAGS) -O3 -DPGY_LLVM_ENABLED -DPGY_INTENT_OBSERVABILITY_ENABLED=0 -c -o $@ $<

$(ABI_PERF_RUNTIME_RELEASE_OBS1): $(RUNTIME_DIR)/pgy_runtime_lib.c $(RUNTIME_DIR)/pgy_runtime.h
	@$(call pgy_mkdir_p,$(dir $@))
	$(CC) $(CFLAGS) -O3 -DPGY_LLVM_ENABLED -DPGY_INTENT_OBSERVABILITY_ENABLED=1 -c -o $@ $<

abi-perf-runtime: $(ABI_PERF_RUNTIME_RELEASE_OBS0) $(ABI_PERF_RUNTIME_RELEASE_OBS1)

test-abi-perf: $(ABI_PIPELINE_TEST) abi-perf-runtime
	@echo "=== ABI Pipeline Benchmark ==="
	$(if $(ABI_PERF_LINKER_ENV),$(ABI_PERF_LINKER_ENV) )PGY_PREBUILT_RUNTIME_OBJ_RELEASE_OBS0="$(ABI_PERF_RUNTIME_RELEASE_OBS0)" \
	PGY_PREBUILT_RUNTIME_OBJ_RELEASE_OBS1="$(ABI_PERF_RUNTIME_RELEASE_OBS1)" \
	PGY_ABI_PERF_MODE=1 $(call pgy_run_native,"$(ABI_PIPELINE_TEST)")

perf-summary:
	@test -n "$(PERF_LOG)" || { echo "usage: make perf-summary PERF_LOG=/path/to/test-abi-perf.log" >&2; exit 1; }
	$(BASH) tests/perf_summary.sh "$(PERF_LOG)"

perf-contract-test-smoke:
	"$(BASH)" tests/perf_contract_smoke.sh

backend-fail-closed-test-smoke:
	"$(BASH)" tests/backend_fail_closed_smoke.sh

worker-boundary-ub-test-smoke:
	"$(BASH)" tests/worker_boundary_ub_smoke.sh

perf-c-baseline-test-smoke: $(PGY)
	PGY_BIN="$(PGY)" CC="$(CC)" "$(BASH)" tests/perf_c_baseline_smoke.sh

evidence-guard-amortization-test-smoke: $(PGY)
	PGY_BIN="$(PGY)" CC="$(CC)" "$(BASH)" tests/evidence_guard_amortization_smoke.sh

# ============================================================
# Compile-Speed Timing Baseline (docs/126 §7)
#
# Walks the regression suite and records per-phase wall-clock
# into review/timing_baseline_YYYYMMDD.log so future PRs can
# diff against a baseline. Not for release-time perf claims;
# this is engineering-discipline measurement only.
# ============================================================
timing: $(PGY)
	@echo "=== Compile-speed timing baseline (docs/126 §7) ==="
	@mkdir -p review
	@TIMING_LOG="review/timing_baseline_$$(date +%Y%m%d).log"; \
	echo "# Pergyra compile-speed timing baseline" > $$TIMING_LOG; \
	echo "# date: $$(date -Iseconds)" >> $$TIMING_LOG; \
	echo "# cc:   $$($(CC) --version | head -n1)" >> $$TIMING_LOG; \
	echo "# host: $$(uname -srm)" >> $$TIMING_LOG; \
	echo "" >> $$TIMING_LOG; \
	for ex in examples/basic.pgy examples/battle_sim.pgy examples/async_demo.pgy; do \
	  if [ -f "$$ex" ]; then \
	    echo "## $$ex" >> $$TIMING_LOG; \
	    /usr/bin/time -f "wall_seconds=%e user_seconds=%U sys_seconds=%S max_rss_kb=%M" \
	      $(PGY) "$$ex" --emit-llvm -o /tmp/pgy_timing_out 2>>$$TIMING_LOG || \
	      $(PGY) "$$ex" --emit-llvm -o /tmp/pgy_timing_out 2>&1 | head -n3 >> $$TIMING_LOG; \
	    echo "" >> $$TIMING_LOG; \
	  fi; \
	done; \
	echo "[timing] baseline written to $$TIMING_LOG"

test-concurrency: $(CONCURRENCY_TEST)
	@echo "=== Concurrency Test ==="
	$(call pgy_run_native,$(CONCURRENCY_TEST))

test-dir: $(DIR_TEST)
	@echo "=== DIR Test ==="
	$(call pgy_run_native,$(DIR_TEST))

test-air: $(AIR_TEST)
	@echo "=== AIR Test ==="
	$(call pgy_run_native,$(AIR_TEST))

test-rir: $(RIR_TEST)
	@echo "=== RIR Test ==="
	$(call pgy_run_native,$(RIR_TEST))

test-mir: $(MIR_TEST)
	@echo "=== MIR Test ==="
	$(call pgy_run_native,$(MIR_TEST))

test-hir: $(HIR_TEST)
	@echo "=== HIR Test ==="
	$(call pgy_run_native,$(HIR_TEST))

windows-build-smoke: all-with-tests
	@echo "=== Windows Build Smoke ==="
	@echo "built compiler/LSP and explicit frontend/runtime test binaries with CC=$(CC)"

test-all:
	$(MAKE) test
	$(MAKE) test-parser
	$(MAKE) test-datastructures
	$(MAKE) test-semantic
	$(MAKE) test-transpile
	$(MAKE) test-memory
	$(MAKE) test-abi
	$(MAKE) test-concurrency
	$(MAKE) test-dir
	$(MAKE) test-air
	$(MAKE) test-rir
	$(MAKE) test-mir
	$(MAKE) test-hir
	@echo "=== All Frontend Tests Completed ==="

llvm-test:
	$(MAKE) LLVM_ENABLED=1 test

llvm-test-parser:
	$(MAKE) LLVM_ENABLED=1 test-parser

llvm-test-semantic:
	$(MAKE) LLVM_ENABLED=1 test-semantic

llvm-test-transpile:
	$(MAKE) LLVM_ENABLED=1 test-transpile

llvm-test-memory:
	$(MAKE) LLVM_ENABLED=1 test-memory

llvm-test-concurrency:
	$(MAKE) LLVM_ENABLED=1 test-concurrency

llvm-test-rir:
	$(MAKE) LLVM_ENABLED=1 test-rir

llvm-test-mir:
	$(MAKE) LLVM_ENABLED=1 test-mir

llvm-test-dir:
	$(MAKE) LLVM_ENABLED=1 test-dir

llvm-test-hir:
	$(MAKE) LLVM_ENABLED=1 test-hir

llvm-test-smoke:
	$(MAKE) LLVM_ENABLED=1 $(PGY)
	PGY_BIN="$(abspath $(PGY))" PGY_CC="$(CC)" "$(BASH)" tests/llvm_smoke.sh

llvm-runtime-aggregate-return-abi-test-smoke:
	$(MAKE) LLVM_ENABLED=1 $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/llvm_runtime_aggregate_return_abi_smoke.sh

llvm-test-abi-same-process: $(ABI_PIPELINE_TEST)
	@echo "=== ABI Pipeline Same-Process LLVM Regression ==="
	"$(BASH)" tests/abi_pipeline_same_process_smoke.sh "$(ABI_PIPELINE_TEST)"

fmt-test-smoke:
	$(MAKE) $(PGY)
	PGY_BIN="$(abspath $(PGY))" PGY_CC="$(CC)" "$(BASH)" tests/fmt_smoke.sh

tooling-conformance-test-smoke: $(PGY) $(PGY_LSP)
	PGY_BIN="$(abspath $(PGY))" PGY_LSP_BIN="$(abspath $(PGY_LSP))" PGY_CC="$(CC)" "$(BASH)" tests/tooling_conformance_smoke.sh

stdlib-test-smoke:
	$(MAKE) $(PGY)
	PGY_STDLIB_BACKENDS="$${PGY_STDLIB_BACKENDS:-$(STDLIB_BACKENDS)}" \
	PGY_BIN="$(abspath $(PGY))" PGY_CC="$(CC)" "$(BASH)" tests/stdlib_surface_smoke.sh

stage4-determinism-test-smoke:
	$(MAKE) $(PGY)
	PGY_STAGE4_DETERMINISM_BACKENDS="$${PGY_STAGE4_DETERMINISM_BACKENDS:-$(STAGE4_DETERMINISM_BACKENDS)}" \
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/stage4_determinism_smoke.sh

filesystem-directory-walk-test-smoke:
	$(MAKE) $(PGY)
	PGY_FILESYSTEM_WALK_BACKENDS="$${PGY_FILESYSTEM_WALK_BACKENDS:-$(FILESYSTEM_WALK_BACKENDS)}" \
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/filesystem_directory_walk_smoke.sh

module-test-smoke:
	$(MAKE) $(PGY)
	PGY_BIN="$(abspath $(PGY))" PGY_CC="$(CC)" "$(BASH)" tests/module_smoke.sh

ir-pipeline-test-smoke:
	$(MAKE) $(PGY)
	PGY_BIN="$(abspath $(PGY))" PGY_CC="$(CC)" "$(BASH)" tests/ir_pipeline_probe.sh

cfg-body-dataflow-test-smoke:
	$(MAKE) $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/cfg_body_dataflow_smoke.sh

ast-dispatch-test-smoke:
	"$(BASH)" tests/ast_dispatch_partition_smoke.sh

mir-declaration-inventory-test-smoke:
	"$(BASH)" tests/mir_declaration_inventory_smoke.sh

module-taxonomy-test-smoke:
	"$(BASH)" tests/module_taxonomy_smoke.sh

package-module-resolver-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/package_module_resolver_smoke.sh

unicode-policy-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/unicode_policy_smoke.sh

beta-test-suite-freeze-test-smoke:
	"$(BASH)" tests/beta_test_suite_freeze_smoke.sh

build-source-inventory-test-smoke:
	"$(BASH)" tests/build_source_inventory_smoke.sh

ci-step-runner-test-smoke:
	"$(BASH)" tests/ci_step_runner_smoke.sh

__pgy_build_source_inventory_print:
	$(foreach src,$(BUILD_SOURCE_INVENTORY_SOURCES),$(info $(src)))
	$(foreach src,$(BUILD_CONTRACT_INVENTORY_FILES),$(info $(src)))
	@:

source-utf8-test-smoke:
	"$(BASH)" tests/source_utf8_smoke.sh

observability-schema-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/observability_schema_smoke.sh

memory-concurrency-model-test-smoke: $(PGY)
	PGY_MEMORY_CONCURRENCY_BACKENDS="$${PGY_MEMORY_CONCURRENCY_BACKENDS:-$(MEMORY_CONCURRENCY_BACKENDS)}" \
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/memory_concurrency_model_smoke.sh

async-model-positioning-test-smoke:
	"$(BASH)" tests/async_model_positioning_smoke.sh

documentation-quality-test-smoke:
	"$(BASH)" tests/documentation_quality_smoke.sh

backend-wasm-pointer-closure-test-smoke:
	"$(BASH)" tests/backend_wasm_pointer_closure_smoke.sh

language-surface-hygiene-test-smoke:
	"$(BASH)" tests/language_surface_hygiene_smoke.sh

# Canonical grammar cheat-sheet contract: pins docs/grammar/00_cheatsheet.md's
# load-bearing rules and enforces the semicolon REGISTER in authored examples
# (zone/world/effect/relation world-layer facts carry no `;`). Text-only, no
# compiler needed -- always load-bearing in CI.
grammar-cheatsheet-contract-test-smoke:
	"$(BASH)" tests/grammar_cheatsheet_contract_smoke.sh

language-contract-golden-test-smoke:
	"$(BASH)" tests/language_contract_golden_smoke.sh

verification-methodology-test-smoke:
	"$(BASH)" tests/verification_methodology_smoke.sh

proof-spine-test-smoke:
	"$(BASH)" tests/proof_spine_smoke.sh

self-host-preparation-test-smoke: self-host-preparation-contract-test-smoke self-host-preparation-parity-test-smoke

self-host-preparation-contract-test-smoke: $(PGY)
	"$(BASH)" tests/self_host_preparation_smoke.sh
	"$(BASH)" tests/self_hosted_scaffold_smoke.sh
	"$(BASH)" tests/self_hosted_component_contract_smoke.sh
	"$(BASH)" tests/self_host_substrate_contract_smoke.sh
	"$(BASH)" tests/self_host_hard_contract_smoke.sh
	"$(BASH)" tests/self_host_pergyra_likeness_smoke.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_host_compiler_world_contract_smoke.sh
	PGY_FILESYSTEM_WALK_BACKENDS="$${PGY_FILESYSTEM_WALK_BACKENDS:-$(FILESYSTEM_WALK_BACKENDS)}" \
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/filesystem_directory_walk_smoke.sh

self-host-preparation-parity-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/air_graph_json_validator_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/air_graph_id_uniqueness_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/air_graph_node_count_integrity_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/air_graph_ref_live_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/air_graph_ref_integrity_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/air_graph_reachability_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/ast_read_surface_checker_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/backend_output_comparator_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/backend_output_tri_compare_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/doc_link_checker_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/examples_inventory_checker_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/lexer_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/linter_parity.sh
	PGY_SELFHOST_PARSER_BACKENDS="$${PGY_SELFHOST_PARSER_BACKENDS:-$(SELFHOST_PARSER_BACKENDS)}" \
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/parser_parity.sh
	PGY_SELFHOST_SEMANTIC_BACKENDS="$${PGY_SELFHOST_SEMANTIC_BACKENDS:-$(SELFHOST_SEMANTIC_BACKENDS)}" \
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/semantic_parity.sh
	PGY_SELFHOST_SEMANTIC_BACKENDS="$${PGY_SELFHOST_SEMANTIC_BACKENDS:-$(SELFHOST_SEMANTIC_BACKENDS)}" \
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/selfcheck_sources.sh
	PGY_SELFHOST_CODEGEN_BACKENDS="$${PGY_SELFHOST_CODEGEN_BACKENDS:-$(SELFHOST_CODEGEN_BACKENDS)}" \
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/codegen_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/codegen_bootstrap.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/mir_json_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/module_manifest_resolver_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/production_c_size_checker_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/production_header_size_checker_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/runtime_boundary_checker_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/stable_subset_section_checker_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/stdlib_dispatch_inventory_checker_parity.sh

self-host-runtime-boundary-parity-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/runtime_boundary_checker_parity.sh

self-host-air-graph-consumer-parity-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/air_graph_json_validator_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/air_graph_id_uniqueness_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/air_graph_node_count_integrity_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/air_graph_ref_live_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/air_graph_ref_integrity_parity.sh
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/air_graph_reachability_parity.sh

self-host-diagnostic-catalog-parity-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh

self-host-ast-read-surface-parity-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/ast_read_surface_checker_parity.sh

self-host-semantic-parity-test-smoke: $(PGY)
	PGY_SELFHOST_SEMANTIC_BACKENDS="$${PGY_SELFHOST_SEMANTIC_BACKENDS:-$(SELFHOST_SEMANTIC_BACKENDS)}" \
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/semantic_parity.sh

self-host-semantic-selfcheck-test-smoke: $(PGY)
	PGY_SELFHOST_SEMANTIC_BACKENDS="$${PGY_SELFHOST_SEMANTIC_BACKENDS:-$(SELFHOST_SEMANTIC_BACKENDS)}" \
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/selfcheck_sources.sh

self-host-linter-parity-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/linter_parity.sh

self-host-backend-tri-compare-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/backend_output_tri_compare_parity.sh

self-host-backend-tri-compare-extended-test-smoke: $(PGY)
	PGY_BACKEND_TRI_COMPARE_SUITE=extended PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/backend_output_tri_compare_parity.sh

self-host-lexer-parity-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/lexer_parity.sh

self-host-lex-minimal-parity-test-smoke: self-host-lexer-parity-test-smoke

self-host-parser-parity-test-smoke: $(PGY)
	PGY_SELFHOST_PARSER_BACKENDS="$${PGY_SELFHOST_PARSER_BACKENDS:-$(SELFHOST_PARSER_BACKENDS)}" \
	PGY_BIN="$(abspath $(PGY))" \
	"$(BASH)" tests/self_hosted/parity/parser_parity.sh

self-host-codegen-parity-test-smoke: $(PGY)
	PGY_SELFHOST_CODEGEN_BACKENDS="$${PGY_SELFHOST_CODEGEN_BACKENDS:-$(SELFHOST_CODEGEN_BACKENDS)}" \
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/codegen_parity.sh

self-host-codegen-bootstrap-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/codegen_bootstrap.sh

self-host-mir-json-parity-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/mir_json_parity.sh

self-host-fuzz-backend-generator-parity-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh

fuzz-backend-parity-test-smoke: $(PGY)
	PGY_FUZZ_BACKEND_RUN_ORACLE=1 PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh

fuzz-backend-parity-matrix-test-smoke: $(PGY)
	PGY_FUZZ_BACKEND_RUN_ORACLE=1 PGY_FUZZ_COUNT=12 PGY_FUZZ_SEED=1 PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh
	PGY_FUZZ_BACKEND_RUN_ORACLE=1 PGY_FUZZ_COUNT=12 PGY_FUZZ_SEED=42 PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh
	PGY_FUZZ_BACKEND_RUN_ORACLE=1 PGY_FUZZ_COUNT=12 PGY_FUZZ_SEED=1001 PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh

self-host-component-contract-test-smoke:
	"$(BASH)" tests/self_hosted_component_contract_smoke.sh

self-host-substrate-contract-test-smoke:
	"$(BASH)" tests/self_host_substrate_contract_smoke.sh

self-host-hard-contract-test-smoke:
	"$(BASH)" tests/self_host_hard_contract_smoke.sh

self-host-pergyra-likeness-test-smoke:
	"$(BASH)" tests/self_host_pergyra_likeness_smoke.sh

execution-lane-policy-test-smoke:
	"$(BASH)" tests/execution_lane_policy_smoke.sh

sea-execution-lane-golden-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/sea_execution_lane_golden_smoke.sh

lane-scheduler-test-smoke:
	"$(BASH)" tests/lane_scheduler_smoke.sh

self-host-execution-lane-parity-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_host_execution_lane_parity_smoke.sh

memory-safety-failclosed-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/memory_safety_failclosed_smoke.sh

nested-array-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/nested_array_smoke.sh

checkedarith-failclosed-test-smoke:
	PLATFORM_CFLAGS="$(PLATFORM_CFLAGS)" THREAD_LINK_LIB="$(THREAD_LINK_LIB)" "$(BASH)" tests/checked_arith_failclosed_smoke.sh

generic-nested-failclosed-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/generic_nested_failclosed_smoke.sh

stdlib-inventory-test-smoke:
	"$(BASH)" tests/stdlib_inventory_smoke.sh

selfhost-driver-lsp-wiring-test-smoke:
	"$(BASH)" tests/selfhost_driver_lsp_wiring_smoke.sh

# docs/151 Decision-0 empirical kernels: measured carriage verdicts per
# axis x mode (STATIC / RUNTIME / SILENT-COPY), locked on both backends.
axis-carriage-probe-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/axis_carriage_probe_smoke.sh

# docs/151 matrix-lock: closed decisions (Decision-0/GATE), constructor
# rows, sketch tier, and the G-rung ladder are a contract.
generic-axis-matrix-test-smoke:
	"$(BASH)" tests/generic_axis_matrix_smoke.sh

# docs/151 §8 falsification battery: constraint enforcement, unification/
# unbound diagnostics, default-arg binding, where x G-1 composition —
# measured voices locked on both backends.
generic-falsification-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/generic_falsification_smoke.sh

secure-token-reuse-test-smoke:
	"$(BASH)" tests/secure_token_reuse_failclosed_smoke.sh

site-generator-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/site_generator_smoke.sh

site: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" site/build.sh

self-host-compiler-world-contract-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/self_host_compiler_world_contract_smoke.sh

self-host-compiler-world-perf-probe:
	"$(BASH)" tests/self_host_compiler_world_perf_probe.sh

debug-hygiene-test-smoke:
	"$(BASH)" tests/debug_hygiene_smoke.sh

memory-string-safety-test-smoke:
	"$(BASH)" tests/memory_string_safety_smoke.sh

security-portability-contract-test-smoke:
	"$(BASH)" tests/security_portability_contract_smoke.sh

llvm-campaign-projection-test-smoke:
	$(MAKE) LLVM_ENABLED=1 $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/llvm_campaign_projection_smoke.sh

llvm-dnd-campaign-test-smoke:
	$(MAKE) LLVM_ENABLED=1 $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/llvm_dnd_campaign_smoke.sh

beta-readiness-checklist-test-smoke:
	"$(BASH)" tests/beta_readiness_checklist_smoke.sh

dogfood-webgl-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/dogfood_webgl_smoke.sh

wasm-backend-parity-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/wasm_backend_parity_smoke.sh

formal-semantics-test-smoke:
	"$(BASH)" tests/formal_semantics_smoke.sh
	"$(BASH)" tests/axis_keyword_adequacy_smoke.sh
	"$(BASH)" tests/slot_calculus_adequacy_smoke.sh
	"$(BASH)" tests/ir_minimality_adequacy_smoke.sh
	"$(BASH)" tests/proof_carrying_adequacy_smoke.sh
	"$(BASH)" tests/judgment_diagnostic_adequacy_smoke.sh
	"$(BASH)" tests/ast_to_mir_loss_contract_smoke.sh
	"$(BASH)" tests/loss_contract_adequacy_smoke.sh

abstraction-loss-contract-test-smoke:
	"$(BASH)" tests/abstraction_loss_contract_smoke.sh

ast-to-mir-loss-contract-test-smoke:
	"$(BASH)" tests/ast_to_mir_loss_contract_smoke.sh

air-drift-test-smoke:
	$(MAKE) test-air
	"$(BASH)" tests/air_drift_smoke.sh

air-json-schema-test-smoke:
	$(MAKE) $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/air_json_schema_smoke.sh

proof-carrying-pipeline-test-smoke:
	$(MAKE) $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/proof_carrying_pipeline_smoke.sh

proof-carrying-adequacy-test-smoke:
	"$(BASH)" tests/proof_carrying_adequacy_smoke.sh

air-backend-nonimpact-test-smoke:
	$(MAKE) LLVM_ENABLED="$(LLVM_ENABLED)" $(PGY)
	PGY_BIN="$(abspath $(PGY))" \
	PGY_AIR_NONIMPACT_BACKENDS="$(AIR_NONIMPACT_BACKENDS)" \
	PGY_AIR_NONIMPACT_CASE_LIMIT="$(AIR_NONIMPACT_CASE_LIMIT)" \
	PGY_AIR_NONIMPACT_SHARD_COUNT="$(AIR_NONIMPACT_SHARD_COUNT)" \
	PGY_AIR_NONIMPACT_SHARD_INDEX="$(AIR_NONIMPACT_SHARD_INDEX)" \
	"$(BASH)" tests/air_backend_nonimpact_smoke.sh

air-backend-nonimpact-full-test-smoke:
	$(MAKE) LLVM_ENABLED="$(LLVM_ENABLED)" $(PGY)
	PGY_BIN="$(abspath $(PGY))" \
	PGY_AIR_NONIMPACT_SOURCE=all \
	PGY_AIR_NONIMPACT_BACKENDS="$(AIR_NONIMPACT_BACKENDS)" \
	PGY_AIR_NONIMPACT_CASE_LIMIT="$(AIR_NONIMPACT_CASE_LIMIT)" \
	PGY_AIR_NONIMPACT_SHARD_COUNT="$(AIR_NONIMPACT_SHARD_COUNT)" \
	PGY_AIR_NONIMPACT_SHARD_INDEX="$(AIR_NONIMPACT_SHARD_INDEX)" \
	"$(BASH)" tests/air_backend_nonimpact_smoke.sh

codegen-determinism-test-smoke:
	$(MAKE) LLVM_ENABLED=1 $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/codegen_determinism_smoke.sh

runtime-none-contract-test-smoke:
	PGY_BIN="$(abspath $(PGY))" \
	PGY_RUNTIME_NONE_ALLOW_MISSING_BIN=1 \
	"$(BASH)" tests/runtime_none_contract_smoke.sh

raw-escape-contract-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" \
	"$(BASH)" tests/raw_escape_contract_smoke.sh

semantic-inc-size-test-smoke:
	"$(BASH)" tests/semantic_inc_size_smoke.sh

semantic-tu-size-test-smoke:
	"$(BASH)" tests/semantic_tu_size_smoke.sh

production-header-size-test-smoke: $(PGY)
	"$(BASH)" tests/production_header_size_smoke.sh
	"$(BASH)" tests/self_hosted/parity/production_header_size_checker_parity.sh

# Pergyra-side primary gate: no C-side smoke owns the .c file cap today.
production-c-size-test-smoke: $(PGY)
	"$(BASH)" tests/self_hosted/parity/production_c_size_checker_parity.sh

# Pergyra-side primary gate: examples/ inventory has no C-side smoke today.
examples-inventory-test-smoke: $(PGY)
	"$(BASH)" tests/self_hosted/parity/examples_inventory_checker_parity.sh

backend-inc-size-test-smoke:
	"$(BASH)" tests/backend_inc_size_smoke.sh

test-inc-size-test-smoke:
	"$(BASH)" tests/test_inc_size_smoke.sh

transpile-strict-source-test-smoke:
	"$(BASH)" tests/transpile_strict_source_smoke.sh

source-test-harness-compile-test-smoke:
	"$(BASH)" tests/source_test_harness_compile_smoke.sh

inc-sentinel-test-smoke:
	"$(BASH)" tests/inc_sentinel_smoke.sh

semantic-core-shape-test-smoke:
	"$(BASH)" tests/semantic_core_shape_smoke.sh

type-resolution-dag-test-smoke: $(SEMANTIC_TEST)
	SEMANTIC_TEST_BIN="$(abspath $(SEMANTIC_TEST))" "$(BASH)" tests/type_resolution_dag_smoke.sh

type-resolution-resolver-inventory-test-smoke:
	"$(BASH)" tests/type_resolution_resolver_inventory_smoke.sh

semantic-fixture-isolation-test-smoke: $(SEMANTIC_TEST)
	SEMANTIC_TEST_BIN="$(abspath $(SEMANTIC_TEST))" "$(BASH)" tests/semantic_fixture_isolation_smoke.sh

diagnostic-registry-test-smoke:
	"$(BASH)" tests/diagnostic_registry_smoke.sh

layered-diagnostics-contract-test-smoke:
	"$(BASH)" tests/layered_diagnostics_contract_smoke.sh

intent-compression-contract-test-smoke:
	"$(BASH)" tests/intent_compression_contract_smoke.sh

runtime-authority-contract-test-smoke:
	"$(BASH)" tests/runtime_authority_contract_smoke.sh

runtime-panic-contract-test-smoke:
	"$(BASH)" tests/runtime_panic_contract_smoke.sh

runtime-panic-abi-test-smoke:
	CC="$(CC)" "$(BASH)" tests/runtime_panic_abi_smoke.sh

runtime-panic-codegen-test-smoke: $(PGY)
	PGY_RUNTIME_PANIC_CODEGEN_BACKENDS="$${PGY_RUNTIME_PANIC_CODEGEN_BACKENDS:-$(RUNTIME_PANIC_CODEGEN_BACKENDS)}" \
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/runtime_panic_codegen_smoke.sh

slot-contract-test-smoke: $(PGY)
	PGY_SLOT_CONTRACT_BACKENDS="$${PGY_SLOT_CONTRACT_BACKENDS:-$(SLOT_CONTRACT_BACKENDS)}" \
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/slot_contract_smoke.sh

projection-diagnostic-contract-test-smoke:
	"$(BASH)" tests/projection_diagnostic_contract_smoke.sh

runtime-abi-lifetime-test-smoke:
	"$(BASH)" tests/runtime_abi_lifetime_smoke.sh

abi-ownership-shape-test-smoke:
	"$(BASH)" tests/abi_ownership_shape_smoke.sh

runtime-frontier-contract-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/runtime_frontier_contract_smoke.sh

runtime-frontier-policy-test-smoke:
	CC="$(CC)" "$(BASH)" tests/runtime_frontier_policy_smoke.sh

runtime-intent-observability-contract-test-smoke:
	"$(BASH)" tests/runtime_intent_observability_contract_smoke.sh

parallel-core-contract-test-smoke:
	"$(BASH)" tests/parallel_core_contract_smoke.sh

parser-lexer-diagnostic-test-smoke:
	"$(BASH)" tests/parser_lexer_diagnostic_smoke.sh

diagnostics-json-test-smoke:
	$(MAKE) $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/diagnostics_json_smoke.sh

backend-compare-llvm-coverage-test-smoke:
	"$(BASH)" tests/backend_compare_llvm_coverage_smoke.sh

backend-compare-inventory-test-smoke:
	PGY_BACKEND_COMPARE_INVENTORY_ONLY=1 "$(BASH)" tests/compare_backends.sh

llvm-test-backend-compare: $(if $(filter 0,$(PGY_BACKEND_COMPARE_PRECHECK)),,$(ABI_PIPELINE_TEST))
	$(MAKE) backend-compare-inventory-test-smoke
	$(MAKE) backend-compare-llvm-coverage-test-smoke
	$(MAKE) LLVM_ENABLED=1 $(PGY)
	PGY_BIN="$(abspath $(PGY))" \
	PGY_CC="$(CC)" \
	PGY_ABI_PIPELINE_TEST_BIN="$(abspath $(ABI_PIPELINE_TEST))" \
	LLVM_INSTALL="$(LLVM_INSTALL)" \
	PGY_BACKEND_COMPARE_PRECHECK_SAME_PROCESS="$(PGY_BACKEND_COMPARE_PRECHECK)" \
	PGY_BACKEND_COMPARE_SHARD_TOTAL="$(PGY_BACKEND_COMPARE_SHARD_TOTAL)" \
	PGY_BACKEND_COMPARE_SHARD_INDEX="$(PGY_BACKEND_COMPARE_SHARD_INDEX)" \
	PGY_BACKEND_COMPARE_CASES="$(PGY_BACKEND_COMPARE_CASES)" \
	PGY_BACKEND_COMPARE_START_INDEX="$(PGY_BACKEND_COMPARE_START_INDEX)" \
	PGY_BACKEND_COMPARE_MAX_CASES="$(PGY_BACKEND_COMPARE_MAX_CASES)" \
	"$(BASH)" tests/compare_backends.sh

air-strict-backend-compare-test-smoke: $(if $(filter 0,$(PGY_BACKEND_COMPARE_PRECHECK)),,$(ABI_PIPELINE_TEST))
	$(MAKE) LLVM_ENABLED=1 $(PGY)
	PGY_AIR_STRICT_EVIDENCE=1 \
	PGY_BIN="$(abspath $(PGY))" \
	PGY_CC="$(CC)" \
	PGY_ABI_PIPELINE_TEST_BIN="$(abspath $(ABI_PIPELINE_TEST))" \
	LLVM_INSTALL="$(LLVM_INSTALL)" \
	PGY_BACKEND_COMPARE_PRECHECK_SAME_PROCESS="$(PGY_BACKEND_COMPARE_PRECHECK)" \
	PGY_BACKEND_COMPARE_SHARD_TOTAL="$(PGY_BACKEND_COMPARE_SHARD_TOTAL)" \
	PGY_BACKEND_COMPARE_SHARD_INDEX="$(PGY_BACKEND_COMPARE_SHARD_INDEX)" \
	PGY_BACKEND_COMPARE_CASES="$(PGY_BACKEND_COMPARE_CASES)" \
	PGY_BACKEND_COMPARE_START_INDEX="$(PGY_BACKEND_COMPARE_START_INDEX)" \
	PGY_BACKEND_COMPARE_MAX_CASES="$(PGY_BACKEND_COMPARE_MAX_CASES)" \
	"$(BASH)" tests/compare_backends.sh

example-test-smoke:
	$(MAKE) $(PGY)
	PGY_BIN="$(abspath $(PGY))" PGY_CC="$(CC)" "$(BASH)" tests/example_contract_smoke.sh

string-window-builtins-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/string_window_builtins_smoke.sh

llvm-test-all:
	$(MAKE) LLVM_ENABLED=1 test
	$(MAKE) LLVM_ENABLED=1 test-parser
	$(MAKE) LLVM_ENABLED=1 test-semantic
	$(MAKE) LLVM_ENABLED=1 test-transpile
	$(MAKE) LLVM_ENABLED=1 test-memory
	$(MAKE) LLVM_ENABLED=1 test-concurrency
	$(MAKE) LLVM_ENABLED=1 test-hir
	$(MAKE) LLVM_ENABLED=1 $(PGY) $(ABI_PIPELINE_TEST)
	PGY_BIN="$(abspath $(PGY))" PGY_CC="$(CC)" "$(BASH)" tests/llvm_smoke.sh
	PGY_BIN="$(abspath $(PGY))" \
	PGY_CC="$(CC)" \
	PGY_ABI_PIPELINE_TEST_BIN="$(abspath $(ABI_PIPELINE_TEST))" \
	PGY_BACKEND_COMPARE_PRECHECK_SAME_PROCESS=1 \
	"$(BASH)" tests/compare_backends.sh

check-linux-toolchain:
	@cc_machine="$$( $(CI_LINUX_CC) -dumpmachine 2>/dev/null || true )"; \
	if echo "$$cc_machine" | grep -qi 'mingw'; then \
		echo "ci-linux requires a native Linux toolchain." >&2; \
		echo "current CC: $(CI_LINUX_CC)" >&2; \
		echo "detected target: $${cc_machine:-unknown}" >&2; \
		exit 1; \
	fi

check-macos-toolchain:
	@cc_machine="$$( $(CI_MACOS_CC) -dumpmachine 2>/dev/null || true )"; \
	if echo "$$cc_machine" | grep -qi 'darwin'; then \
		exit 0; \
	fi; \
	echo "ci-macos requires a native macOS/Darwin toolchain." >&2; \
	echo "current CC: $(CI_MACOS_CC)" >&2; \
	echo "detected target: $${cc_machine:-unknown}" >&2; \
	echo "hint: run under GitHub Actions macos-latest or a native macOS shell." >&2; \
	exit 1

check-build-tools:
	@if ! "$(BASH)" -lc 'exit 0' >/dev/null 2>&1; then \
		echo "build preflight requires bash for smoke tests and CI scripts." >&2; \
		echo "current BASH: $(BASH)" >&2; \
		echo "hint: install bash, or run from MSYS2/Git Bash/WSL on Windows." >&2; \
		exit 1; \
	fi
	@if ! $(CC) --version >/dev/null 2>&1; then \
		echo "build preflight requires a working C compiler." >&2; \
		echo "current CC: $(CC)" >&2; \
		echo "hint: install gcc/clang, or set CC=/path/to/compiler." >&2; \
		exit 1; \
	fi
	@if [ "$(LLVM_ENABLED)" != "0" ]; then \
		if [ -n "$(LLVM_CONFIG)" ] && "$(LLVM_CONFIG)" --version >/dev/null 2>&1; then \
			exit 0; \
		fi; \
		if [ -n "$(LLVM_MONOLITHIC_SONAME)" ] && [ -f "$(LLVM_MONOLITHIC_SONAME)" ]; then \
			exit 0; \
		fi; \
		if [ -f "$(LLVM_DIR)/llvm-c/Core.h" ] && [ -d "$(LLVM_INSTALL)/lib" ]; then \
			exit 0; \
		fi; \
		echo "build preflight requires LLVM when LLVM_ENABLED=$(LLVM_ENABLED)." >&2; \
		echo "hint: install llvm-config/libLLVM, or run with LLVM_ENABLED=0 for the C backend." >&2; \
		exit 1; \
	fi
	@if [ "$(ENABLE_ASM_FASTPATH)" != "1" ]; then \
		echo "build preflight: assembly runtime objects are disabled by default; set ENABLE_ASM_FASTPATH=1 to opt in." >&2; \
	elif [ -z "$(NASM)" ]; then \
		echo "build preflight: nasm not found; assembly runtime objects are disabled." >&2; \
	fi

SECURITY_OPENSSL_PREFLIGHT = tests/security_openssl_preflight.c
SECURITY_WINDOWS_PREFLIGHT = tests/security_windows_bcrypt_preflight.c

check-security-toolchain: | $(BUILD_DIR)
ifeq ($(EXEEXT),.exe)
	@echo "security test preflight: checking Windows CNG/BCrypt provider"
	$(CC) $(PLATFORM_CFLAGS) $(SECURITY_WINDOWS_PREFLIGHT) -o $(BUILD_DIR)/pgy_bcrypt_check$(EXEEXT) -lbcrypt
	@$(BASH) -c "rm -f '$(BUILD_DIR)/pgy_bcrypt_check$(EXEEXT)'"
else
	@echo "security test preflight: checking OpenSSL EVP/HMAC/RAND provider"
	$(CC) $(PLATFORM_CFLAGS) $(SECURITY_OPENSSL_PREFLIGHT) -o $(BUILD_DIR)/pgy_openssl_check$(EXEEXT) -lssl -lcrypto
	@$(BASH) -c "rm -f '$(BUILD_DIR)/pgy_openssl_check$(EXEEXT)'"
endif

ci-linux:
	@CI_LINUX_CC="$(CI_LINUX_CC)" \
	 CI_LINUX_BUILD_DIR="$(CI_LINUX_BUILD_DIR)" \
	 CI_LINUX_BIN_DIR="$(CI_LINUX_BIN_DIR)" \
	 CI_BACKEND_COMPARE_SHARD_TOTAL="$(CI_BACKEND_COMPARE_SHARD_TOTAL)" \
	 CI_BACKEND_COMPARE_SHARD_INDEX="$(CI_BACKEND_COMPARE_SHARD_INDEX)" \
	 PGY_CI_NAME=ci-linux \
	 "$(BASH)" scripts/ci_step_runner.sh scripts/ci_linux_steps.sh

ci-macos:
	@CI_MACOS_CC="$(CI_MACOS_CC)" \
	 CI_MACOS_BUILD_DIR="$(CI_MACOS_BUILD_DIR)" \
	 CI_MACOS_BIN_DIR="$(CI_MACOS_BIN_DIR)" \
	 PGY_CI_NAME=ci-macos \
	 "$(BASH)" scripts/ci_step_runner.sh scripts/ci_macos_steps.sh

check-windows-toolchain:
	@cc_machine="$$( $(CI_WINDOWS_CC) -dumpmachine 2>/dev/null || true )"; \
	if [ -n "$(MSYSTEM)" ] && [ "$(PGY_WINDOWS_BASH_IS_MSYS)" != "1" ]; then \
		echo "ci-windows requires MSYS2 bash when MSYSTEM is set." >&2; \
		echo "selected bash: $(BASH)" >&2; \
		echo "hint: run from an MSYS2 shell, not Git Bash with a synthetic MSYSTEM." >&2; \
		exit 1; \
	fi; \
	if [ -n "$(PGY_WINDOWS_BASH)" ] && [ "$(PGY_WINDOWS_BASH_IS_MSYS)" != "1" ] && echo "$$cc_machine" | grep -qi 'mingw'; then \
		echo "ci-windows requires MSYS2 bash for Windows-hosted MinGW builds." >&2; \
		echo "selected bash: $(BASH)" >&2; \
		echo "hint: install/run from MSYS2, or run ci-windows as a cross build from a POSIX host." >&2; \
		exit 1; \
	fi; \
	if [ -n "$${MSYSTEM:-}" ] || echo "$$cc_machine" | grep -qi 'mingw'; then \
		exit 0; \
	fi; \
	echo "ci-windows requires an MSYS2/MinGW toolchain." >&2; \
	echo "current CC: $(CI_WINDOWS_CC)" >&2; \
	echo "detected target: $${cc_machine:-unknown}" >&2; \
	echo "hint: run under GitHub Actions windows-latest with msys2/setup-msys2," >&2; \
	echo "      or use a MinGW cross-compiler such as x86_64-w64-mingw32-gcc." >&2; \
	exit 1

ci-windows:
	@CI_WINDOWS_CC="$(CI_WINDOWS_CC)" \
	 CI_WINDOWS_BUILD_DIR="$(CI_WINDOWS_BUILD_DIR)" \
	 CI_WINDOWS_BIN_DIR="$(CI_WINDOWS_BIN_DIR)" \
	 CI_WINDOWS_RUNNABLE="$(CI_WINDOWS_RUNNABLE)" \
	 CI_WINDOWS_CC_MACHINE="$(CI_WINDOWS_CC_MACHINE)" \
	 WINDOWS_LLVM_READY="$(WINDOWS_LLVM_READY)" \
	 CI_BACKEND_COMPARE_SHARD_TOTAL="$(CI_BACKEND_COMPARE_SHARD_TOTAL)" \
	 CI_BACKEND_COMPARE_SHARD_INDEX="$(CI_BACKEND_COMPARE_SHARD_INDEX)" \
	 PGY_CI_NAME=ci-windows \
	 "$(BASH)" scripts/ci_step_runner.sh scripts/ci_windows_steps.sh

# -----------------------------------------------------------------
# pgy driver convenience targets
# -----------------------------------------------------------------
example-hello: $(PGY)
	"$(PGY)" examples/hello.pgy --run -v

example-slots: $(PGY)
	"$(PGY)" examples/slots.pgy --run -v

# Emit C only
emit-c-%: $(PGY)
	"$(PGY)" examples/$*.pgy --emit-c -v

# Emit LLVM IR only
emit-llvm-%:
	$(MAKE) LLVM_ENABLED=1 $(PGY)
	"$(PGY)" examples/$*.pgy --emit-llvm -o $*.ll -v

# -----------------------------------------------------------------
# Maintenance
# -----------------------------------------------------------------
clean:
	"$(BASH)" -c "rm -rf '$(BUILD_DIR)' '$(BIN_DIR)'"

clean-objects:
	"$(BASH)" -c "rm -f $(BUILD_ARTIFACT_GLOBS)"

# Force a default compiler/LSP rebuild from scratch. Use when source edits
# aren't reflected (stale .o, broken .d, CONFIG_STAMP mismatch, etc).
# See docs/91_build_troubleshooting.md.
rebuild: clean
	$(MAKE) all

debug: CFLAGS += -DDEBUG -g3 -O0
debug: $(PGY)

release: CFLAGS += -DNDEBUG -O3 -flto
release: $(PGY)

analyze:
	cppcheck --enable=all --suppress=missingIncludeSystem $(SRC_DIR)

format:
	find $(SRC_DIR) -name "*.c" -o -name "*.h" | xargs clang-format -i

memcheck: debug
	valgrind --leak-check=full --show-leak-kinds=all ./$(TRANSPILE_TEST)

lsp: $(PGY_LSP)

.PHONY: all all-with-tests clean clean-objects rebuild debug release analyze format memcheck \
        test test-parser test-datastructures test-security test-semantic test-transpile test-memory test-abi test-concurrency test-dir test-air test-rir test-mir test-hir test-all \
llvm-test llvm-test-parser llvm-test-semantic llvm-test-transpile llvm-test-memory llvm-test-concurrency llvm-test-dir llvm-test-rir llvm-test-mir llvm-test-hir backend-compare-inventory-test-smoke backend-compare-llvm-coverage-test-smoke llvm-test-backend-compare llvm-test-all llvm-test-smoke llvm-runtime-aggregate-return-abi-test-smoke tooling-conformance-test-smoke stdlib-test-smoke stage4-determinism-test-smoke filesystem-directory-walk-test-smoke module-test-smoke module-taxonomy-test-smoke package-module-resolver-test-smoke unicode-policy-test-smoke beta-test-suite-freeze-test-smoke build-source-inventory-test-smoke ci-step-runner-test-smoke observability-schema-test-smoke memory-concurrency-model-test-smoke async-model-positioning-test-smoke documentation-quality-test-smoke backend-wasm-pointer-closure-test-smoke language-surface-hygiene-test-smoke grammar-cheatsheet-contract-test-smoke language-contract-golden-test-smoke verification-methodology-test-smoke proof-spine-test-smoke self-host-preparation-test-smoke self-host-preparation-contract-test-smoke self-host-preparation-parity-test-smoke self-host-runtime-boundary-parity-test-smoke self-host-air-graph-consumer-parity-test-smoke self-host-diagnostic-catalog-parity-test-smoke self-host-ast-read-surface-parity-test-smoke self-host-semantic-parity-test-smoke self-host-semantic-selfcheck-test-smoke self-host-linter-parity-test-smoke self-host-backend-tri-compare-test-smoke self-host-backend-tri-compare-extended-test-smoke self-host-lexer-parity-test-smoke self-host-parser-parity-test-smoke self-host-codegen-parity-test-smoke self-host-codegen-bootstrap-test-smoke self-host-mir-json-parity-test-smoke self-host-fuzz-backend-generator-parity-test-smoke fuzz-backend-parity-test-smoke fuzz-backend-parity-matrix-test-smoke self-host-component-contract-test-smoke self-host-substrate-contract-test-smoke self-host-hard-contract-test-smoke self-host-compiler-world-contract-test-smoke self-host-lex-minimal-parity-test-smoke debug-hygiene-test-smoke memory-string-safety-test-smoke security-portability-contract-test-smoke llvm-campaign-projection-test-smoke llvm-dnd-campaign-test-smoke beta-readiness-checklist-test-smoke dogfood-webgl-test-smoke wasm-backend-parity-test-smoke formal-semantics-test-smoke proof-carrying-pipeline-test-smoke proof-carrying-adequacy-test-smoke abstraction-loss-contract-test-smoke ast-to-mir-loss-contract-test-smoke air-drift-test-smoke air-json-schema-test-smoke air-backend-nonimpact-test-smoke air-backend-nonimpact-full-test-smoke air-strict-backend-compare-test-smoke codegen-determinism-test-smoke runtime-none-contract-test-smoke raw-escape-contract-test-smoke semantic-inc-size-test-smoke semantic-tu-size-test-smoke production-header-size-test-smoke production-c-size-test-smoke examples-inventory-test-smoke backend-inc-size-test-smoke test-inc-size-test-smoke transpile-strict-source-test-smoke source-test-harness-compile-test-smoke semantic-core-shape-test-smoke type-resolution-dag-test-smoke type-resolution-resolver-inventory-test-smoke semantic-fixture-isolation-test-smoke diagnostic-registry-test-smoke layered-diagnostics-contract-test-smoke intent-compression-contract-test-smoke runtime-authority-contract-test-smoke runtime-panic-contract-test-smoke runtime-panic-abi-test-smoke runtime-panic-codegen-test-smoke slot-contract-test-smoke projection-diagnostic-contract-test-smoke runtime-abi-lifetime-test-smoke abi-ownership-shape-test-smoke runtime-frontier-contract-test-smoke runtime-frontier-policy-test-smoke runtime-intent-observability-contract-test-smoke parallel-core-contract-test-smoke perf-contract-test-smoke backend-fail-closed-test-smoke worker-boundary-ub-test-smoke perf-c-baseline-test-smoke evidence-guard-amortization-test-smoke parser-lexer-diagnostic-test-smoke diagnostics-json-test-smoke cfg-body-dataflow-test-smoke mir-declaration-inventory-test-smoke example-test-smoke ast-dispatch-test-smoke ci-linux ci-macos ci-windows check-build-tools check-security-toolchain check-linux-toolchain check-macos-toolchain check-windows-toolchain \
        example-hello example-slots llvm emit-llvm-% lsp

ifeq ($(filter clean clean-objects,$(MAKECMDGOALS)),)
-include $(ALL_DEP_FILES)
endif
