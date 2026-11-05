#pragma once
#include <stdbool.h>
#include <stddef.h>
// Extract at most two complete sentences that fit the actual badge layout.
// Parenthetical pronunciation/unit conversions and citation brackets are removed.
// No generated claims, partial words, or ellipses are introduced.
bool summarize_extract(const char *title,const char *extract,char *out,size_t capacity);

void clean_fact_text(const char *source,char *out,size_t capacity);
