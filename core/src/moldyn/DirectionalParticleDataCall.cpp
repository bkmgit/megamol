/*
 * DirectionalParticleDataCall.cpp
 *
 * Copyright (C) 2009 by Universitaet Stuttgart (VISUS). 
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "mmcore/moldyn/DirectionalParticleDataCall.h"
//#include "vislib/memutils.h"

using namespace megamol::core;

/****************************************************************************/


unsigned int megamol::core::moldyn::DirectionalParticles::DirDataSize[] = {0, 12};


/*
 * moldyn::DirectionalParticles::DirectionalParticles
 */
moldyn::DirectionalParticles::DirectionalParticles(void)
        : SimpleSphericalParticles(), dirDataType(DIRDATA_NONE), dirPtr(NULL),
    dirStride(0), dirAccessor{new DirData_None{}} {
    // intentionally empty
}


/*
 * moldyn::DirectionalParticles::DirectionalParticles
 */
moldyn::DirectionalParticles::DirectionalParticles(
        const moldyn::DirectionalParticles& src) {
    *this = src;
}


/*
 * moldyn::DirectionalParticles::~DirectionalParticles
 */
moldyn::DirectionalParticles::~DirectionalParticles(void) {
    this->dirDataType = DIRDATA_NONE;
    this->dirPtr = NULL; // DO NOT DELETE
}


/*
 * moldyn::DirectionalParticles::operator=
 */
moldyn::DirectionalParticles&
moldyn::DirectionalParticles::operator=(
        const moldyn::DirectionalParticles& rhs) {
    SimpleSphericalParticles::operator=(rhs);
    this->dirDataType = rhs.dirDataType;
    this->dirPtr = rhs.dirPtr;
    this->dirStride = rhs.dirStride;
    return *this;
}


/*
 * moldyn::DirectionalParticles::operator==
 */
bool moldyn::DirectionalParticles::operator==(
        const moldyn::DirectionalParticles& rhs) const {
    return SimpleSphericalParticles::operator==(rhs)
        && (this->dirDataType == rhs.dirDataType)
        && (this->dirPtr == rhs.dirPtr)
        && (this->dirStride == rhs.dirStride);
}


/****************************************************************************/


/*
 * moldyn::DirectionalParticleDataCall::DirectionalParticleDataCall
 */
moldyn::DirectionalParticleDataCall::DirectionalParticleDataCall(void)
        : AbstractParticleDataCall<DirectionalParticles>() {
    // Intentionally empty
}


/*
 * moldyn::DirectionalParticleDataCall::~DirectionalParticleDataCall
 */
moldyn::DirectionalParticleDataCall::~DirectionalParticleDataCall(void) {
    // Intentionally empty
}


/*
 * moldyn::DirectionalParticleDataCall::operator=
 */
moldyn::DirectionalParticleDataCall& moldyn::DirectionalParticleDataCall::operator=(
        const moldyn::DirectionalParticleDataCall& rhs) {
    AbstractParticleDataCall<DirectionalParticles>::operator =(rhs);
    return *this;
}
