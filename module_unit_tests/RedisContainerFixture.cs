using StackExchange.Redis;
using DotNet.Testcontainers.Builders;
using Testcontainers.Redis;

namespace module_unit_tests;

public class RedisContainerFixture : IAsyncLifetime
{
    public RedisContainer RedisContainer { get; }
    public ConnectionMultiplexer? Redis { get; private set; } 

    public RedisContainerFixture()
    {
        RedisContainer = new RedisBuilder()
            .WithImage("redis:7.0")
            .WithName("degrading_counter_tests")
            .WithBindMount(AppContext.BaseDirectory, "/module_unit_tests")
            .WithCommand("redis-server", "--loadmodule", "/module_unit_tests/degrading-counter.so")
            .WithWaitStrategy(Wait.ForUnixContainer()
                .UntilCommandIsCompleted("redis-cli", "PING")
            )
            .Build();
    }

    public async Task InitializeAsync()
    {
        await RedisContainer.StartAsync();

        Redis = await ConnectionMultiplexer.ConnectAsync(RedisContainer.GetConnectionString());
    }


    public async Task DisposeAsync()
    {
        if (Redis is not null)
        {
            await Redis.CloseAsync();
            await Redis.DisposeAsync();
        }
        
        await RedisContainer.StopAsync();
        await RedisContainer.DisposeAsync();
    }
}