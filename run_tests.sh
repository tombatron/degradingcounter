#!/usr/bin/zsh
make build

cp degrading-counter.so module_unit_tests

dotnet test module_unit_tests/module_unit_tests.csproj