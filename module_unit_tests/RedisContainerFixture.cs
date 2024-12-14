using DotNet.Testcontainers.Builders;
using StackExchange.Redis;
using Testcontainers.Redis;

namespace module_unit_tests;

// ReSharper disable once ClassNeverInstantiated.Global
public class RedisContainerFixture : IAsyncLifetime
{
    private readonly RedisContainer _redisContainer = new RedisBuilder()
        .WithImage("redis:7.0")
        .WithName("degrading_counter_tests")
        .WithBindMount(AppContext.BaseDirectory, "/module_unit_tests")

        .WithCommand(
            "redis-server",
            "--loadmodule", "/module_unit_tests/degrading-counter.so",
            "--appendonly", "no",
            "--save", "''"
        )
        .WithWaitStrategy(Wait.ForUnixContainer()
            .UntilCommandIsCompleted("redis-cli", "PING")
        )
        .Build();
    
    public ConnectionMultiplexer? Redis { get; private set; }

    public async Task InitializeAsync()
    {
        await _redisContainer.StartAsync();

        Redis = await ConnectionMultiplexer.ConnectAsync(_redisContainer.GetConnectionString());
    }
    
    public async Task DisposeAsync()
    {
        if (Redis is not null)
        {
            await Redis.CloseAsync();
            await Redis.DisposeAsync();
        }

        await _redisContainer.StopAsync();
        await _redisContainer.DisposeAsync();
    }
}