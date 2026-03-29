# Rando Logic Script

The goal for RLS is to be a **declarative, domain-specific language** for defining randomizer logic - regions, locations, exits, events, and their access conditions. It transpiles to **C++** (for [Shipwright](https://github.com/HarbourMasters/Shipwright)) and **Python** (for [Archipelago-SoH](https://github.com/HarbourMasters/Archipelago-SoH), possibly rulebuilder json), so both projects share a single source of truth.

- [Language Overview](docs\RandoLogicScript-Overview.md)
- [Language Design Doc](docs\RandoLogicScript-Full.md)
