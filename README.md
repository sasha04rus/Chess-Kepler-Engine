# Chess-Kepler-Engine

## TAR: сборка для игры

```bash
cmake -S . -B build-tar-release -DTAKE_AND_RETURN_VARIANT=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build-tar-release -j 4
ctest --test-dir build-tar-release --output-on-failure
```