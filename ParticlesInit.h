#pragma once

/// Registers the ParticleGraph asset loader and the built-in modifiers. Safe to
/// call more than once. Called from DekiInitPackageSystems() on static builds
/// and from DekiPluginInit() when the package is a DLL.
///
/// Global scope on purpose: the editor generates a file that declares these as
/// plain `extern void DekiParticlesInitSystem();`, and that file cannot know a
/// package's namespace (see deki-tween's TweenInit.h).
void DekiParticlesInitSystem();
void DekiParticlesShutdownSystem();
