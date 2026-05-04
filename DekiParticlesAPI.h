#pragma once

// Tiny header that defines only DEKI_PARTICLES_API. Component headers include
// this instead of DekiParticlesModule.h to avoid a circular include
// (DekiParticlesModule.h is the umbrella include for external consumers and
// pulls in every component header — including it from a component header
// would re-enter modifier headers before ParticleModifier was fully defined).

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
