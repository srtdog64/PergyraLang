/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * pgy_runtime_linkage.h -- runtime function linkage mode (inline->extern
 * workstream, docs/189 C14 / WO-RED2).
 *
 * Runtime builtins are authored once, as bodies, in the *_inline.h headers.
 * The storage class those bodies take is chosen at compile time so the SAME
 * source serves three consumers without drift:
 *
 *   default (no macro defined)   static inline   Self-contained emission: the
 *                                                LLVM-generated C, the tests,
 *                                                and any TU that wants the body
 *                                                inlined in place. This is the
 *                                                historical behaviour and stays
 *                                                byte-identical.
 *
 *   PGY_RUNTIME_DECLS_ONLY       extern          The C backend's emitted TU in
 *                                                extern mode: it parses only the
 *                                                prototype and links the runtime
 *                                                object, so the ~9k lines of
 *                                                inline bodies leave the per-TU
 *                                                parse -- the measured 91% of
 *                                                emitted-C compile time.
 *
 *   PGY_RUNTIME_EXTERN_DEFS      (none: extern   The runtime object TU: exactly
 *                                linkage def)    one external-linkage definition
 *                                                the emitted TU links against.
 *
 * The prototype (DECLS_ONLY) and the definition (EXTERN_DEFS / default) are cut
 * from the ONE signature in the ONE header, so the ABI at the emitted call site
 * and the ABI of the linked body cannot diverge.
 *
 * A converted function is written:
 *
 *     PGY_RT_DECL <ret> Name(<params>)
 *     #ifndef PGY_RUNTIME_DECLS_ONLY
 *     { <body> }
 *     #else
 *     ;
 *     #endif
 *
 * The body is guarded by a real preprocessor conditional -- never passed
 * through a function-like macro -- so commas, braces, string literals, and even
 * nested #if directives inside the body need no escaping.
 */

#ifndef PGY_RUNTIME_LINKAGE_H
#define PGY_RUNTIME_LINKAGE_H

#if defined(PGY_RUNTIME_DECLS_ONLY) && defined(PGY_RUNTIME_EXTERN_DEFS)
#error "PGY_RUNTIME_DECLS_ONLY and PGY_RUNTIME_EXTERN_DEFS are mutually exclusive"
#endif

#if defined(PGY_RUNTIME_DECLS_ONLY)
#  define PGY_RT_DECL extern
#elif defined(PGY_RUNTIME_EXTERN_DEFS)
#  define PGY_RT_DECL /* external-linkage definition */
#else
#  define PGY_RT_DECL static inline
#endif

#endif /* PGY_RUNTIME_LINKAGE_H */
