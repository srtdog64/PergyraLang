/*
 * Internal LSP helpers split by owner. This is intentionally private to
 * src/lsp so the public compiler surface does not grow around tooling glue.
 */

#ifndef PERGYRA_PGY_LSP_INTERNAL_H
#define PERGYRA_PGY_LSP_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "../parser/ast.h"

extern char lsp_response_buf[65536];
extern const char *lsp_completion_items;

bool json_find_string_copy(const char *json, const char *key,
                           char *out, size_t out_size);
char *json_find_string_dup(const char *json, const char *key);
int json_find_int(const char *json, const char *key);
void json_escape_copy(char *dst, size_t dst_size, const char *src);

void lsp_send(const char *body);
void lsp_respond(int id, const char *result_json);
void lsp_notify(const char *method, const char *params_json);

bool extract_word_at_position(const char *source_text, int line, int character,
                              char *out_word, size_t out_word_size);
bool is_word_boundary_char(char c);

void respond_document_symbols(int id, const char *source_text);
void respond_definition(int id, const char *uri, const char *source_text,
                        int line, int character);
void respond_references(int id, const char *uri, const char *source_text,
                        int line, int character);
void respond_rename(int id, const char *uri, const char *source_text,
                    int line, int character, const char *new_name);
void respond_hover(int id, const char *source_text, int line, int character);
bool lsp_build_diagnostics_params(const char *uri, const char *source_text,
                                  char *params, size_t params_size);
void publish_diagnostics(const char *uri, const char *source_text);

#endif /* PERGYRA_PGY_LSP_INTERNAL_H */
