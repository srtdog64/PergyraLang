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

CC      = gcc
CC_DUMP_MACHINE := $(shell $(CC) -dumpmachine 2>/dev/null || echo unknown)
CC_MACHINE := $(CC_DUMP_MACHINE)
ifneq ($(findstring darwin,$(CC_DUMP_MACHINE)),)
OPENMP_FLAGS =
THREAD_LINK_LIB =
PLATFORM_CFLAGS =
else ifeq ($(or $(findstring mingw,$(CC_DUMP_MACHINE)),$(MSYSTEM)),)
OPENMP_FLAGS = -fopenmp
THREAD_LINK_LIB = -lpthread
PLATFORM_CFLAGS =
else
OPENMP_FLAGS =
THREAD_LINK_LIB = -lwinpthread
# MinGW gcc links the MSVCRT printf by default, which rejects the C99
# %zu / %ju / %jd / %td specifiers. Defining __USE_MINGW_ANSI_STDIO=1
# routes printf/fprintf/sprintf/snprintf through MinGW's C99-conformant
# __mingw_vfprintf wrapper so diagnostic messages (e.g. "expects %zu
# argument(s), got %zu") print correctly and -Wformat validation passes.
# This is MinGW's own documented opt-in — no effect on Linux/macOS.
PLATFORM_CFLAGS = -D__USE_MINGW_ANSI_STDIO=1
endif
TMPDIR ?= /tmp
export TMPDIR
TMPDIR_CI := $(subst \,/,$(TMPDIR))
CFLAGS  = -Wall -Wextra -Werror=implicit-function-declaration -Werror=implicit-int -std=c11 -O2 -g $(OPENMP_FLAGS) $(PLATFORM_CFLAGS) -I$(SRC_DIR)
DEPFLAGS = -MMD -MP -MT $@
ASMFLAGS = -f elf64
NASM    := $(shell command -v nasm 2>/dev/null)
CI_LINUX_CC := $(or $(shell command -v cc 2>/dev/null),$(shell command -v gcc 2>/dev/null),gcc)
CI_WINDOWS_CROSS_CC := $(shell command -v x86_64-w64-mingw32-gcc 2>/dev/null)
CI_WINDOWS_CC := $(if $(MSYSTEM),$(CC),$(or $(CI_WINDOWS_CROSS_CC),$(CC)))
CI_WINDOWS_CC_MACHINE := $(shell $(CI_WINDOWS_CC) -dumpmachine 2>/dev/null || echo unknown)
CI_WINDOWS_RUNNABLE := $(if $(MSYSTEM),1,0)
CI_LINUX_BUILD_DIR := $(TMPDIR_CI)/pgy-ci-linux-build
CI_LINUX_BIN_DIR   := $(TMPDIR_CI)/pgy-ci-linux-bin
CI_WINDOWS_BUILD_DIR := $(TMPDIR_CI)/pgy-ci-windows-build
CI_WINDOWS_BIN_DIR   := $(TMPDIR_CI)/pgy-ci-windows-bin
CI_MACOS_CC := $(or $(shell command -v cc 2>/dev/null),$(CC))
CI_MACOS_BUILD_DIR := $(TMPDIR_CI)/pgy-ci-macos-build
CI_MACOS_BIN_DIR   := $(TMPDIR_CI)/pgy-ci-macos-bin
BASH := $(shell command -v bash 2>/dev/null)
ifeq ($(strip $(BASH)),)
BASH := bash
endif
LLVM_CONFIG := $(shell command -v llvm-config 2>/dev/null || command -v llvm-config-20 2>/dev/null || command -v llvm-config-19 2>/dev/null || command -v llvm-config-18 2>/dev/null || command -v llvm-config-17 2>/dev/null || command -v llvm-config-16 2>/dev/null || command -v llvm-config-15 2>/dev/null)
WINDOWS_LLVM_READY := $(shell if [ -n "$(LLVM_CONFIG)" ] && "$(LLVM_CONFIG)" --libs core >/dev/null 2>&1; then echo 1; else echo 0; fi)
LLD := $(shell command -v ld.lld 2>/dev/null || command -v lld 2>/dev/null)
PROJECT_ROOT := $(CURDIR)
CFLAGS  += -DPGY_PROJECT_ROOT=\"$(PROJECT_ROOT)\"
CFLAGS  += -DPGY_SRC_DIR=\"$(PROJECT_ROOT)/src\"
CFLAGS  += -DPGY_RUNTIME_DIR=\"$(PROJECT_ROOT)/src/runtime\"
CFLAGS  += -DPGY_RUNTIME_LIB_C=\"$(PROJECT_ROOT)/src/runtime/pgy_runtime_lib.c\"
LLVM_MONOLITHIC_SONAME := $(firstword $(wildcard /usr/lib/llvm-*/lib/libLLVM.so.1 /usr/lib/llvm-*/lib/libLLVM.so.* /usr/lib/x86_64-linux-gnu/libLLVM.so.* /usr/lib/x86_64-linux-gnu/libLLVM-*.so /lib/x86_64-linux-gnu/libLLVM.so.* /lib/x86_64-linux-gnu/libLLVM-*.so))
LLVM_MONOLITHIC_DIR := $(dir $(LLVM_MONOLITHIC_SONAME))
LLVM_MONOLITHIC_NAME := $(notdir $(LLVM_MONOLITHIC_SONAME))
LLVM_INCLUDEDIR_FALLBACK := $(or $(shell for d in /usr/lib/llvm-*/include /usr/include third_party; do if [ -f "$$d/llvm-c/Core.h" ]; then echo "$$d"; break; fi; done),third_party)

ifneq ($(or $(findstring mingw,$(CC_MACHINE)),$(MSYSTEM)),)
EXEEXT := .exe
else
EXEEXT :=
endif

# -----------------------------------------------------------------
# LLVM backend (enabled by default)
#   make                         — build with LLVM native backend (default)
#   make LLVM_ENABLED=0          — build without LLVM (C transpiler only)
# -----------------------------------------------------------------
LLVM_DIR     = third_party
LLVM_INSTALL = C:/Program Files/LLVM
LLVM_ENABLED ?= 1
STDLIB_BACKENDS ?= $(if $(filter 0,$(LLVM_ENABLED)),c,c llvm)
RUNTIME_PANIC_CODEGEN_BACKENDS ?= $(if $(filter 0,$(LLVM_ENABLED)),c,c llvm)
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
COMMON_SOURCES   = $(COMMON_DIR)/arena.c
LEXER_SOURCES    = $(LEXER_DIR)/lexer.c \
                   $(LEXER_DIR)/lexer_token_debug.c
PARSER_SOURCES   = $(PARSER_DIR)/ast.c \
                   $(PARSER_DIR)/ast_destroy.c \
                   $(PARSER_DIR)/ast_destroy_domain.c \
                   $(PARSER_DIR)/ast_clone.c \
                   $(PARSER_DIR)/ast_constructors.c \
                   $(PARSER_DIR)/ast_domain_constructors.c \
                   $(PARSER_DIR)/ast_print.c \
                   $(PARSER_DIR)/ast_print_domain.c \
                   $(PARSER_DIR)/ast_print_event.c \
                   $(PARSER_DIR)/ast_print_generics.c \
                   $(PARSER_DIR)/ast_print_inline.c \
                   $(PARSER_DIR)/ast_print_intent.c \
                   $(PARSER_DIR)/ast_print_misc.c \
                   $(PARSER_DIR)/parser.c \
                   $(PARSER_DIR)/parser_decl_hints.c \
                   $(PARSER_DIR)/parser_doc.c \
                   $(PARSER_DIR)/parser_enum.c \
                   $(PARSER_DIR)/parser_export.c \
                   $(PARSER_DIR)/parser_expr.c \
                   $(PARSER_DIR)/parser_pin.c \
                   $(PARSER_DIR)/parser_stmt.c \
                   $(PARSER_DIR)/parser_statement_dispatch.c \
                   $(PARSER_DIR)/parser_type.c \
                   $(PARSER_DIR)/parser_zone_context.c \
                   $(PARSER_DIR)/parser_decl.c \
                   $(PARSER_DIR)/parser_decl_clause.c \
                   $(PARSER_DIR)/parser_decl_function_clause.c \
                   $(PARSER_DIR)/parser_decl_start.c \
                   $(PARSER_DIR)/parser_intent.c \
                   $(PARSER_DIR)/parser_domain.c \
                   $(PARSER_DIR)/parser_domain_event.c \
                   $(PARSER_DIR)/parser_domain_projection.c \
                   $(PARSER_DIR)/parser_domain_relation_effect.c \
                   $(PARSER_DIR)/parser_domain_roster.c \
                   $(PARSER_DIR)/parser_domain_world.c \
                   $(PARSER_DIR)/parser_domain_zone.c \
                   $(PARSER_DIR)/parser_async.c
