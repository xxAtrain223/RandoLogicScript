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

### Visual Studio Extension

The RLS Visual Studio extension is an x64 Visual Studio 2022-compatible VSIX tested
with Visual Studio 2026. Building it requires:

- **Desktop development with C++** for the native language server
- **Visual Studio extension development** for the Experimental Instance workflow
- Node.js 22 for native binary validation
- Python 3.12 for the process smoke test
- PowerShell 7 for the automated Experimental Instance harness

Visual Studio 2022 can build the native server with its CMake generator. Visual
Studio 2026 currently requires an x64 developer shell and Ninja because CMake 4.1
does not expose a Visual Studio 18 generator:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1' `
	-Arch amd64 -HostArch amd64 -SkipAutomaticLocation
cmake -S . -B build-visualstudio-release -G Ninja `
	-DCMAKE_BUILD_TYPE=Release `
	-DBUILD_TESTING=OFF `
	-DRLS_STATIC_MSVC_RUNTIME=ON
cmake --build build-visualstudio-release --target rls_language_server --parallel
```

Build the VSIX with the full-framework MSBuild installed by Visual Studio:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' `
	editors\visualstudio\RandoLogicScript.VisualStudio.sln `
	/restore /t:Build /p:Configuration=Release
```

The project defaults `RlsLanguageServerPath` to
`build-visualstudio-release\lsp\rls_language_server.exe`. Override it for another
generator or build directory:

```powershell
/p:RlsLanguageServerPath=C:\path\to\rls_language_server.exe
```

The build rejects a missing server, a non-x64 PE, or a server importing the dynamic
MSVC runtime. The resulting package is:

```text
editors\visualstudio\src\bin\Release\net472\RandoLogicScript.VisualStudio.vsix
```

Validate and test it with:

```powershell
python lsp\tests\process_smoke.py `
	--server build-visualstudio-release\lsp\rls_language_server.exe

dotnet test editors\visualstudio\tests\RandoLogicScript.VisualStudio.Tests.csproj `
	--configuration Release

& editors\visualstudio\scripts\validate-vsix.ps1 `
	-VsixPath editors\visualstudio\src\bin\Release\net472\RandoLogicScript.VisualStudio.vsix

& editors\visualstudio\scripts\test-experimental-instance.ps1 `
	-Configuration Release `
	-RootSuffix RLSPhase4
```

The Experimental Instance harness runs only on Visual Studio 2026. It deploys the
VSIX, opens Unicode/spaced Open Folder and standalone fixtures through DTE automation,
validates protocol traces and crash recovery, and cleans up its processes. Use
`editors/visualstudio/MANUAL-TESTING.md` for visual, accessibility, theme, and supported
Visual Studio version checks.

For development, `RLS_LANGUAGE_SERVER_PATH` overrides the bundled executable when
Visual Studio starts the language client. Release users do not need this setting.

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
| `RLS_STATIC_MSVC_RUNTIME` | `OFF` | Link the MSVC runtime statically for distributable Windows editor packages. |

## Additional CMake Targets

### Clean

```sh
cmake --build build --target clean
```
