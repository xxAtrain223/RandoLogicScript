# Rando Logic Script

The goal for RLS is to be a **declarative, domain-specific language** for defining randomizer logic - regions, locations, exits, events, and their access conditions. It transpiles to **C++** (for [Shipwright](https://github.com/HarbourMasters/Shipwright)) and **Python** (for [Archipelago-SoH](https://github.com/HarbourMasters/Archipelago-SoH), possibly rulebuilder json), so both projects share a single source of truth.

## Usage

```
RandoLogicScript [options] <files/folders...>
```

### Options

| Option                    | Description                                                   |
| ------------------------- | ------------------------------------------------------------- |
| `-t, --transpiler <name>` | Transpiler to use, must be followed by `-o`. May be repeated. |
| `-o, --output <dir>`      | Output directory for the preceding transpiler.                |
| `-h, --help`              | Show help message.                                            |

Each `-t` must be paired with an `-o`:

```
RandoLogicScript -t soh -o out/soh/ src/
```

Multiple transpilers can be specified, each with their own output directory:

```
RandoLogicScript -t soh -o out/soh/ -t archipelago -o out/archipelago/ src/ extra.rls
```

Input paths can be individual `.rls` files or directories (which are recursively scanned for `.rls` files).

### Available Transpilers

| Name         | Target             |
| ------------ | ------------------ |
| `soh`        | C++ for Shipwright |

## Docs

- [Language Overview](docs\RandoLogicScript-Overview.md)
- [Language Design Doc](docs\RandoLogicScript-Full.md)
