#include "redismodule.h"
#include <string.h>
#include <math.h>

#define DEGRADING_COUNTER_TYPE_NAME "DeGrad-TB"
#define DEGRADING_COUNTER_ENCODING_VERSION 0
#define DEGRADING_COUNTER_MODULE_VERSION 1

#define MILLISECONDS_PER_MILLISECOND 1 // Hehe.
#define MILLISECONDS_PER_SECOND 1000
#define MILLISECONDS_PER_MINUTE 60000

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
    double value; // What is accumulated value of the counter? This will be a raw "un-degraded" number. Only increments and decrements apply here.
} DegradingCounterData;

// This method will compute the degraded value of the counter.
double DegradingCounter_Compute_Value(const DegradingCounterData *counter) {
    // Get current time stamp.
    const ustime_t current_time_ms = RedisModule_Milliseconds();

    // Compute the difference. This will give us our age.
    const ustime_t age_in_milliseconds = current_time_ms - counter->created;

    // Determine units per increment.
    long long units_per_increment = 0;

    switch (counter->increment) {
        case Milliseconds:
            units_per_increment = MILLISECONDS_PER_MILLISECOND;
            break;

        case Seconds:
            units_per_increment = MILLISECONDS_PER_SECOND;
            break;

        case Minutes:
            units_per_increment = MILLISECONDS_PER_MINUTE;
            break;

        default:
            // Not sure how we got here, but we should just leave immediately.
            return 0;
    }

    // TODO: Add in some checks to ensure that we're not overflowing anywhere.

    // Compute the number of increments by dividing the age.
    const long long number_of_increments = age_in_milliseconds / units_per_increment;

    // Multiply the number of increments by how fast the counter is degrading to figure out degradation.
    const double degradation = (double)number_of_increments * counter->degrades_at;

    // Subtract the degradation from the value. Clamp the value at zero.
    const double degraded_value = fmax(0, counter->value - degradation);

    // Return the result.
    return degraded_value;
}

int DegradingCounter_ParseIntervalString(const char *interval_str, int &number_of_increments, CounterIncrements &unit) {
    char unit_str[4]; // This will hold the `min`, `sec`, `ms` component of the interval string.

    if (sscanf(interval_str, "%d%4s", number_of_increments, unit_str) != 2) { // NOLINT(*-err34-c), At this point I don't care why parsing failed.
        // TODO: Maybe start caring?
        return -1;
    }

    if (strcmp(unit_str, "ms") == 0) {
        unit = Milliseconds;
        return 0;
    }

    if (strcmp(unit_str, "sec") == 0) {
        unit = Seconds;
        return 0;
    }

    if (strcmp(unit_str, "min") == 0) {
        unit = Minutes;
        return 0;
    }

    // We made it here, the input must have been invalid. We'll leave it to the caller to say why and report the error.
    return -1;
}

