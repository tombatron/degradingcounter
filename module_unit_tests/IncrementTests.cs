using StackExchange.Redis;
using static module_unit_tests.TestUtilities;

namespace module_unit_tests;

[Collection("Module Test Collection")]
public class IncrementTests(RedisContainerFixture redisFixture)
{
    private readonly IDatabase _redis = redisFixture.Redis!.GetDatabase();
    
    public static IEnumerable<object[]> TimeUnits => [["ms"], ["sec"], ["min"]];
    
    [Theory]
    [MemberData(nameof(TimeUnits))]
    public async Task ItCanCreateACounterAndReturnTheInitialValue(string timeUnit)
    {
        var amount = GetRandomDouble(1.0, 100.0);

        var testKey = CreateTestKey();
        
        var result = await _redis.ExecuteAsync("DC.INCR", testKey, "AMOUNT", amount, "DEGRADE_RATE", 1.0, "INTERVAL", $"5{timeUnit}");

        Assert.NotNull(result);
        Assert.Equal(amount, (double)result);
    }

    [Theory]
    [MemberData(nameof(TimeUnits))]
    public async Task ItWillAccumulateACounterValue(string timeUnit)
    {
        var testKey = CreateTestKey();
        
        double expectedTotalAmount = 0;
        double actualValue = 0;

        for (var i = 0; i < 10; i++)
        {
            var randomAmount = GetRandomDouble(1.0, 100.0);

            expectedTotalAmount += randomAmount;

            actualValue = (double)(await _redis.ExecuteAsync("DC.INCR", testKey, "AMOUNT", randomAmount, "DEGRADE_RATE", 1.0,
                "INTERVAL", $"6{timeUnit}"));
        }
        
        Assert.Equal(expectedTotalAmount, actualValue);
    }
}