RUNTIME_SOURCES  = $(RUNTIME_DIR)/slot_manager.c \
                   $(RUNTIME_DIR)/slot_manager_pin.c \
                   $(RUNTIME_DIR)/slot_manager_query_lock.c \
                   $(RUNTIME_DIR)/slot_manager_secure_ops.c \
                   $(RUNTIME_DIR)/slot_type_utils.c \
                   $(RUNTIME_DIR)/slot_pool.c \
                   $(RUNTIME_DIR)/slot_pool_perf.c \
                   $(RUNTIME_DIR)/slot_security.c \
                   $(RUNTIME_DIR)/slot_security_crypto.c \
                   $(RUNTIME_DIR)/slot_security_memory.c \
                   $(RUNTIME_DIR)/slot_security_platform.c \
                   $(RUNTIME_DIR)/slot_security_sealed_payload.c \
                   $(RUNTIME_DIR)/slot_manager_security_stats.c \
                   $(RUNTIME_DIR)/slot_manager_scope.c \
                   $(RUNTIME_DIR)/party_runtime.c \
                   $(RUNTIME_DIR)/world_roster.c
ASYNC_SOURCES    = $(ASYNC_DIR)/concurrent_queue.c \
                   $(ASYNC_DIR)/async_scope.c \
                   $(ASYNC_DIR)/fiber.c \
                   $(ASYNC_DIR)/scheduler.c
RUNTIME_SOURCES  += $(ASYNC_SOURCES)
RUNTIME_ASM_SOURCES = $(RUNTIME_DIR)/slot_asm.s
SEMANTIC_SOURCES = $(SEMANTIC_DIR)/type_system.c \
                   $(SEMANTIC_DIR)/type_effects.c \
                   $(SEMANTIC_DIR)/type_infer.c \
                   $(SEMANTIC_DIR)/type_env.c \
                   $(SEMANTIC_DIR)/symbol_table.c \
                   $(SEMANTIC_DIR)/type_checker.c \
                   $(SEMANTIC_DIR)/type_checker_diag.c \
                   $(SEMANTIC_DIR)/type_checker_generic_diag.c \
                   $(SEMANTIC_DIR)/type_checker_generic_validation.c \
                   $(SEMANTIC_DIR)/type_checker_type_constraint.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_helpers.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_core.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_collect.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_labels.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_domain.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_world.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_zone.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_zone_inventory.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_zone_tail.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_inventory.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_body.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_graph_decl.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_metadata_fallback.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_metadata_constructed.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_metadata_alias.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_metadata_diagnostics.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_metadata_storage.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_metadata.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_stage_domain.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_stage_lookup.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_stage_stats.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_stage_signature.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_stage_alias.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_stage_nominal.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_stage_systemic.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_stage_domain_decl.c \
                   $(SEMANTIC_DIR)/type_checker_resolution_stage.c \
                   $(SEMANTIC_DIR)/type_checker_class_decl.c \
                   $(SEMANTIC_DIR)/type_checker_host_helpers.c \
                   $(SEMANTIC_DIR)/type_checker_program.c \
                   $(SEMANTIC_DIR)/type_checker_func_decl.c \
                   $(SEMANTIC_DIR)/type_checker_func_action_contract.c \
                   $(SEMANTIC_DIR)/type_checker_relation_decl.c \
                   $(SEMANTIC_DIR)/type_checker_effect_decl.c \
                   $(SEMANTIC_DIR)/type_checker_zone_decl.c \
                   $(SEMANTIC_DIR)/type_checker_zone_decl_authority.c \
                   $(SEMANTIC_DIR)/type_checker_zone_shape.c \
                   $(SEMANTIC_DIR)/type_checker_zone_projection_rules.c \
                   $(SEMANTIC_DIR)/type_checker_zone_state.c \
                   $(SEMANTIC_DIR)/type_checker_ability_decl.c \
                   $(SEMANTIC_DIR)/type_checker_world_decl.c \
                   $(SEMANTIC_DIR)/type_checker_world_helpers.c \
                   $(SEMANTIC_DIR)/type_checker_domain_slots.c \
                   $(SEMANTIC_DIR)/type_checker_intent_decl.c \
                   $(SEMANTIC_DIR)/type_checker_intent_action_contract.c \
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
                   $(SEMANTIC_DIR)/type_checker_event.c \
                   $(SEMANTIC_DIR)/type_checker_qubit.c \
                   $(SEMANTIC_DIR)/type_checker_ability_ref.c \
                   $(SEMANTIC_DIR)/type_checker_stdlib_use.c \
                   $(SEMANTIC_DIR)/type_checker_module_contract.c \
                   $(SEMANTIC_DIR)/type_checker_module_contract_diag.c \
                   $(SEMANTIC_DIR)/type_checker_ability_fields.c \
                   $(SEMANTIC_DIR)/type_checker_ability_match.c \
                   $(SEMANTIC_DIR)/type_checker_ability_where.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_classify.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_diag.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_diag_constructor.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_assign.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_array_store.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_boundaries.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_call.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_destructure.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_let.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_param_summary.c \
                   $(SEMANTIC_DIR)/type_checker_ownership_return.c \
                    $(SEMANTIC_DIR)/type_checker_channel_transport.c \
                    $(SEMANTIC_DIR)/type_checker_visibility.c \
                    $(SEMANTIC_DIR)/type_checker_builtins_projection.c \
                    $(SEMANTIC_DIR)/type_checker_resolution_retired.c \
                    $(SEMANTIC_DIR)/type_checker_type_helpers.c \
                    $(SEMANTIC_DIR)/type_checker_expr_host.c \
                   $(SEMANTIC_DIR)/type_checker_expr_call.c \
                   $(SEMANTIC_DIR)/type_checker_expr.c \
                   $(SEMANTIC_DIR)/type_checker_expr_names.c \
                   $(SEMANTIC_DIR)/type_checker_expr_ops.c \
                   $(SEMANTIC_DIR)/type_checker_builtins.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_intent_observability.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_nominal.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_query.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_query_channel.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_query_domain.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_query_world.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_resolve.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_secure_token.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_slotops.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_cancel.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_stdlib_scalar.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_stdlib_map.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_stdlib_collections.c \
                   $(SEMANTIC_DIR)/type_checker_builtins_stdlib_body.c \
                   $(SEMANTIC_DIR)/type_checker_domain_role_lookup.c \
                   $(SEMANTIC_DIR)/type_checker_domain_projection.c \
                   $(SEMANTIC_DIR)/type_checker_overlay_common.c \
                   $(SEMANTIC_DIR)/type_checker_projection_path.c \
                   $(SEMANTIC_DIR)/type_checker_world_embedding.c \
                   $(SEMANTIC_DIR)/type_checker_decls_domain_helpers.c \
                   $(SEMANTIC_DIR)/type_checker_domain_contracts.c \
                   $(SEMANTIC_DIR)/type_checker_call_constructor.c \
                   $(SEMANTIC_DIR)/type_checker_call_contract_helpers.c \
                   $(SEMANTIC_DIR)/type_checker_call_generic_where.c \
                   $(SEMANTIC_DIR)/type_checker_helpers_effects.c \
                   $(SEMANTIC_DIR)/type_checker_helpers_late.c \
                   $(SEMANTIC_DIR)/type_checker_slot_view_active.c \
                   $(SEMANTIC_DIR)/type_checker_slot_view_boundary.c \
                   $(SEMANTIC_DIR)/type_checker_flow_effects.c \
                   $(SEMANTIC_DIR)/type_checker_flow_match.c \
                   $(SEMANTIC_DIR)/type_checker_flow.c \
                   $(SEMANTIC_DIR)/slot_analyzer.c \
                   $(SEMANTIC_DIR)/slot_analyzer_escape.c \
                   $(SEMANTIC_DIR)/slot_analyzer_summary.c \
                   $(SEMANTIC_DIR)/semantic.c
