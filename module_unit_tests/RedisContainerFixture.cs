using StackExchange.Redis;
using DotNet.Testcontainers.Builders;
using Testcontainers.Redis;
using DotNet.Testcontainers.Images;
using DotNet.Testcontainers.Volumes;
using System.Text;

namespace module_unit_tests;

public class RedisContainerFixture : IAsyncLifetime
{
    public static string ValgrindLogPath => Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "valgrind-logs");
    private readonly IFutureDockerImage _baseImage;
    public RedisContainer RedisContainer { get; }
    public ConnectionMultiplexer? Redis { get; private set; }

    private IVolume _logVolume;

    public RedisContainerFixture()
    {
        if (Directory.Exists(ValgrindLogPath))
        {
            Directory.Delete(ValgrindLogPath, true);
        }

        Directory.CreateDirectory(ValgrindLogPath);

        _logVolume = new VolumeBuilder().WithName($"valgrind-logs-{Guid.NewGuid():n}").Build();
        _logVolume.CreateAsync().Wait();

        _baseImage = new ImageFromDockerfileBuilder()
            .WithDockerfile("Dockerfile")
            .WithDockerfileDirectory(AppContext.BaseDirectory)
            .Build();

        RedisContainer = new RedisBuilder()
            .WithImage(_baseImage)
            .WithName("degrading_counter_tests")
            .WithBindMount(AppContext.BaseDirectory, "/module_unit_tests")
            .WithVolumeMount(_logVolume.Name, "/valgrind-logs/")
            .WithCommand(
                "valgrind",
                "--leak-check=full",
                "--track-origins=yes",
                "--log-file=/valgrind-logs/valgrind.log",
                "redis-server",
                "--loadmodule", "/module_unit_tests/degrading-counter.so",
                "--appendonly", "no",
                "--save", "''"
            )
            .WithWaitStrategy(Wait.ForUnixContainer()
                .UntilCommandIsCompleted("redis-cli", "PING")
            )
            .Build();
    }

    public async Task InitializeAsync()
    {
        await _baseImage.CreateAsync();

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

        try
        {
            var container = new ContainerBuilder()
                .WithImage("busybox")
                .WithVolumeMount(_logVolume.Name, "/valgrind-logs")
                .WithName("valgrind-output")
                .Build();

            try
            {
                await container.StartAsync();

                var valgrindLogText = Encoding.UTF8.GetString(await container.ReadFileAsync("/valgrind-logs/valgrind.log"));

                File.WriteAllText("valgrind.log", valgrindLogText);

                Console.WriteLine(valgrindLogText);
            }
            finally
            {
                await container.StopAsync();
                await container.DisposeAsync();
            }
        }
        finally
        {
            await _logVolume.DeleteAsync();
        }
    }
}