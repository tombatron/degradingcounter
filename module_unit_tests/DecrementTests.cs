using StackExchange.Redis;

namespace module_unit_tests;

[Collection("Module Test Collection")]
public class DecrementTests(RedisContainerFixture redisFixture)
{
    private readonly IDatabase _redis = redisFixture.Redis!.GetDatabase();

    [Fact]
    public async Task ItCanDecrementAnExistingCounter()
    {
        var testKey = CreateTestKey();
        
        await _redis.ExecuteAsync(ModuleCommand.Increment, testKey, "AMOUNT", 100.0, "DEGRADE_RATE", 1.0, "INTERVAL", "60min");

        var randomDecrementValue = GetRandomDouble(1.0, 100.0);

        var result = (double)await _redis.ExecuteAsync(ModuleCommand.Decrement, testKey, randomDecrementValue);
        var expectedResult = 100.0 - randomDecrementValue;
        
        Assert.Equal(expectedResult, result);
    }

    [Fact]
    public async Task ItReturnsNullWhenDecrementingNonExistentKey()
    {
        var testKey = CreateTestKey();

        var result = await _redis.ExecuteAsync(ModuleCommand.Decrement, testKey, 1);
        
        Assert.True(result.IsNull);
    }

    [Fact]
    public async Task ItWillDeleteKeyIfDecrementedToZero()
    {
        var testKey = CreateTestKey();

        await _redis.ExecuteAsync(ModuleCommand.Increment, testKey, "AMOUNT", 100, "DEGRADE_RATE", 1.0, "INTERVAL",
            "60min");

        var keyInitiallyExists = await _redis.KeyExistsAsync(testKey);

        var decrementResult = (double)await _redis.ExecuteAsync(ModuleCommand.Decrement, testKey, 1000);

        var keyExistsAfterTotallyDecrementing = await _redis.KeyExistsAsync(testKey);
        
        Assert.True(keyInitiallyExists);
        Assert.Equal(0, decrementResult);
        Assert.False(keyExistsAfterTotallyDecrementing);
    }
}