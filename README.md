# Rando Logic Script

The goal for RLS is to be a **declarative, domain-specific language** for defining randomizer logic - regions, locations, exits, events, and their access conditions. It transpiles to **C++** (for [Shipwright](https://github.com/HarbourMasters/Shipwright)) and **Python** (for [Archipelago-SoH](https://github.com/HarbourMasters/Archipelago-SoH), possibly rulebuilder json), so both projects share a single source of truth.

## Usage

```
RandoLogicScript [options] [files/folders...]
```

### Options

| Option                    | Description                                                   |
| ------------------------- | ------------------------------------------------------------- |
| `-p, --project <path>`    | Load an `rls.json` manifest or a directory containing one.   |
| `-t, --transpiler <name>` | Transpiler to use, must be followed by `-o`. May be repeated. |
| `-o, --output <dir>`      | Output directory for the preceding transpiler.                |
| `-h, --help`              | Show help message.                                            |

Each `-t` must be paired with an `-o`:

```
RandoLogicScript -t soh -o out/soh/ src/
```

Multiple transpilers can be specified, each with their own output directory:

```
RandoLogicScript -t soh -o out/soh/ -t ap -o out/ap/ src/ extra.rls
```

Input paths can be individual `.rls` files or directories (which are recursively scanned for `.rls` files).

### Project Files

An `rls.json` file describes a project rooted at the directory containing the manifest:

```json
{
	"version": 1,
	"sources": ["src", "stdlib/host.rls"],
	"exclude": ["generated/**"],
	"transpilers": {
		"soh": { "output": "generated/soh" },
		"ap": { "output": "generated/ap" }
	}
}
```

All manifest paths are relative to the manifest and must stay within the project root. Source directories are scanned recursively in deterministic order. Project scans exclude `build`, VCS directories, caches, configured exclusions, and transpiler output directories. An output directory is included only when it is explicitly named in `sources`.

Run a project explicitly with:

```
RandoLogicScript --project path/to/rls.json
```

`--project` also accepts a directory containing `rls.json`. When no input files or folders are given, the CLI searches from the current directory upward for the nearest manifest. Explicit input paths remain supported, but cannot be combined with `--project`.

Manifest transpiler outputs are used by default. Each command-line `-t <name> -o <dir>` pair replaces the manifest output for the same transpiler name and leaves other manifest transpilers enabled. Transpiler names are validated by the CLI's registered implementations.

### Available Transpilers

| Name         | Target                 |
| ------------ | ---------------------- |
| `soh`        | C++ for Shipwright     |
| `ap`         | Python for Archipelago |

## Docs

- [Language Overview](docs/RandoLogicScript-Overview.md)
- [Language Design Doc](docs/RandoLogicScript-Full.md)
- [Building Guide](docs/BUILDING.md)
