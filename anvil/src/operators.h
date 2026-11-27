#pragma once

#include <anvil/anvil_plugin.h>

#include "accession.h"
#include "ps_script.h"

typedef struct PsQueryState {
    unsigned leader;
    int authority;
    bool passed;
} PsQueryState;

typedef enum PsQueryResult {
    PS_QUERY_INACTIVE, PS_QUERY_READY, PS_QUERY_UNKNOWN_REFORM, PS_QUERY_UNKNOWN_INSTANCE, PS_QUERY_UNKNOWN_RESOURCE,
} PsQueryResult;

typedef struct OperatorOrigin {
    const char *path;
    unsigned line;
} OperatorOrigin;

typedef struct OperatorState {
    void (*load_aliases)(PsAliases *output);
    bool (*instance_known)(const char *id);
    const char *(*resolve_type)(const char *type, char instance[GK_SCRIPT_WORD_SIZE]);
    PsQueryResult (*query)(const PsArguments *args, PsQueryState *output);
    const char *(*execute)(PsOperation operation, const PsArguments *args, unsigned actor);
    void (*log)(AnvilLogLevel level, const char *text);
    const char *(*election_query)(PsOperation operation, const PsArguments *args, unsigned actor, bool *result);
    const char *(*member_query)(const PsArguments *args, unsigned actor, bool *result);
    const char *(*structure_query)(PsOperation operation, const PsArguments *args, unsigned actor, bool *result);
    const char *(*resource_query)(PsOperation operation, const PsArguments *args, unsigned actor, bool *result);
    const char *(*action_query)(PsOperation operation, const PsArguments *args, unsigned actor, bool *result);
    const char *(*lifecycle_query)(const PsArguments *args, bool *result);
    const char *(*war_query)(unsigned attacker, unsigned defender, bool *result);
    bool (*describe)(PsOperation operation, const PsArguments *args, unsigned country, const char *key, GameString *out);
} OperatorState;

AnvilResult register_operators(const AnvilHost *host, AnvilPlugin *self, OperatorState callbacks);
const char *ps_test_condition(PsOperation op, const PsArguments *args, unsigned actor, bool *result);
const char *operators_instance(const char *instance);
OperatorOrigin operators_origin(OperatorOrigin origin);
unsigned native_reader_line(const void *reader);
