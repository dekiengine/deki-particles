#pragma once

// Defines only DEKI_PARTICLES_API. Package headers include this rather than
// DekiParticlesPackage.h, the umbrella header for outside users, which
// includes every header of the package and would include them circularly.

#ifdef DEKI_EDITOR
#ifdef _WIN32
#ifdef DEKI_PARTICLES_EXPORTS
#define DEKI_PARTICLES_API __declspec(dllexport)
#else
#define DEKI_PARTICLES_API __declspec(dllimport)
#endif
#else
#define DEKI_PARTICLES_API
#endif
#else
#define DEKI_PARTICLES_API
#endif