CODEGEN_SOURCES  = $(CODEGEN_DIR)/transpiler_context.c \
                   $(CODEGEN_DIR)/transpiler_entry.c \
                   $(CODEGEN_DIR)/transpiler_symbols.c \
                   $(CODEGEN_DIR)/transpiler_decl_lookup.c \
                   $(CODEGEN_DIR)/transpiler_decl_host_lookup.c \
                   $(CODEGEN_DIR)/transpiler_enum.c \
                   $(CODEGEN_DIR)/transpiler_extern.c \
                   $(CODEGEN_DIR)/transpiler_log_normalize.c \
                   $(CODEGEN_DIR)/transpiler_misc_decl.c \
                   $(CODEGEN_DIR)/transpiler_nominal.c \
                   $(CODEGEN_DIR)/transpiler_operator.c \
                   $(CODEGEN_DIR)/transpiler_projection.c \
                   $(CODEGEN_DIR)/transpiler_thread_pool.c \
                   $(CODEGEN_DIR)/transpiler_type_alias.c \
                   $(CODEGEN_DIR)/transpiler_type_declarator.c \
                   $(CODEGEN_DIR)/transpiler_type_require.c \
                   $(CODEGEN_DIR)/transpiler.c
COMPILER_SOURCES = $(COMPILER_DIR)/compiler.c \
                   $(COMPILER_DIR)/compiler_result.c \
                   $(COMPILER_DIR)/compiler_llvm.c \
                   $(COMPILER_DIR)/dir.c \
                   $(COMPILER_DIR)/dir_collect.c \
                   $(COMPILER_DIR)/dir_collect_domain.c \
                   $(COMPILER_DIR)/dir_validate.c \
                   $(COMPILER_DIR)/air.c \
                   $(COMPILER_DIR)/air_boundary.c \
                   $(COMPILER_DIR)/air_boundary_walk.c \
                   $(COMPILER_DIR)/air_dump.c \
                   $(COMPILER_DIR)/air_evidence.c \
                   $(COMPILER_DIR)/air_verify.c \
                   $(COMPILER_DIR)/rir.c \
                   $(COMPILER_DIR)/rir_names.c \
                   $(COMPILER_DIR)/rir_public_surface.c \
                   $(COMPILER_DIR)/rir_validation.c \
                   $(COMPILER_DIR)/rir_flow.c \
                   $(COMPILER_DIR)/rir_facts.c \
                   $(COMPILER_DIR)/rir_builder.c \
                   $(COMPILER_DIR)/rir_builder_walk.c \
                   $(COMPILER_DIR)/rir_builder_intent.c \
                   $(COMPILER_DIR)/hir_analysis.c \
                   $(COMPILER_DIR)/hir_cfg.c \
                   $(COMPILER_DIR)/hir_cfg_phi.c \
                   $(COMPILER_DIR)/hir_lower_cfg.c \
                   $(COMPILER_DIR)/hir_lower_intent_cfg.c \
                   $(COMPILER_DIR)/mir.c \
                   $(COMPILER_DIR)/mir_cleanup.c \
                   $(COMPILER_DIR)/mir_intent.c \
                   $(COMPILER_DIR)/mir_type_helpers.c \
                   $(COMPILER_DIR)/hir.c \
                   $(COMPILER_DIR)/hir_routines.c \
                   $(COMPILER_DIR)/hir_destroy.c \
                   $(COMPILER_DIR)/hir_public.c \
                   $(COMPILER_DIR)/module_loader.c \
                   $(COMPILER_DIR)/module_normalizer.c \
                   $(COMPILER_DIR)/module_normalizer_refs.c \
                   $(COMPILER_DIR)/import_resolver.c \
                   $(COMPILER_DIR)/driver_app.c \
                   $(COMPILER_DIR)/driver_diag.c \
                   $(COMPILER_DIR)/driver_scaffold.c \
                   $(COMPILER_DIR)/driver_scaffold_project.c \
                   $(COMPILER_DIR)/compiler_toolchain.c \
                   $(COMPILER_DIR)/compiler_runtime_cache.c \
                   $(COMPILER_DIR)/runtime_none_contract.c \
                   $(COMPILER_DIR)/path_utils.c \
                   $(COMPILER_DIR)/llvm_runner.c \
                   $(COMPILER_DIR)/c_runner.c \
                   $(COMPILER_DIR)/repl.c \
                   $(COMPILER_DIR)/fmt.c \
                   $(COMPILER_DIR)/pkg.c \
                   $(COMPILER_DIR)/debugger.c

# LLVM backend sources (only compiled when LLVM_ENABLED=1)
ifneq ($(LLVM_ENABLED),0)
  LLVM_BACKEND_SOURCES = $(CODEGEN_DIR)/llvm_backend.c \
                   $(CODEGEN_DIR)/llvm_backend_type_map.c \
                   $(CODEGEN_DIR)/llvm_type.c \
                   $(CODEGEN_DIR)/llvm_api.c \
                   $(CODEGEN_DIR)/llvm_backend_generic.c \
                   $(CODEGEN_DIR)/llvm_pipeline.c \
                         $(CODEGEN_DIR)/llvm_intent.c \
                         $(CODEGEN_DIR)/llvm_intent_setup.c \
                         $(CODEGEN_DIR)/llvm_intent_cleanup.c \
                         $(CODEGEN_DIR)/llvm_intent_step_context.c \
                         $(CODEGEN_DIR)/llvm_registry.c \
                         $(CODEGEN_DIR)/llvm_registry_resources.c \
                         $(CODEGEN_DIR)/llvm_error.c \
                          $(CODEGEN_DIR)/llvm_register.c \
                          $(CODEGEN_DIR)/llvm_runtime.c \
                          $(CODEGEN_DIR)/llvm_runtime_raw_collections.c \
                          $(CODEGEN_DIR)/llvm_runtime_channels.c \
                         $(CODEGEN_DIR)/llvm_event.c \
                        $(CODEGEN_DIR)/llvm_mir_emit.c \
                        $(CODEGEN_DIR)/llvm_mir_cfg_control.c \
                        $(CODEGEN_DIR)/llvm_mir_for_in_control.c \
                        $(CODEGEN_DIR)/llvm_intent_mir_meta.c \
                         $(CODEGEN_DIR)/llvm_intent_zone.c \
                         $(CODEGEN_DIR)/llvm_intent_effect.c \
                         $(CODEGEN_DIR)/llvm_intent_flow.c \
                         $(CODEGEN_DIR)/llvm_expr.c \
                         $(CODEGEN_DIR)/llvm_stmt.c \
                         $(CODEGEN_DIR)/llvm_stmt_type_infer.c \
                         $(CODEGEN_DIR)/llvm_stmt_let_callable.c \
                         $(CODEGEN_DIR)/llvm_stmt_let_collections.c \
                         $(CODEGEN_DIR)/llvm_stmt_let_helpers.c \
                         $(CODEGEN_DIR)/llvm_stmt_let_with.c \
                         $(CODEGEN_DIR)/llvm_stmt_with.c \
                         $(CODEGEN_DIR)/llvm_stmt_loop_match.c \
                         $(CODEGEN_DIR)/llvm_stmt_parallel_async.c \
                         $(CODEGEN_DIR)/llvm_stmt_type_render.c \
                         $(CODEGEN_DIR)/llvm_stmt_zone_action.c \
                         $(CODEGEN_DIR)/llvm_decl.c \
                         $(CODEGEN_DIR)/llvm_domain_method_helpers.c \
                   $(CODEGEN_DIR)/llvm_domain_method_emit.c \
                   $(CODEGEN_DIR)/llvm_domain_event.c \
                   $(CODEGEN_DIR)/llvm_domain_role_emit.c \
                    $(CODEGEN_DIR)/llvm_domain_sync_frontier.c \
                    $(CODEGEN_DIR)/llvm_domain_zone_frontier_state.c \
                          $(CODEGEN_DIR)/llvm_domain_zone_sync.c \
                          $(CODEGEN_DIR)/llvm_domain_zone_sync_relations.c \
                          $(CODEGEN_DIR)/llvm_domain_world_sync_directives.c \
                          $(CODEGEN_DIR)/llvm_domain_world_frontier.c \
                          $(CODEGEN_DIR)/llvm_domain_world_sync.c \
                         $(CODEGEN_DIR)/llvm_domain_forward.c \
                         $(CODEGEN_DIR)/llvm_domain_struct_fields.c \
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
ifeq ($(NASM),)
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
                   $(ASYNC_SOURCES) \
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
                   $(RUNTIME_DIR)/pgy_runtime_lib_device_slot_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_io_string_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_qubit_state_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_raw_collection_common_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_raw_map_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_raw_queue_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_raw_set_exports.h \
                   $(RUNTIME_DIR)/pgy_runtime_lib_secure_slot_exports.h \
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
SEMANTIC_LINK_SUPPORT = $(BUILD_DIR)/compiler/import_resolver.o \
                        $(BUILD_DIR)/compiler/module_normalizer.o \
                        $(BUILD_DIR)/compiler/module_normalizer_refs.o \
                        $(BUILD_DIR)/compiler/path_utils.o

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
# Default target — build the driver and all tests
# -----------------------------------------------------------------
all: $(PGY) $(PGY_LSP) $(LEXER_TEST) $(PARSER_TEST) $(SEMANTIC_TEST) $(TRANSPILE_TEST) $(MEMORY_TEST) $(CONCURRENCY_TEST) $(HIR_TEST)

