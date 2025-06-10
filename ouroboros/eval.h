#ifndef EVAL_H
#define EVAL_H

#include "stack.h"
#include "ast_types.h"

const char* evaluate_expression(ASTNode *expr, StackFrame *frame);
const char* call_user_function(const char *name, int arg_count, const char **args);

#endif // EVAL_H
