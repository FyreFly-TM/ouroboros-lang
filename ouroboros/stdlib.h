#ifndef STDLIB_H
#define STDLIB_H

#include "parser.h"

void register_stdlib_functions();
int call_builtin_function(const char *name, const char **args, int arg_count);
void set_call_args(const char **args, int count);

#endif // STDLIB_H
