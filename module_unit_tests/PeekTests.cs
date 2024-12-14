using StackExchange.Redis;

namespace module_unit_tests;

[Collection("Module Test Collection")]
public class PeekTests(RedisContainerFixture redisFixture)
{
    private readonly IDatabase _redis = redisFixture.Redis!.GetDatabase();

    [Fact]
    public async Task ItCanGetCurrentCounterValue()
    {
        var amount = GetRandomDouble(1.0, 100.0);

        var testKey = CreateTestKey();

        await _redis.ExecuteAsync(ModuleCommand.Increment, testKey, "AMOUNT", amount, "DEGRADE_RATE", 1.0, "INTERVAL",
            "60min");

        var peekedResult = (double)await _redis.ExecuteAsync(ModuleCommand.Peek, testKey);

        Assert.Equal(amount, peekedResult);
    }

    [Fact]
    public async Task ItWillReturnNullIfANonExistentKey()
    {
        var shouldBeNull = await _redis.ExecuteAsync(ModuleCommand.Peek, "whatever_this_is_it_doesnt_exist");
        
        Assert.True(shouldBeNull.IsNull);
    }
}