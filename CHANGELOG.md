# Changelog

Notable changes to `deki-particles`. Engine and editor changes are in the
[engine changelog](https://github.com/dekiengine/deki-engine/blob/master/CHANGELOG.md).

A package's `minEngine` names the engine version it needs. Before 1.0 a
breaking change bumps the minor across the editor, the engine and every
package together, so a package with no changes of its own is still released
alongside one that has them.

## Unreleased

### Changed
- **Names follow the code style** (deki-engine/docs/codestyle): types, functions and enum values are PascalCase, constants kPascalCase, members m_PascalCase, locals and parameters camelCase. The code is formatted with clang-format 22.
- The functions the editor finds by name are PascalCase: DekiParticlesRegisterComponents, DekiParticlesGetAutoComponentCount, DekiParticlesEnsureRegistered and the rest. Built against engine ABI 21; a build of this package from before does not load and is rebuilt.
- Renamed: `AssetTypeName` is `kAssetTypeName`; `StaticNodeDisplayName` and `StaticNodeDescription` are `kStaticNodeDisplayName` and `kStaticNodeDescription`.

### Removed
- The former names from before 0.16.0 (bare class names, and deki-gpio's
  `DekiEsp32::ESP32PinSetup`). A scene that old is upgraded with 0.17 first.

## 0.17.0

### Changed
- `minEngine` 0.17.0. Reflection ABI 20: the package must be rebuilt.
- The asset file is read into an External Deki::Buffer while it is parsed,
  not a std::vector on the internal heap.

### Fixed
- A particle graph asset loads on a device: its loader opened the path with
  `std::ifstream`, which cannot open `F:/` or `S:/`.

## 0.16.0

### Changed
- **Moved into the `DekiParticles` namespace.** Every component was declared at global
  scope, which made its identity a bare class name — the name a scene file
  stores and the name the registry keys on — so two packages defining one name
  collided there with nothing to tell them apart. Each component carries
  `DEKI_FORMER_NAME` with the name it was saved under before, so existing
  scenes load unchanged and are written back qualified on the next save.
  Code naming these types needs the namespace: `using namespace DekiParticles;` or a
  qualified name.
- Enum properties are stored by name rather than by number, so appending to an
  enum or reordering one no longer changes what a saved scene means. Files
  written before this still read.
- `minEngine` 0.16.0. Reflection ABI 17: the package must be rebuilt.

## 0.15.0

### Changed
- `ParticlePool`'s columns are `Deki::Buffer`, allocated through the engine
  with an explicit region instead of `new[]`.
- Blit sources are built from a named `PixelLayout`, and `Texture2D` comes
  from the engine core.

### Added
- This package declares its own memory region, which is what the open region
  names in engine 0.15.0 are for: a package can name a region the engine has
  never heard of, and a board's provider decides whether it has it.
