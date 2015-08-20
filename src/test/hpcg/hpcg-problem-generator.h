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

#ifndef HPCG_PROBLEM_GENERATOR_H_INCLUDED
#define HPCG_PROBLEM_GENERATOR_H_INCLUDED

#include "lgncg.h"

#include <inttypes.h>
#include <cstdlib>
#include <set>

////////////////////////////////////////////////////////////////////////////////
// TODO
// pass raw pointers to this routine to keep memory usage down
////////////////////////////////////////////////////////////////////////////////

struct ProblemGenerator {
    // here for destructor
    int64_t nLocalRows;
    // sparse matrix stuff
    double **A;
    uint8_t *non0sInRow;
    int64_t **mtxInd;
    double  **matDiag;
    int64_t *l2gTab;
    // b vector stuff
    double *b;
    // total number of non-zeros
    int64_t tNon0s;
    /**
     * akin to HPCG's GenerateProblem
     */
    ProblemGenerator(const lgncg::Geometry &geom,
                     int64_t maxNon0sInCol,
                     int taskID)
    {
        using lgncg::Geometry;

        const int64_t nx  = geom.nx;
        const int64_t ny  = geom.ny;
        const int64_t nz  = geom.nz;
        const int64_t npx = geom.npx;
        const int64_t npy = geom.npy;
        const int64_t npz = geom.npz;
        //current task's z location in the npx by npy by npz processor grid
        const int64_t ipz = taskID / (npx * npy);
        //current task's y location in the npx by npy by npz processor grid
        const int64_t ipy = (taskID - ipz * npx * npy) / npx;
        //current task's x location in the npx by npy by npz processor grid
        const int64_t ipx = taskID % npx;
        // global geometry
        const int64_t gnx = npx * nx;
        const int64_t gny = npy * ny;
        const int64_t gnz = npz * nz;
        // number of local rows for this task
        this->nLocalRows = nx * ny * nz;
        // stencil size
        const int64_t stencilSize = maxNon0sInCol;
        // max stencilSize for number of non-zeros per row
        const int64_t nNon0sPerRow = stencilSize;
        // allocate arrays that are of length nLocalRows
        non0sInRow = new uint8_t[nLocalRows];
        mtxInd = new int64_t *[nLocalRows];
        A = new double *[nLocalRows];
        matDiag = new double *[nLocalRows];
        b = new double[nLocalRows];
        l2gTab = new int64_t[nLocalRows];
        // NULLify some things
        for (int64_t i = 0; i < nLocalRows; ++i) {
            A[i] = NULL;
            matDiag[i] = NULL;
            mtxInd[i] = NULL;
        }
        // now allocate the arrays pointed to
        for (int64_t i = 0; i < nLocalRows; ++i) {
            A[i] = new double[nNon0sPerRow];
            (void)memset(A[i], 0, nNon0sPerRow * sizeof(double));
            mtxInd[i] = new int64_t[nNon0sPerRow];
            (void)memset(mtxInd[i], 0, nNon0sPerRow * sizeof(double));
        }
        int64_t locNNon0s = 0;
        for (int64_t iz = 0; iz < nz; iz++) {
            int64_t giz = ipz * nz + iz;
            for (int64_t iy = 0; iy < ny; iy++) {
                int64_t giy = ipy * ny + iy;
                for (int64_t ix = 0; ix < nx; ix++) {
                    int64_t gix = ipx * nx + ix;
                    // current local row
                    int64_t curLocRow = iz * nx * ny + iy * nx + ix;
                    int64_t curGlobRow = giz * gnx * gny + giy * gnx + gix;
                    l2gTab[curLocRow] = curGlobRow;
                    // we'll setup the g2l map later in halo setup
                    char nNon0sInRow = 0;
                    // pointer to current value in current row
                    double *curValPtr = A[curLocRow];
                    // pointer to current index in current row
                    int64_t *currentIndexPointerG = mtxInd[curLocRow];
                    for (int64_t sz = -1; sz <= 1; sz++) {
                        if (giz + sz > -1 && giz + sz < gnz) {
                            for (int64_t sy = -1; sy <= 1; sy++) {
                                if (giy + sy > -1 && giy + sy < gny) {
                                    for (int64_t sx = -1; sx <= 1; sx++) {
                                        if (gix + sx > -1 && gix + sx < gnx) {
                                            int64_t curcol = curGlobRow + sz *
                                                             gnx * gny + sy *
                                                             gnx + sx;
                                            if (curcol == curGlobRow) {
                                                matDiag[curLocRow] = curValPtr;
                                                *curValPtr++ = 26.0;
                                            }
                                            else {
                                                *curValPtr++ = -1.0;
                                            }
                                            *currentIndexPointerG++ = curcol;
                                            nNon0sInRow++;
                                        } // end x bounds test
                                    } // end sx loop
                                } // end y bounds test
                            } // end sy loop
                        } // end z bounds test
                    } // end sz loop
                    tNon0s += nNon0sInRow;
                    non0sInRow[curLocRow] = nNon0sInRow;
                    locNNon0s += nNon0sInRow;
                    b[curLocRow] = 26.0 - (double)(nNon0sInRow - 1);
                } // end ix loop
            } // end iy loop
        } // end iz loop
    }
    /**
     * destructor
     */
    ~ProblemGenerator(void) {
        delete[] non0sInRow;
        delete[] b;
        for (int64_t i = 0; i < nLocalRows; ++i) {
            delete[] A[i];
            delete[] mtxInd[i];
        }
        delete[] A;
        delete[] mtxInd;
        delete[] matDiag;
        delete[] l2gTab;
    }
private:
    ProblemGenerator(void);
};

#endif
