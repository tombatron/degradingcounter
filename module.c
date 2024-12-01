#include "redismodule.h"

#define DEGRADING_COUNTER_TYPE_NAME "DeGrad-TB"
#define DEGRADING_COUNTER_ENCODING_VERSION 0
#define DEGRADING_COUNTER_MODULE_VERSION 1

// This is a static global pointer to the custom type defined for the degrading counter.
static RedisModuleType *DegradingCounter;

typedef enum CounterIncrements {
    Milliseconds = 0,
    Seconds = 1,
    Minutes = 2
} CounterIncrements;

typedef struct DegradingCounterData {
    ustime_t created; // Time stamp in which the counter was created.
    double degrades_at; // How much should the counter degrade after an increment has passed. e.g. 1 every millisecond, or .5 every minute.
    int number_of_increments; // How many increments should elapse before degrading the counter? Defaults to 1.
    CounterIncrements increment; // Which time increment should be used to degrade the counter?
    double value; // What is accumulated value of the counter? This will be a raw "undegraded" number. Only increments and decrements apply here.
} DegradingCounterData;

double DegradingCounter_ComputeMilliseconds(const DegradingCounterData *counter) {
    return 0;
}

double DegradingCounter_ComputeSeconds(const DegradingCounterData *counter) {
    return 0;
}

double DegradingCounter_ComputeMinutes(const DegradingCounterData *counter) {
    return 0;
}

// ------- Commands

// Increment counter (DC.INCR): Create counter if it doesn't exist increment by specified amount of values if it does.
//                    Gonna set the rest of the properties.
int DegradingCounterIncrement_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return REDISMODULE_OK;
}

// Decrement counter (DC.DECR): Provide a way for a user to decrement a counter.
int DegradingCounterDecrement_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return REDISMODULE_OK;
}

// Peek counter (DC.PEEK): look at the current value of the counter without incrementing it.
int DegradingCounterPeek_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return REDISMODULE_OK;
}

// ------- Native Type Callbacks.

// Provided as the `rdb_load` callback for our data type.
void *DegradingCounterRDBLoad(RedisModuleIO *io, int encver) {

}

// Provided as the `rdb_save` callback for our data type.
void *DegradingCounterRDBSave(RedisModuleIO *io, void *ptr) {

}

// Provided as the `aof_rewrite` callback for our data type.
void DegradingCounterAOFRewrite(RedisModuleIO *aof, RedisModuleString *key, void *value) {

}

// Provided as the `free` callback for our data type.
void DegradingCounterFree(void *value) {
    // This should suffice as our data type doesn't require a complex structure.
    RedisModule_Free(value);
}

int RedisModule_OnLoad(RedisModuleCtx *ctx) {
    if (RedisModule_Init(ctx, DEGRADING_COUNTER_TYPE_NAME, DEGRADING_COUNTER_MODULE_VERSION, REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleTypeMethods tm = {
        .version = REDISMODULE_TYPE_METHOD_VERSION,
        .rdb_load = DegradingCounterRDBLoad,
        .rdb_save = DegradingCounterRDBSave,
        .aof_rewrite = DegradingCounterAOFRewrite,
        .free = DegradingCounterFree
    };

    DegradingCounter = RedisModule_CreateDataType(ctx,
        DEGRADING_COUNTER_TYPE_NAME,
        DEGRADING_COUNTER_ENCODING_VERSION,
        &tm);

    if (DegradingCounter == NULL) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "dc.incr",
        DegradingCounterIncrement_RedisCommand,"fast write", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "dc.decr",
        DegradingCounterDecrement_RedisCommand,"fast write", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "dc.peek",
        DegradingCounterPeek_RedisCommand,"fast write", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}