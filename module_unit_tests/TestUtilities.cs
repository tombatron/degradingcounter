using Random = System.Random;

namespace module_unit_tests;

public static class TestUtilities
{
    public static double GetRandomDouble(double min, double max)
    {
        var rand = new Random();
        
        var scale = max - min;

        return (rand.NextDouble() * scale) + min;
    }

    public static string CreateTestKey() => Guid.NewGuid().ToString("n");
}