// Create a struct of type DegradingCounterData and populate it from the arguments passed into the Redis command.
DegradingCounterData* GetDegradingCounterDataFromRedisArguments(RedisModuleCtx* ctx, RedisModuleString **argv) {
    // This method is intended to be called from within a context that has already checked the number of arguments.

    // Let's start by having Redis allocate enough memory for us to store would DegradingCounterData struct. Allocating
    // the memory this way let's Redis correctly report how much memory it's using.
    // TODO: Check to ensure that the memory was allocated successfully.
    DegradingCounterData *degrading_counter_data = RedisModule_Alloc(sizeof(DegradingCounterData));

    // Now that the memory for the degrading_counter_data struct instance is allocated we'll go and parse the arguments and
    // hopefully return a pointer to the struct containing the data we want to work with.

    // I don't think we should require the arguments to be in a specific order, as long as everything is provided it should
    // be fine. So we'll loop over the arguments, which should have already been validated to ensure that exact six
    // were provided.
    // TODO: Should make sure AMOUNT, DEGRADE_RATE, and INTERVAL were each passed in.
    for (int i = 2; i < 6; i += 2) { // Adding two so that the next index will reference a name.
        size_t arg_len;
        const char *arg_name = RedisModule_StringPtrLen(argv[i], &arg_len); // Doesn't need to be freed as it's handled by Redis.

        // Check `AMOUNT`
        if (strcmp(arg_name, "AMOUNT") == 0) {
            if (RedisModule_StringToDouble(argv[i + 1], &degrading_counter_data->value) != REDISMODULE_OK) {
                RedisModule_ReplyWithError(ctx, "ERR invalid value for AMOUNT: must be a signed double.");
                RedisModule_Free(degrading_counter_data);
                return NULL;
            }
        }

        // Check `DEGRADE_RATE`
        else if (strcmp(arg_name, "DEGRADE_RATE") == 0) {
            if (RedisModule_StringToDouble(argv[i + 1], &degrading_counter_data->degrades_at) != REDISMODULE_OK) {
                RedisModule_ReplyWithError(ctx, "ERR invalid value for DEGRADE_RATE: must be a signed double.");
                RedisModule_Free(degrading_counter_data);
                return NULL;
            }
        }

        // Check `INTERVAL`
        else if (strcmp(arg_name, "INTERVAL") == 0) {
            size_t interval_len;
            const char *interval_str = RedisModule_StringPtrLen(argv[i + 1], &interval_len);

            if (DegradingCounter_ParseIntervalString(interval_str,
                                                     degrading_counter_data->number_of_increments,
                                                     degrading_counter_data->increment) != 0) {

                RedisModule_ReplyWithErrorFormat(ctx, "Err invalid value for INTERVAL: %s", interval_str);
                RedisModule_Free(degrading_counter_data);
                return NULL;
            }
        }

        // Got something else...
        else {
            RedisModule_Free(degrading_counter_data); // This is in a bad state, and we don't need it.
            RedisModule_ReplyWithErrorFormat(ctx, "ERR unexpected argument: %s. (Remember argument names are case sensitive.)", arg_name);

            return NULL; // There is nothing to return. This will be handled by the caller.
        }
    }

    // We've made it this far... I'm assuming that there are no issues so we're going to return the pointer to the
    // caller, where we expect the instance of the struct to be used and then freed.
    return degrading_counter_data;
}

// ------- Commands

// Increment counter (DC.INCR): Create counter if it doesn't exist increment by specified amount of values if it does.
//                    Gonna set the rest of the properties.

// DC.INCR test_counter AMOUNT 1 DEGRADE_RATE 1.0 INTERVAL 30min
int DegradingCounterIncrement_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    // TODO: Make some of these arguments optional.
    RedisModule_AutoMemory(ctx); // Enable the use of automatic memory management.

    // For now all arguments are required, we'll pass back an error in the event that 6 arguments weren't
    // passed in.
    if (argc != 8) { // 6 user supplied arguments, plus two more for the command name and key name.
        return RedisModule_WrongArity(ctx);
    }

    // Get the key from the argument list.
    RedisModuleString *key_name = argv[1];

    // Get a reference to the actual Redis key handle.
    RedisModuleKey *key = RedisModule_OpenKey(ctx, key_name, REDISMODULE_READ|REDISMODULE_WRITE);

    // Get the key type, this will tell us if the key we are working with is already set or not.
    const int key_type = RedisModule_KeyType(key);

    // Let's make sure we're dealing with the correct kind of key first.
    if (key_type != REDISMODULE_KEYTYPE_EMPTY &&
        RedisModule_ModuleTypeGetType(key) != DegradingCounter) {
        // The wrong type of key was specified so we're bailing out with a wrong type error message.
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    // Next, if possible, let's parse the args passed into the Redis command and see what we have.
    DegradingCounterData *degrading_counter_data = GetDegradingCounterDataFromRedisArguments(ctx, argv);

    // If `degrading_counter_data` is NULL here, there was an error parsing the values and we should just bail out.
    if (degrading_counter_data == NULL) {
        // All we have to do is return an error here, the error has already been sent to the caller.
        return REDISMODULE_ERR;
    }

    if (key_type == REDISMODULE_KEYTYPE_EMPTY) {
        // We have a new key here. Let's persist and return the starting value.
        RedisModule_ModuleTypeSetValue(key, DegradingCounter, degrading_counter_data);

        // We're just going to return the initial value on first save.
        RedisModule_ReplyWithDouble(ctx, degrading_counter_data->value);
    } else {
        // We have an existing key, let's get access to it.
        DegradingCounterData *stored_degrading_counter_data = RedisModule_ModuleTypeGetValue(key);

        // We pull a reference to the memory that is holding our existing key and increment the `value` property by the
        // amount from the passed in argument.
        stored_degrading_counter_data->value += degrading_counter_data->value;

        // TODO: If the result of the above operation results in a value that is less than or equal to zero then we
        //       should go ahead and remove the key from the keyspace.

        // Next, let's compute how much of our counter has degraded.
        const double degraded_counter_value = DegradingCounter_Compute_Value(stored_degrading_counter_data);

        // Finally, return the degraded counter value.
        RedisModule_ReplyWithDouble(ctx, degraded_counter_value);
    }

    // Mark the key ready to replicate to secondaries or to an AOF file...
    RedisModule_ReplicateVerbatim(ctx);

    return REDISMODULE_OK;
}

