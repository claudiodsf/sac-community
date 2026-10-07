/*
 * win/compat/bison_trace.c -- no-op bison trace helpers.
 *
 * src/eval/expr_parse.c is committed generated code and calls ParseTrace() and
 * ParseNoOpTrace() unconditionally.  Bison only emits those helpers when the
 * parser is built with tracing enabled (YYDEBUG / %define parse.trace), which
 * is not the case here, so the symbols would otherwise be undefined.
 *
 * Rather than regenerate or hand-patch the generated parser, provide the
 * symbols as no-ops: they only ever produce debugging output.
 */
#ifdef _WIN32

#include <stdarg.h>
#include <stdio.h>

void
ParseTrace(FILE *stream, const char *fmt, ...)
{
    va_list ap;

    (void) stream;
    (void) fmt;

    va_start(ap, fmt);
    va_end(ap);
}

void
ParseNoOpTrace(FILE *stream, const char *fmt, ...)
{
    va_list ap;

    (void) stream;
    (void) fmt;

    va_start(ap, fmt);
    va_end(ap);
}

#endif /* _WIN32 */
