/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements some raster functions.
 * Author:   Abel Pau
 *
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MMR_MIRAMON_H_INCLUDED
#define MMR_MIRAMON_H_INCLUDED

#include <cstdio>

//#include "cpl_conv.h"
#include "cpl_string.h"

#include "miramonrel.h"

/* -------------------------------------------------------------------- */
/*      Prototypes                                                      */
/* -------------------------------------------------------------------- */

int CPL_DLL MMRClose(MMRHandle); /* 0 = success */

CPLErr CPL_DLL MMRGetBandInfo(MMRHandle hMMR, int nBand,
                              CPLString *osBandSection,
                              MMDataType *eMMRDataType,
                              MMBytesPerPixel *eMMBytesPerPixel,
                              int *pnBlockXSize, int *pnBlockYSize);
int CPL_DLL MMRGetBandNoData(MMRHandle hMMR, int nBand, double *pdfValue);
CPLErr CPL_DLL MMRGetRasterBlock(MMRHandle hMMR, int nBand, int nXBlock,
                                 int nYBlock, void *pData);
CPLErr CPL_DLL MMRGetRasterBlockEx(MMRHandle hMMR, int nBand, int nXBlock,
                                   int nYBlock, void *pData, int nDataSize);
const char *MMRGetBandName(MMRHandle hMMR, int nBand);
CPLErr CPL_DLL MMRGetPCT(MMRHandle, int);

#endif /* ndef MMR_MIRAMON_H_INCLUDED */
