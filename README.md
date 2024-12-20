# Degrading Counter

[![Build](https://github.com/tombatron/degradingcounter/actions/workflows/build_and_test.yml/badge.svg)](https://github.com/tombatron/degradingcounter/actions/workflows/build_and_test.yml)

## Introduction

This is a project that I created in order to explore creating a custom Redis data type. The intent behind this functionality 
is to provide a way to create a counter that automatically degrades over time and that executes in constant time, but the overall
goal of the project was just as a learning exercise. 

## Status

Very much under development. 

## Installation

First, [build](#building) this module. 

Next, copy the module to a directory accessible by your Redis server. 

From within the redis-cli connected to your Redis server issue the command `MODULE LOAD {path to module}/degrading-counter.so`.

[MODULE LOAD](https://redis.io/docs/latest/commands/module-load/)

## Usage

### `DC.INCR`
**Syntax:**
```plaintext
DC.INCR key AMOUNT <value> DEGRADE_RATE <rate> INTERVAL <interval>
```

**Description:**

Increments the value of a degrading counter stored at key using the specified parameters. Creates a new counter if the key does not exist. The degradation properties adjust how the counter's value changes over time.

**Arguments:**

key: The name of the degrading counter.

AMOUNT: The value to increment the counter by (signed double).

Example: 10.5

DEGRADE_RATE: The rate at which the counter degrades over time (signed double).

Example: 0.1

INTERVAL: A string representing the interval and unit of degradation. In the format of `{numeric value}{ms|sec|min}`

Examples: "10ms", "2min", "1sec"

**Argument Order:**

Arguments must be provided in pairs (e.g., AMOUNT <value>). Argument names are case-sensitive.

**Return Value:**

The updated counter value after applying the increment and accounting for degradation.

### `DC.DECR`
**Syntax:**
```plaintext
DC.DECR key <amount>
```

**Description:**

Decrements the value of a degrading counter stored at the provided key.

**Arguments:**

key: The name of the degrading counter.

value: The amount you want to decrement. 

**Return Value:**

The updated counter value after applying the decrement or null if the key doesn't exist. 

### `DC.PEEK`
**Syntax:**
```plaintext
DC.PEEK key
```

**Description:**

Display the current computed value of the decrementing counter.

**Arguments:**

key: The name of the degrading counter.

**Return Value:**

The current computed value of the degrading counter, or null if the key doesn't exist. 


## Implementation

The primary implementation of this module was done using C with the unit tests being implemented using C# (.NET 8, with [Testcontainers](https://testcontainers.com/) and xUnit). 

## Data Type

Behind the scenes the degrading counter is a C structure that stores the following:

| Data Type                | Name                 | Description                                                                                                                   |
|--------------------------|----------------------|-------------------------------------------------------------------------------------------------------------------------------|
| ustime_t (long long)     | created              | The UNIX timestamp in milliseconds then the instance of the data type was created.                                            |
| double                   | degrades_at          | How much does the counter degrade after the specified number of increments have passed.                                       |
| int                      | number_of_increments | How many increments should elapse before degrading the counter? Defaults to 1.                                                |
| CounterIncrements (enum) | increment            | Which time increment should be used to degrade the counter?                                                                   |
| double                   | value                | What is accumulated value of the counter? This will be a raw "un-degraded" number. Only increments and decrements apply here. | 

The `CounterIncrements` enumeration is defined as follows:

| Enumerator    | Value |
|---------------|-------|
| Milliseconds  | 0     |
| Seconds       | 1     | 
| Minutes       | 2     | 

## Building

First thing's first. You need to clone the Redis source code as that is where the header we need to build a module is located. You can find the Redis open-source repository
[here](https://github.com/redis/redis). 

Next... clone this project. 

On my local development environment I cloned the Redis repository to a location directly adjacent to the project directory. You'll see in the make file there is a variable 
called `REDIS_SRC` which is set to `../redis/src`. If you too cloned the Redis repository to an adjacent directory, then you don't need to do anything other than run `make build`. 

The output should be an object called `degrading-counter.so`. 

Running the tests requires that you have the .NET 8.0 SDK and Docker installed. Once that requirement has been satisfied you just need to execute the `run_tests.sh` script. This script will build
the module, copy it to the test project, and then execute the tests using the dotnet test runner. Docker is required because the test project uses a library called [Testcontainers](https://testcontainers.com/) to load up
a test Redis server with the newly built module.

## Contributing

Open a pull-request and include a nice message. 