// Decrement counter (DC.DECR): Provide a way for a user to decrement a counter.
// DC.DECR test_counter 1
int DegradingCounterDecrement_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc > 3) { // 1 optional user supplied argument, plus the command name and the key name.
        return RedisModule_WrongArity(ctx);
    }

    // Get the key from the argument list.
    RedisModuleString *key_name = argv[1];

    // Get a reference to the actual Redis key handle.
    RedisModuleKey *key = RedisModule_OpenKey(ctx, key_name, REDISMODULE_READ|REDISMODULE_WRITE);

    // Get the key type, this will tell us if the key we are working with is already set or not.
    const int key_type = RedisModule_KeyType(key);

    // Make sure that we're dealing with the correct kind key here.
    if (key_type != REDISMODULE_KEYTYPE_EMPTY &&
        RedisModule_ModuleTypeGetType(key) != DegradingCounter) {
        // The wrong type of key was specified so we're bailing out with a wrong type error message.
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    // The key doesn't exist at all so...
    if (key_type == REDISMODULE_KEYTYPE_EMPTY) {
        return RedisModule_ReplyWithNull(ctx);
    }

    double decrement_amount;

    // If we have three arguments we're assuming that the user isn't using the default value.
    if (argc == 3) {
        if (RedisModule_StringToDouble(argv[2], &decrement_amount) != REDISMODULE_OK) {
            // We were passed a bad value, bail!
            RedisModule_ReplyWithError(ctx, "ERR invalid value for decrement: must be a number.");
            return REDISMODULE_ERR;
        }
    }
    // We didn't get a third argument, so we're assuming that the user is using the default value.
    else {
        decrement_amount = 1;
    }

    // Let's go ahead and get the counter from memory.
    DegradingCounterData *stored_degrading_counter_data = RedisModule_ModuleTypeGetValue(key);

    // Decrement the value, clamping at zero.
    double decremented_final_value = fmax(0, stored_degrading_counter_data->value - decrement_amount);

    // If decremented_final_value is 0, we're deleting the key.
    if (decremented_final_value == 0) {
        RedisModule_UnlinkKey(key); // The value can't degrade anymore so we'll remove the method.
    }
    // Otherwise we update the value in memory.
    else {
        // Update the stored value.
        stored_degrading_counter_data->value = decremented_final_value;

        // We've decremented the value, now we have to compute.
        decremented_final_value = DegradingCounter_Compute_Value(stored_degrading_counter_data);
    }

    // Mark the key ready to replicate to secondaries or to an AOF file...
    RedisModule_ReplicateVerbatim(ctx);

    return RedisModule_ReplyWithDouble(ctx, decremented_final_value);
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