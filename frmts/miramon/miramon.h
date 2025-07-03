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

CPLErr CPL_DLL MMRGetBandInfo(const MMRInfo &hMMR, int nBand,
                              CPLString *osBandSection,
                              MMDataType *eMMRDataType,
                              MMBytesPerPixel *eMMBytesPerPixel,
                              int *pnBlockXSize, int *pnBlockYSize);
int CPL_DLL MMRGetBandNoData(MMRInfo &hMMR, int nBand, double *pdfValue);
CPLErr CPL_DLL MMRGetRasterBlock(MMRInfo &hMMR, int nBand, int nXBlock,
                                 int nYBlock, void *pData);
CPLErr CPL_DLL MMRGetRasterBlockEx(MMRInfo &hMMR, int nBand, int nXBlock,
                                   int nYBlock, void *pData, int nDataSize);
const char *MMRGetBandName(const MMRInfo &hMMR, int nBand);
CPLErr CPL_DLL MMRGetPCT(MMRInfo &hMMR, int);

#endif /* ndef MMR_MIRAMON_H_INCLUDED */
