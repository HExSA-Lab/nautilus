/**
 * Copyright (c) 2014      Los Alamos National Security, LLC
 *                         All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * LA-CC 10-123
 *
 * -- High Performance Conjugate Gradient Benchmark (HPCG)
 *    HPCG - 2.1 - January 31, 2014
 *
 *    Michael A. Heroux
 *    Scalable Algorithms Group, Computing Research Center
 *    Sandia National Laboratories, Albuquerque, NM
 *
 *    Piotr Luszczek
 *    Jack Dongarra
 *    University of Tennessee, Knoxville
 *    Innovative Computing Laboratory
 *    (C) Copyright 2013 All Rights Reserved
 *
 */

#ifndef LGNCG_GEOMETRY_H_INCLUDED
#define LGNCG_GEOMETRY_H_INCLUDED

//#include <cassert>
#include <stdint.h>

namespace lgncg {

struct Geometry {
    int64_t size;
    int64_t npx;
    int64_t npy;
    int64_t npz;
    int64_t nx;
    int64_t ny;
    int64_t nz;
    int64_t ipx;
    int64_t ipy;
    int64_t ipz;

    Geometry(void)
        : size(0),
           npx(0),
           npy(0),
           npz(0),
            nx(0),
            ny(0),
            nz(0),
           ipx(0),
           ipy(0),
           ipz(0) { ; }

    Geometry(int64_t size,
             int64_t npx,
             int64_t npy,
             int64_t npz,
             int64_t nx,
             int64_t ny,
             int64_t nz)
        : size(size),
           npx(npx),
           npy(npy),
           npz(npz),
            nx(nx),
            ny(ny),
            nz(nz),
           ipx(0),
           ipy(0),
           ipz(0) { /*KCH REMOVED assert(npx * npy * npz == size);*/ }
};

inline int64_t
computeTIDOfMatrixRow(const Geometry &geom,
                      int64_t index)
{
    int64_t gnx = geom.nx * geom.npx;
    int64_t gny = geom.ny * geom.npy;

    int64_t iz = index / (gny * gnx);
    int64_t iy = (index - iz * gny * gnx) / gnx;
    int64_t ix = index % gnx;
    int64_t ipz = iz / geom.nz;
    int64_t ipy = iy / geom.ny;
    int64_t ipx = ix / geom.nx;
    return ipx + ipy * geom.npx + ipz * geom.npy * geom.npx;
}

} // end lgncg namespace

#endif
