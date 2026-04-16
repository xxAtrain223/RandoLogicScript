# Building RandoLogicScript

## Prerequisites

- **CMake** 3.14 or later
- A **C++20**-capable compiler:
  - GCC 10+
  - Clang 10+
  - MSVC 2019+ (Visual Studio 2019 or later)
- **Git** (required by CMake to fetch dependencies automatically)

All other dependencies ([PEGTL](https://github.com/taocpp/PEGTL) for parsing and [GoogleTest](https://github.com/google/googletest) for testing) are fetched automatically by CMake via `FetchContent`.

## Getting Started

### 1. Clone the repository

```sh
git clone https://github.com/xxAtrain223/RandoLogicScript.git
cd RandoLogicScript
```

### 2. Configure

```sh
cmake -B build
```

To also enable the test suite, pass `-DBUILD_TESTING=ON`:

```sh
cmake -B build -DBUILD_TESTING=ON
```

### 3. Build

```sh
cmake --build build
```

The compiled binary will be located at `build/console/RandoLogicScript` (Linux/macOS) or `build\console\Debug\RandoLogicScript.exe` (Windows/MSVC).

### 4. Run

```sh
./build/console/RandoLogicScript --help
```

See the [README](README.md) for full usage instructions and available options.

## Running Tests

Configure with `-DBUILD_TESTING=ON`, then after building run:

```sh
ctest --test-dir build --output-on-failure
```

Individual test executables (`ast_tests`, `parser_tests`, etc.) can also be run directly from the `build` directory.

## IDE Support

### Visual Studio (Windows)

Open the repository folder with **File → Open → CMake…** or **File → Open → Folder…**. Visual Studio will detect `CMakeLists.txt` automatically.

### VS Code

Install the **CMake Tools** extension, then open the repository folder. Use the CMake Tools sidebar to configure, build, and run tests.

### CLion

Open the repository folder — CLion will detect `CMakeLists.txt` and configure the project automatically.

## Build Options

| Option           | Default | Description                        |
| ---------------- | ------- | ---------------------------------- |
| `BUILD_TESTING`  | `OFF`   | Build the GoogleTest test suites.  |
