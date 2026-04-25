# Building RandoLogicScript

All third-party dependencies ([PEGTL](https://github.com/taocpp/PEGTL) for parsing and [GoogleTest](https://github.com/google/googletest) for the test suite) are fetched automatically by CMake via `FetchContent` - no manual dependency installation is needed beyond the items listed per platform below.

## Windows

Requires:

- Visual Studio 2022 Community Edition with the **Desktop development with C++** feature set
- CMake 3.14 or later (bundled with Visual Studio, or install via [winget](https://learn.microsoft.com/en-us/windows/package-manager/winget/) / [Chocolatey](https://chocolatey.org/))
- Git (bundled with Visual Studio, or install standalone)

```powershell
# Clone the repository
git clone https://github.com/xxAtrain223/RandoLogicScript.git
cd RandoLogicScript
# Configure (generates a Visual Studio 2022 solution)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
# Build
cmake --build build
# Run
.\build\console\Debug\RandoLogicScript.exe --help
```

### Developing on Windows

#### Visual Studio

Open the generated solution file `build\RandoLogicScript.sln`, or open the repository folder directly via **File → Open → Folder…** - Visual Studio will detect `CMakeLists.txt` automatically.

#### Visual Studio Code

Open the repository folder in VS Code. Install the [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) extension to configure, build, and debug directly from the editor.

## Linux

### Install dependencies

#### Debian / Ubuntu

```sh
# using gcc
apt-get install gcc g++ git cmake

# or using clang
apt-get install clang git cmake
```

#### Arch

```sh
# using gcc
pacman -S gcc git cmake

# or using clang
pacman -S clang git cmake
```

#### Fedora

```sh
# using gcc
dnf install gcc gcc-c++ git cmake

# or using clang
dnf install clang git cmake
```

#### NixOS

```sh
# Enter a temporary shell with required tools
nix-shell -p gcc git cmake

# or using clang
nix-shell -p clang git cmake
```

### Build

```sh
# Clone the repository
git clone https://github.com/xxAtrain223/RandoLogicScript.git
cd RandoLogicScript

# Configure
cmake -S . -B build

# Build
cmake --build build

# Run
./build/console/RandoLogicScript --help
```

*Note: If you're using VS Code, the [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) extension makes it easy to configure, build, and debug without leaving the editor.*

## macOS

Requires Xcode command-line tools and CMake (install via [Homebrew](https://brew.sh/) or manually).

```sh
# Install Xcode command-line tools (if not already installed)
xcode-select --install

# Install CMake via Homebrew
brew install cmake

# Clone the repository
git clone https://github.com/xxAtrain223/RandoLogicScript.git
cd RandoLogicScript

# Configure
cmake -S . -B build

# Build
cmake --build build

# Run
./build/console/RandoLogicScript --help
```

*Note: If you're using VS Code, the [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) extension makes it easy to configure, build, and debug without leaving the editor.*

## Running Tests

Pass `-DBUILD_TESTING=ON` at configure time, then use `ctest` after building:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Individual test executables (`ast_tests`, `parser_tests`, etc.) can also be run directly from the `build` directory.

Acceptance tests are included in `console_acceptance_tests` and run the end-to-end
pipeline over `examples/rls`.

- SOH acceptance golden files: `examples/soh/*.gen.{h,cpp}`
- AP acceptance golden file: `examples/ap/ap.py`

To run only acceptance tests:

```sh
ctest --test-dir build -R Acceptance --output-on-failure
```

## Build Options

| Option          | Default | Description                       |
| --------------- | ------- | --------------------------------- |
| `BUILD_TESTING` | `OFF`   | Build the GoogleTest test suites. |

## Additional CMake Targets

### Clean

```sh
cmake --build build --target clean
```