pgy: $(PGY) $(REPO_BIN_DIR)/pgy$(EXEEXT)
llvm:
	$(MAKE) LLVM_ENABLED=1 all

# -----------------------------------------------------------------
# Build rules
# -----------------------------------------------------------------

# pgy compiler driver
$(PGY): $(FRONTEND_OBJECTS) $(DRIVER_OBJ) | $(BIN_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_LLVM) $(THREAD_LINK_LIB) -lm

$(REPO_BIN_DIR)/pgy$(EXEEXT): $(PGY) | $(REPO_BIN_DIR)
	@if [ "$(abspath $<)" != "$(abspath $@)" ]; then cp -f "$<" "$@"; fi

# Lexer smoke-test (original main.c)
$(LEXER_TEST): $(LEXER_OBJECTS) $(MAIN_OBJECT) | $(BIN_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^

# Parser test
$(PARSER_TEST): $(LEXER_OBJECTS) $(PARSER_OBJECTS) $(PARSER_TEST_OBJECT) | $(BIN_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^

# Data structures test
$(DATASTRUCTURES_TEST): $(BUILD_DIR)/runtime/slot_pool.o \
                         $(BUILD_DIR)/runtime/slot_pool_perf.o \
                         $(TEST_DATASTRUCTURES_OBJ) | $(BIN_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(THREAD_LINK_LIB)

# Security test
$(SECURITY_TEST): $(RUNTIME_OBJECTS) $(RUNTIME_ASM_OBJECTS) \
                   $(TEST_SECURITY_OBJ) | $(BIN_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(THREAD_LINK_LIB) -lssl -lcrypto

# Semantic analyzer test
$(SEMANTIC_TEST): $(FRONTEND_OBJECTS) $(TEST_SEMANTIC_OBJ) | $(BIN_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_LLVM) $(THREAD_LINK_LIB) -lm

# C backend test
$(TRANSPILE_TEST): $(FRONTEND_OBJECTS) $(TEST_TRANSPILE_OBJ) | $(BIN_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_LLVM) $(THREAD_LINK_LIB) -lm

# Memory layout test (runtime-only, no frontend)
$(MEMORY_TEST): $(TEST_MEMORY_OBJ) | $(BIN_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^

# ABI spec validation test (runtime-only, includes pgy_runtime.h for cross-check)
$(ABI_TEST): $(TEST_ABI_OBJ) | $(BIN_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^

# ABI pipeline integration test (frontend + backend + produced binary)
$(ABI_PIPELINE_TEST): $(FRONTEND_OBJECTS) $(TEST_ABI_PIPELINE_OBJ) | $(BIN_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_LLVM) $(THREAD_LINK_LIB) -lm

# Concurrency runtime test
$(CONCURRENCY_TEST): $(TEST_CONCURRENCY_OBJ) | $(BIN_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(THREAD_LINK_LIB)

# DIR lowering test
$(DIR_TEST): $(COMMON_OBJECTS) $(LEXER_OBJECTS) $(PARSER_OBJECTS) $(SEMANTIC_OBJECTS) $(SEMANTIC_LINK_SUPPORT) $(BUILD_DIR)/compiler/dir.o $(BUILD_DIR)/compiler/dir_collect.o $(BUILD_DIR)/compiler/dir_collect_domain.o $(BUILD_DIR)/compiler/dir_validate.o $(TEST_DIR_OBJ) | $(BIN_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(THREAD_LINK_LIB)

# AIR synthesis and drift test
$(AIR_TEST): $(COMMON_OBJECTS) $(LEXER_OBJECTS) $(PARSER_OBJECTS) $(SEMANTIC_OBJECTS) $(SEMANTIC_LINK_SUPPORT) $(BUILD_DIR)/compiler/dir.o $(BUILD_DIR)/compiler/dir_collect.o $(BUILD_DIR)/compiler/dir_collect_domain.o $(BUILD_DIR)/compiler/dir_validate.o $(BUILD_DIR)/compiler/hir_analysis.o $(BUILD_DIR)/compiler/hir_cfg.o $(BUILD_DIR)/compiler/hir_cfg_phi.o $(BUILD_DIR)/compiler/hir_lower_cfg.o $(BUILD_DIR)/compiler/hir_lower_intent_cfg.o $(BUILD_DIR)/compiler/hir.o $(BUILD_DIR)/compiler/hir_routines.o $(BUILD_DIR)/compiler/hir_destroy.o $(BUILD_DIR)/compiler/hir_public.o $(BUILD_DIR)/compiler/rir.o $(BUILD_DIR)/compiler/rir_names.o $(BUILD_DIR)/compiler/rir_public_surface.o $(BUILD_DIR)/compiler/rir_validation.o $(BUILD_DIR)/compiler/rir_flow.o $(BUILD_DIR)/compiler/rir_facts.o $(BUILD_DIR)/compiler/rir_builder.o $(BUILD_DIR)/compiler/rir_builder_walk.o $(BUILD_DIR)/compiler/rir_builder_intent.o $(BUILD_DIR)/compiler/air.o $(BUILD_DIR)/compiler/air_boundary.o $(BUILD_DIR)/compiler/air_boundary_walk.o $(BUILD_DIR)/compiler/air_dump.o $(BUILD_DIR)/compiler/air_evidence.o $(BUILD_DIR)/compiler/air_verify.o $(TEST_AIR_OBJ) | $(BIN_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(THREAD_LINK_LIB)

# RIR lowering test
$(RIR_TEST): $(COMMON_OBJECTS) $(LEXER_OBJECTS) $(PARSER_OBJECTS) $(SEMANTIC_OBJECTS) $(SEMANTIC_LINK_SUPPORT) $(BUILD_DIR)/compiler/dir.o $(BUILD_DIR)/compiler/dir_collect.o $(BUILD_DIR)/compiler/dir_collect_domain.o $(BUILD_DIR)/compiler/dir_validate.o $(BUILD_DIR)/compiler/hir_analysis.o $(BUILD_DIR)/compiler/hir_cfg.o $(BUILD_DIR)/compiler/hir_cfg_phi.o $(BUILD_DIR)/compiler/hir_lower_cfg.o $(BUILD_DIR)/compiler/hir_lower_intent_cfg.o $(BUILD_DIR)/compiler/hir.o $(BUILD_DIR)/compiler/hir_routines.o $(BUILD_DIR)/compiler/hir_destroy.o $(BUILD_DIR)/compiler/hir_public.o $(BUILD_DIR)/compiler/rir.o $(BUILD_DIR)/compiler/rir_names.o $(BUILD_DIR)/compiler/rir_public_surface.o $(BUILD_DIR)/compiler/rir_validation.o $(BUILD_DIR)/compiler/rir_flow.o $(BUILD_DIR)/compiler/rir_facts.o $(BUILD_DIR)/compiler/rir_builder.o $(BUILD_DIR)/compiler/rir_builder_walk.o $(BUILD_DIR)/compiler/rir_builder_intent.o $(TEST_RIR_OBJ) | $(BIN_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(THREAD_LINK_LIB)

# MIR lowering test
$(MIR_TEST): $(COMMON_OBJECTS) $(LEXER_OBJECTS) $(PARSER_OBJECTS) $(SEMANTIC_OBJECTS) $(SEMANTIC_LINK_SUPPORT) $(BUILD_DIR)/compiler/hir_analysis.o $(BUILD_DIR)/compiler/hir_cfg.o $(BUILD_DIR)/compiler/hir_cfg_phi.o $(BUILD_DIR)/compiler/hir_lower_cfg.o $(BUILD_DIR)/compiler/hir_lower_intent_cfg.o $(BUILD_DIR)/compiler/hir.o $(BUILD_DIR)/compiler/hir_routines.o $(BUILD_DIR)/compiler/hir_destroy.o $(BUILD_DIR)/compiler/hir_public.o $(BUILD_DIR)/compiler/rir.o $(BUILD_DIR)/compiler/rir_names.o $(BUILD_DIR)/compiler/rir_public_surface.o $(BUILD_DIR)/compiler/rir_validation.o $(BUILD_DIR)/compiler/rir_flow.o $(BUILD_DIR)/compiler/rir_facts.o $(BUILD_DIR)/compiler/rir_builder.o $(BUILD_DIR)/compiler/rir_builder_walk.o $(BUILD_DIR)/compiler/rir_builder_intent.o $(BUILD_DIR)/compiler/mir.o $(BUILD_DIR)/compiler/mir_cleanup.o $(BUILD_DIR)/compiler/mir_intent.o $(BUILD_DIR)/compiler/mir_type_helpers.o $(TEST_MIR_OBJ) | $(BIN_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(THREAD_LINK_LIB)

# HIR lowering test
$(HIR_TEST): $(COMMON_OBJECTS) $(LEXER_OBJECTS) $(PARSER_OBJECTS) $(SEMANTIC_OBJECTS) $(SEMANTIC_LINK_SUPPORT) $(BUILD_DIR)/compiler/hir_analysis.o $(BUILD_DIR)/compiler/hir_cfg.o $(BUILD_DIR)/compiler/hir_cfg_phi.o $(BUILD_DIR)/compiler/hir_lower_cfg.o $(BUILD_DIR)/compiler/hir_lower_intent_cfg.o $(BUILD_DIR)/compiler/hir.o $(BUILD_DIR)/compiler/hir_routines.o $(BUILD_DIR)/compiler/hir_destroy.o $(BUILD_DIR)/compiler/hir_public.o $(TEST_HIR_OBJ) | $(BIN_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(THREAD_LINK_LIB)

# LSP server
$(PGY_LSP): $(COMMON_OBJECTS) $(LEXER_OBJECTS) $(PARSER_OBJECTS) $(SEMANTIC_OBJECTS) $(SEMANTIC_LINK_SUPPORT) $(LSP_OBJECTS) | $(BIN_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(THREAD_LINK_LIB)

$(REPO_BIN_DIR)/pgy-lsp$(EXEEXT): $(PGY_LSP) | $(REPO_BIN_DIR)
	@if [ "$(abspath $<)" != "$(abspath $@)" ]; then cp -f "$<" "$@"; fi

# -----------------------------------------------------------------
# Compilation rules
# -----------------------------------------------------------------

# C sources
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(CONFIG_STAMP) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -MF "$(@:.o=.d)" -c -o "$@" $<
	@-sed -i -E 's#[A-Za-z]:/$(notdir $(PROJECT_ROOT))/##g; s#([A-Za-z]):/#\1\\:/#g' "$(@:.o=.d)" 2>/dev/null || true

# Assembly sources
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(NASM) $(ASMFLAGS) -o $@ $<

# -----------------------------------------------------------------
# Directory creation
# -----------------------------------------------------------------
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR) \
		$(BUILD_DIR)/common \
		$(BUILD_DIR)/lexer \
		$(BUILD_DIR)/parser \
		$(BUILD_DIR)/semantic \
		$(BUILD_DIR)/codegen \
		$(BUILD_DIR)/compiler \
		$(BUILD_DIR)/runtime \
		$(BUILD_DIR)/runtime/async \
		$(BUILD_DIR)/lsp
	touch -c -r Makefile $(BUILD_DIR)

$(CONFIG_STAMP): | $(BUILD_DIR)
	rm -f $(BUILD_DIR)/.config_llvm_*.stamp
	find $(BUILD_DIR) -type f \( -name '*.o' -o -name '*.d' \) -delete
	printf "LLVM_ENABLED=%s\nCC=%s\nCC_MACHINE=%s\n" \
		"$(LLVM_ENABLED)" "$(CC)" "$(CC_MACHINE)" > $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)
	touch -c -r Makefile $(BIN_DIR)

$(REPO_BIN_DIR):
	mkdir -p $(REPO_BIN_DIR)

# -----------------------------------------------------------------
# Test execution targets
# -----------------------------------------------------------------
test: $(LEXER_TEST)
	@echo "=== Lexer Test ==="
	$(LEXER_TEST)

test-parser: $(PARSER_TEST)
	@echo "=== Parser Test ==="
	$(PARSER_TEST)

test-datastructures: $(DATASTRUCTURES_TEST)
	@echo "=== Data Structures Test ==="
	$(DATASTRUCTURES_TEST)

test-security: $(SECURITY_TEST)
	@echo "=== Security Test ==="
	$(SECURITY_TEST)

test-semantic: $(SEMANTIC_TEST)
	@echo "=== Semantic Analyzer Test ==="
	$(SEMANTIC_TEST)

test-transpile: $(TRANSPILE_TEST)
	@echo "=== C Backend Test ==="
	$(TRANSPILE_TEST)

test-memory: $(MEMORY_TEST)
	@echo "=== Memory Layout Test ==="
	$(MEMORY_TEST)

test-abi: $(ABI_TEST) $(PGY)
	@echo "=== ABI Spec Validation ==="
	$(ABI_TEST)
	@echo "=== ABI Pipeline Smoke ==="
	PGY_BIN="$(abspath $(PGY))" \
	PGY_ABI_PIPELINE_BACKENDS="$(if $(filter 1,$(LLVM_ENABLED)),c llvm,c)" \
	$(BASH) tests/abi_pipeline_smoke.sh

$(ABI_PERF_RUNTIME_RELEASE_OBS0): $(RUNTIME_DIR)/pgy_runtime_lib.c $(RUNTIME_DIR)/pgy_runtime.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -O3 -DPGY_LLVM_ENABLED -DPGY_INTENT_OBSERVABILITY_ENABLED=0 -c -o $@ $<

$(ABI_PERF_RUNTIME_RELEASE_OBS1): $(RUNTIME_DIR)/pgy_runtime_lib.c $(RUNTIME_DIR)/pgy_runtime.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -O3 -DPGY_LLVM_ENABLED -DPGY_INTENT_OBSERVABILITY_ENABLED=1 -c -o $@ $<

abi-perf-runtime: $(ABI_PERF_RUNTIME_RELEASE_OBS0) $(ABI_PERF_RUNTIME_RELEASE_OBS1)

test-abi-perf: $(ABI_PIPELINE_TEST) abi-perf-runtime
	@echo "=== ABI Pipeline Benchmark ==="
	$(if $(ABI_PERF_LINKER_ENV),$(ABI_PERF_LINKER_ENV) )PGY_PREBUILT_RUNTIME_OBJ_RELEASE_OBS0="$(ABI_PERF_RUNTIME_RELEASE_OBS0)" \
	PGY_PREBUILT_RUNTIME_OBJ_RELEASE_OBS1="$(ABI_PERF_RUNTIME_RELEASE_OBS1)" \
	PGY_ABI_PERF_MODE=1 "$(ABI_PIPELINE_TEST)"

perf-summary:
	@test -n "$(PERF_LOG)" || { echo "usage: make perf-summary PERF_LOG=/path/to/test-abi-perf.log" >&2; exit 1; }
	$(BASH) tests/perf_summary.sh "$(PERF_LOG)"

perf-contract-test-smoke:
	"$(BASH)" tests/perf_contract_smoke.sh

test-concurrency: $(CONCURRENCY_TEST)
	@echo "=== Concurrency Test ==="
	$(CONCURRENCY_TEST)

test-dir: $(DIR_TEST)
	@echo "=== DIR Test ==="
	$(DIR_TEST)

test-air: $(AIR_TEST)
	@echo "=== AIR Test ==="
	$(AIR_TEST)

test-rir: $(RIR_TEST)
	@echo "=== RIR Test ==="
	$(RIR_TEST)

test-mir: $(MIR_TEST)
	@echo "=== MIR Test ==="
	$(MIR_TEST)

test-hir: $(HIR_TEST)
	@echo "=== HIR Test ==="
	$(HIR_TEST)

windows-build-smoke: $(PGY) $(LEXER_TEST) $(PARSER_TEST) $(DATASTRUCTURES_TEST) $(SEMANTIC_TEST) $(TRANSPILE_TEST) \
	$(MEMORY_TEST) $(ABI_TEST) $(ABI_PIPELINE_TEST) $(CONCURRENCY_TEST) $(DIR_TEST) \
	$(AIR_TEST) $(RIR_TEST) $(MIR_TEST) $(HIR_TEST)
	@echo "=== Windows Build Smoke ==="
	@echo "built compiler and frontend/runtime test binaries with CC=$(CC)"

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

llvm-test-abi-same-process: $(ABI_PIPELINE_TEST)
	@echo "=== ABI Pipeline Same-Process LLVM Regression ==="
	PGY_ABI_PIPELINE_SAME_PROCESS=1 \
	PGY_ABI_PIPELINE_BACKEND=llvm \
	"$(ABI_PIPELINE_TEST)"

fmt-test-smoke:
	$(MAKE) $(PGY)
	PGY_BIN="$(abspath $(PGY))" PGY_CC="$(CC)" "$(BASH)" tests/fmt_smoke.sh

tooling-conformance-test-smoke: $(PGY) $(PGY_LSP)
	PGY_BIN="$(abspath $(PGY))" PGY_LSP_BIN="$(abspath $(PGY_LSP))" PGY_CC="$(CC)" "$(BASH)" tests/tooling_conformance_smoke.sh

stdlib-test-smoke:
	$(MAKE) $(PGY)
	PGY_STDLIB_BACKENDS="$${PGY_STDLIB_BACKENDS:-$(STDLIB_BACKENDS)}" \
	PGY_BIN="$(abspath $(PGY))" PGY_CC="$(CC)" "$(BASH)" tests/stdlib_surface_smoke.sh

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
	@missing=0; \
	for src in $(BUILD_SOURCE_INVENTORY_SOURCES) $(BUILD_CONTRACT_INVENTORY_FILES); do \
		if [ ! -f "$$src" ]; then \
			echo "[build-source-inventory] missing required build file: $$src" >&2; \
			missing=1; \
			continue; \
		fi; \
		if command -v git >/dev/null 2>&1 && git check-ignore -q --no-index "$$src" 2>/dev/null; then \
			echo "[build-source-inventory] required build file is ignored: $$src" >&2; \
			missing=1; \
		fi; \
		if [ "$${PGY_REQUIRE_TRACKED_SOURCES:-0}" = "1" ] \
			&& command -v git >/dev/null 2>&1 \
			&& git rev-parse --is-inside-work-tree >/dev/null 2>&1 \
			&& ! git ls-files --error-unmatch "$$src" >/dev/null 2>&1; then \
			echo "[build-source-inventory] required build file is not tracked: $$src" >&2; \
			missing=1; \
		fi; \
	done; \
	if [ "$$missing" -ne 0 ]; then exit 1; fi; \
	echo "[build-source-inventory] Makefile source inventory ok"

observability-schema-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/observability_schema_smoke.sh

memory-concurrency-model-test-smoke: $(PGY)
	PGY_MEMORY_CONCURRENCY_BACKENDS="$${PGY_MEMORY_CONCURRENCY_BACKENDS:-$(MEMORY_CONCURRENCY_BACKENDS)}" \
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/memory_concurrency_model_smoke.sh

async-model-positioning-test-smoke:
	"$(BASH)" tests/async_model_positioning_smoke.sh

documentation-quality-test-smoke:
	"$(BASH)" tests/documentation_quality_smoke.sh

llvm-campaign-projection-test-smoke:
	$(MAKE) LLVM_ENABLED=1 $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/llvm_campaign_projection_smoke.sh

llvm-dnd-campaign-test-smoke:
	$(MAKE) LLVM_ENABLED=1 $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/llvm_dnd_campaign_smoke.sh

beta-readiness-checklist-test-smoke:
	"$(BASH)" tests/beta_readiness_checklist_smoke.sh

formal-semantics-test-smoke:
	"$(BASH)" tests/formal_semantics_smoke.sh

air-drift-test-smoke:
	$(MAKE) test-air
	"$(BASH)" tests/air_drift_smoke.sh

air-backend-nonimpact-test-smoke:
	$(MAKE) LLVM_ENABLED=1 $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/air_backend_nonimpact_smoke.sh

air-backend-nonimpact-full-test-smoke:
	$(MAKE) LLVM_ENABLED=1 $(PGY)
	PGY_BIN="$(abspath $(PGY))" \
	PGY_AIR_NONIMPACT_SOURCE=all \
	"$(BASH)" tests/air_backend_nonimpact_smoke.sh

codegen-determinism-test-smoke:
	$(MAKE) LLVM_ENABLED=1 $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/codegen_determinism_smoke.sh

runtime-none-contract-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/runtime_none_contract_smoke.sh

raw-escape-contract-test-smoke: $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/raw_escape_contract_smoke.sh

semantic-inc-size-test-smoke:
	"$(BASH)" tests/semantic_inc_size_smoke.sh

semantic-tu-size-test-smoke:
	"$(BASH)" tests/semantic_tu_size_smoke.sh

production-header-size-test-smoke:
	"$(BASH)" tests/production_header_size_smoke.sh

backend-inc-size-test-smoke:
	"$(BASH)" tests/backend_inc_size_smoke.sh

test-inc-size-test-smoke:
	"$(BASH)" tests/test_inc_size_smoke.sh

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

runtime-authority-contract-test-smoke:
	"$(BASH)" tests/runtime_authority_contract_smoke.sh

runtime-panic-contract-test-smoke:
	"$(BASH)" tests/runtime_panic_contract_smoke.sh

runtime-panic-abi-test-smoke:
	CC="$(CC)" "$(BASH)" tests/runtime_panic_abi_smoke.sh

runtime-panic-codegen-test-smoke: $(PGY)
	PGY_RUNTIME_PANIC_CODEGEN_BACKENDS="$${PGY_RUNTIME_PANIC_CODEGEN_BACKENDS:-$(RUNTIME_PANIC_CODEGEN_BACKENDS)}" \
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/runtime_panic_codegen_smoke.sh

projection-diagnostic-contract-test-smoke:
	"$(BASH)" tests/projection_diagnostic_contract_smoke.sh

runtime-abi-lifetime-test-smoke:
	"$(BASH)" tests/runtime_abi_lifetime_smoke.sh

abi-ownership-shape-test-smoke:
	"$(BASH)" tests/abi_ownership_shape_smoke.sh

runtime-frontier-contract-test-smoke:
	"$(BASH)" tests/runtime_frontier_contract_smoke.sh

runtime-frontier-policy-test-smoke:
	CC="$(CC)" "$(BASH)" tests/runtime_frontier_policy_smoke.sh

parallel-core-contract-test-smoke:
	"$(BASH)" tests/parallel_core_contract_smoke.sh

parser-lexer-diagnostic-test-smoke:
	"$(BASH)" tests/parser_lexer_diagnostic_smoke.sh

diagnostics-json-test-smoke:
	$(MAKE) $(PGY)
	PGY_BIN="$(abspath $(PGY))" "$(BASH)" tests/diagnostics_json_smoke.sh

llvm-test-backend-compare: $(ABI_PIPELINE_TEST)
	$(MAKE) LLVM_ENABLED=1 $(PGY)
	PGY_BIN="$(abspath $(PGY))" \
	PGY_CC="$(CC)" \
	PGY_ABI_PIPELINE_TEST_BIN="$(abspath $(ABI_PIPELINE_TEST))" \
	LLVM_INSTALL="$(LLVM_INSTALL)" \
	PGY_BACKEND_COMPARE_PRECHECK_SAME_PROCESS=1 \
	"$(BASH)" tests/compare_backends.sh

air-strict-backend-compare-test-smoke: $(ABI_PIPELINE_TEST)
	$(MAKE) LLVM_ENABLED=1 $(PGY)
	PGY_AIR_STRICT_EVIDENCE=1 \
	PGY_BIN="$(abspath $(PGY))" \
	PGY_CC="$(CC)" \
	PGY_ABI_PIPELINE_TEST_BIN="$(abspath $(ABI_PIPELINE_TEST))" \
	LLVM_INSTALL="$(LLVM_INSTALL)" \
	PGY_BACKEND_COMPARE_PRECHECK_SAME_PROCESS=1 \
	"$(BASH)" tests/compare_backends.sh

example-test-smoke:
	$(MAKE) $(PGY)
	PGY_BIN="$(abspath $(PGY))" PGY_CC="$(CC)" "$(BASH)" tests/example_contract_smoke.sh

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

ci-linux:
	$(MAKE) check-linux-toolchain
	$(MAKE) build-source-inventory-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" clean
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" test-all
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" llvm-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" fmt-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" tooling-conformance-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" stdlib-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" module-test-smoke
	$(MAKE) module-taxonomy-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" package-module-resolver-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" unicode-policy-test-smoke
	$(MAKE) beta-test-suite-freeze-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" observability-schema-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" memory-concurrency-model-test-smoke
	$(MAKE) documentation-quality-test-smoke
	$(MAKE) beta-readiness-checklist-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" runtime-none-contract-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" raw-escape-contract-test-smoke
	$(MAKE) formal-semantics-test-smoke
	$(MAKE) air-drift-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" air-backend-nonimpact-full-test-smoke
	$(MAKE) semantic-inc-size-test-smoke
	$(MAKE) semantic-tu-size-test-smoke
	$(MAKE) production-header-size-test-smoke
	$(MAKE) backend-inc-size-test-smoke
	$(MAKE) test-inc-size-test-smoke
	$(MAKE) inc-sentinel-test-smoke
	$(MAKE) semantic-core-shape-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" type-resolution-dag-test-smoke
	$(MAKE) type-resolution-resolver-inventory-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" semantic-fixture-isolation-test-smoke
	$(MAKE) diagnostic-registry-test-smoke
	$(MAKE) runtime-authority-contract-test-smoke
	$(MAKE) runtime-panic-contract-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" runtime-panic-abi-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" runtime-panic-codegen-test-smoke
	$(MAKE) projection-diagnostic-contract-test-smoke
	$(MAKE) runtime-abi-lifetime-test-smoke
	$(MAKE) abi-ownership-shape-test-smoke
	$(MAKE) runtime-frontier-contract-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" runtime-frontier-policy-test-smoke
	$(MAKE) parallel-core-contract-test-smoke
	$(MAKE) perf-contract-test-smoke
	$(MAKE) parser-lexer-diagnostic-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" diagnostics-json-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" ir-pipeline-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" cfg-body-dataflow-test-smoke
	$(MAKE) ast-dispatch-test-smoke
	$(MAKE) mir-declaration-inventory-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" example-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" llvm-test-abi-same-process
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" llvm-test-backend-compare
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" llvm-campaign-projection-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" llvm-dnd-campaign-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" air-strict-backend-compare-test-smoke
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" clean
	$(MAKE) CC="$(CI_LINUX_CC)" BUILD_DIR="$(CI_LINUX_BUILD_DIR)" BIN_DIR="$(CI_LINUX_BIN_DIR)" test-all

ci-macos:
	$(MAKE) check-macos-toolchain
	$(MAKE) build-source-inventory-test-smoke
	$(MAKE) CC="$(CI_MACOS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_MACOS_BUILD_DIR)" BIN_DIR="$(CI_MACOS_BIN_DIR)" clean
	$(MAKE) CC="$(CI_MACOS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_MACOS_BUILD_DIR)" BIN_DIR="$(CI_MACOS_BIN_DIR)" test-all
	PGY_STDLIB_BACKENDS=c $(MAKE) CC="$(CI_MACOS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_MACOS_BUILD_DIR)" BIN_DIR="$(CI_MACOS_BIN_DIR)" fmt-test-smoke
	PGY_STDLIB_BACKENDS=c $(MAKE) CC="$(CI_MACOS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_MACOS_BUILD_DIR)" BIN_DIR="$(CI_MACOS_BIN_DIR)" tooling-conformance-test-smoke
	PGY_STDLIB_BACKENDS=c $(MAKE) CC="$(CI_MACOS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_MACOS_BUILD_DIR)" BIN_DIR="$(CI_MACOS_BIN_DIR)" stdlib-test-smoke
	$(MAKE) module-taxonomy-test-smoke
	$(MAKE) CC="$(CI_MACOS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_MACOS_BUILD_DIR)" BIN_DIR="$(CI_MACOS_BIN_DIR)" package-module-resolver-test-smoke
	PGY_UNICODE_BACKENDS=c $(MAKE) CC="$(CI_MACOS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_MACOS_BUILD_DIR)" BIN_DIR="$(CI_MACOS_BIN_DIR)" unicode-policy-test-smoke
	$(MAKE) beta-test-suite-freeze-test-smoke
	PGY_OBSERVABILITY_BACKENDS=c $(MAKE) CC="$(CI_MACOS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_MACOS_BUILD_DIR)" BIN_DIR="$(CI_MACOS_BIN_DIR)" observability-schema-test-smoke
	PGY_MEMORY_CONCURRENCY_BACKENDS=c $(MAKE) CC="$(CI_MACOS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_MACOS_BUILD_DIR)" BIN_DIR="$(CI_MACOS_BIN_DIR)" memory-concurrency-model-test-smoke
	$(MAKE) documentation-quality-test-smoke
	$(MAKE) beta-readiness-checklist-test-smoke
	$(MAKE) CC="$(CI_MACOS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_MACOS_BUILD_DIR)" BIN_DIR="$(CI_MACOS_BIN_DIR)" runtime-none-contract-test-smoke
	$(MAKE) CC="$(CI_MACOS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_MACOS_BUILD_DIR)" BIN_DIR="$(CI_MACOS_BIN_DIR)" raw-escape-contract-test-smoke
	$(MAKE) formal-semantics-test-smoke
	$(MAKE) perf-contract-test-smoke
	$(MAKE) test-inc-size-test-smoke
	$(MAKE) CC="$(CI_MACOS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_MACOS_BUILD_DIR)" BIN_DIR="$(CI_MACOS_BIN_DIR)" diagnostics-json-test-smoke
	$(MAKE) CC="$(CI_MACOS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_MACOS_BUILD_DIR)" BIN_DIR="$(CI_MACOS_BIN_DIR)" cfg-body-dataflow-test-smoke
	$(MAKE) CC="$(CI_MACOS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_MACOS_BUILD_DIR)" BIN_DIR="$(CI_MACOS_BIN_DIR)" semantic-fixture-isolation-test-smoke
	$(MAKE) parser-lexer-diagnostic-test-smoke

check-windows-toolchain:
	@cc_machine="$$( $(CI_WINDOWS_CC) -dumpmachine 2>/dev/null || true )"; \
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
	$(MAKE) check-windows-toolchain
	$(MAKE) build-source-inventory-test-smoke
	$(MAKE) CC="$(CI_WINDOWS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_WINDOWS_BUILD_DIR)" BIN_DIR="$(CI_WINDOWS_BIN_DIR)" clean
	$(MAKE) beta-test-suite-freeze-test-smoke
	$(MAKE) documentation-quality-test-smoke
	$(MAKE) beta-readiness-checklist-test-smoke
	@if [ "$(CI_WINDOWS_RUNNABLE)" = "1" ]; then \
		echo "ci-windows: native MSYS2 runtime detected; running executable smoke/tests"; \
		$(MAKE) CC="$(CI_WINDOWS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_WINDOWS_BUILD_DIR)" BIN_DIR="$(CI_WINDOWS_BIN_DIR)" test-all; \
		PGY_STDLIB_BACKENDS=c $(MAKE) CC="$(CI_WINDOWS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_WINDOWS_BUILD_DIR)" BIN_DIR="$(CI_WINDOWS_BIN_DIR)" fmt-test-smoke; \
		PGY_STDLIB_BACKENDS=c $(MAKE) CC="$(CI_WINDOWS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_WINDOWS_BUILD_DIR)" BIN_DIR="$(CI_WINDOWS_BIN_DIR)" stdlib-test-smoke; \
		$(MAKE) CC="$(CI_WINDOWS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_WINDOWS_BUILD_DIR)" BIN_DIR="$(CI_WINDOWS_BIN_DIR)" runtime-none-contract-test-smoke; \
		$(MAKE) CC="$(CI_WINDOWS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_WINDOWS_BUILD_DIR)" BIN_DIR="$(CI_WINDOWS_BIN_DIR)" raw-escape-contract-test-smoke; \
		$(MAKE) CC="$(CI_WINDOWS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_WINDOWS_BUILD_DIR)" BIN_DIR="$(CI_WINDOWS_BIN_DIR)" semantic-fixture-isolation-test-smoke; \
		PGY_EXAMPLE_BACKENDS=c $(MAKE) CC="$(CI_WINDOWS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_WINDOWS_BUILD_DIR)" BIN_DIR="$(CI_WINDOWS_BIN_DIR)" example-test-smoke; \
		$(MAKE) test-inc-size-test-smoke; \
	elif echo "$(CI_WINDOWS_CC_MACHINE)" | grep -qi 'mingw'; then \
		echo "ci-windows: cross MinGW toolchain detected; running build-only smoke"; \
		$(MAKE) CC="$(CI_WINDOWS_CC)" LLVM_ENABLED=0 BUILD_DIR="$(CI_WINDOWS_BUILD_DIR)" BIN_DIR="$(CI_WINDOWS_BIN_DIR)" windows-build-smoke; \
	else \
		echo "ci-windows: unable to run or cross-build Windows artifacts with CC=$(CI_WINDOWS_CC)"; \
		exit 1; \
	fi
	@if [ "$(CI_WINDOWS_RUNNABLE)" = "1" ]; then \
		if [ "$(WINDOWS_LLVM_READY)" = "1" ]; then \
			echo "ci-windows: LLVM toolchain detected; running LLVM smoke and backend compare"; \
			$(MAKE) CC="$(CI_WINDOWS_CC)" LLVM_ENABLED=1 BUILD_DIR="$(CI_WINDOWS_BUILD_DIR)" BIN_DIR="$(CI_WINDOWS_BIN_DIR)" llvm-test-smoke; \
			$(MAKE) CC="$(CI_WINDOWS_CC)" LLVM_ENABLED=1 BUILD_DIR="$(CI_WINDOWS_BUILD_DIR)" BIN_DIR="$(CI_WINDOWS_BIN_DIR)" llvm-test-backend-compare; \
		else \
			echo "ci-windows: LLVM toolchain not detected; skipping Windows LLVM smoke/backend compare"; \
		fi; \
	fi

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
	rm -rf $(BUILD_DIR) $(BIN_DIR)

clean-objects:
	find $(BUILD_DIR) -name "*.o" -delete 2>/dev/null || true

# Force a full rebuild from scratch.  Use when source edits aren't
# reflected (stale .o, broken .d, CONFIG_STAMP mismatch, etc).
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

.PHONY: all clean clean-objects rebuild debug release analyze format memcheck \
        test test-parser test-datastructures test-security test-semantic test-transpile test-memory test-abi test-concurrency test-dir test-air test-rir test-mir test-hir test-all \
llvm-test llvm-test-parser llvm-test-semantic llvm-test-transpile llvm-test-memory llvm-test-concurrency llvm-test-dir llvm-test-rir llvm-test-mir llvm-test-hir llvm-test-backend-compare llvm-test-all llvm-test-smoke tooling-conformance-test-smoke stdlib-test-smoke module-test-smoke module-taxonomy-test-smoke package-module-resolver-test-smoke unicode-policy-test-smoke beta-test-suite-freeze-test-smoke build-source-inventory-test-smoke observability-schema-test-smoke memory-concurrency-model-test-smoke async-model-positioning-test-smoke documentation-quality-test-smoke llvm-campaign-projection-test-smoke llvm-dnd-campaign-test-smoke beta-readiness-checklist-test-smoke formal-semantics-test-smoke air-drift-test-smoke air-backend-nonimpact-test-smoke air-backend-nonimpact-full-test-smoke air-strict-backend-compare-test-smoke codegen-determinism-test-smoke runtime-none-contract-test-smoke raw-escape-contract-test-smoke semantic-inc-size-test-smoke semantic-tu-size-test-smoke production-header-size-test-smoke backend-inc-size-test-smoke test-inc-size-test-smoke semantic-core-shape-test-smoke type-resolution-dag-test-smoke type-resolution-resolver-inventory-test-smoke semantic-fixture-isolation-test-smoke diagnostic-registry-test-smoke runtime-authority-contract-test-smoke runtime-panic-contract-test-smoke runtime-panic-abi-test-smoke runtime-panic-codegen-test-smoke projection-diagnostic-contract-test-smoke runtime-abi-lifetime-test-smoke abi-ownership-shape-test-smoke runtime-frontier-contract-test-smoke runtime-frontier-policy-test-smoke parallel-core-contract-test-smoke perf-contract-test-smoke parser-lexer-diagnostic-test-smoke diagnostics-json-test-smoke cfg-body-dataflow-test-smoke mir-declaration-inventory-test-smoke example-test-smoke ast-dispatch-test-smoke ci-linux ci-macos ci-windows check-linux-toolchain check-macos-toolchain check-windows-toolchain \
        example-hello example-slots llvm emit-llvm-% lsp

ifeq ($(filter clean clean-objects,$(MAKECMDGOALS)),)
-include $(shell find $(BUILD_DIR) -name "*.d" 2>/dev/null)
endif
