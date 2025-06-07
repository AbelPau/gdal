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

#include "miramon_p.h"
#include "miramon_rastertools.h"

// Converts FileNameI.rel to FileName.img
CPLString MMRGetFileNameFromRelName(const char *pszRELFile)
{
    if (!pszRELFile)
        return "";

    CPLString pszFile =
        CPLString(CPLResetExtensionSafe(pszRELFile, "").c_str());

    if (pszFile.length() < 2)
        return "";

    pszFile.resize(pszFile.size() - 2);  // I.
    pszFile += pszExtRaster;

    return pszFile;
}
