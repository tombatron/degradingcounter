namespace module_unit_tests;

[Collection("Module Test Collection")]
public class IncrementTests(RedisContainerFixture redisFixture)
{
    [Fact]
    public async Task ItCanIncrementACounter()
    {
        var testKey = Guid.NewGuid().ToString("n");
        
        var db = redisFixture.Redis!.GetDatabase();

        var result = await db.ExecuteAsync("DC.INCR", testKey, "AMOUNT", 1, "DEGRADE_RATE", 1.0, "INTERVAL", "5sec");

        Assert.NotNull(result);
        Assert.Equal(1, (double)result);
    }
}