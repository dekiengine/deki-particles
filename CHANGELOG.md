# Changelog

Notable changes to `deki-particles`. Engine and editor changes are in the
[engine changelog](https://github.com/dekiengine/deki-engine/blob/master/CHANGELOG.md).

A package's `minEngine` names the engine version it needs. Before 1.0 a
breaking change bumps the minor across the editor, the engine and every
package together, so a package with no changes of its own is still released
alongside one that has them.

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
