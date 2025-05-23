/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements some raster functions.
 * Author:   Abel Pau
 * 
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "miramon_p.h"

#include <cstddef>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_pam.h"
#include "gdal_priv.h"

#ifdef MSVC
#include "..\miramon_common\mm_gdal_functions.h"
#include "..\miramon_common\mm_gdal_constants.h"
#else
#include "../miramon_common/mm_gdal_functions.h"
#include "../miramon_common/mm_gdal_constants.h"
#endif

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
