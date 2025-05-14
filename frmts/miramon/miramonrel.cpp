/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements MMRREL class.
 * Author:   Abel Pau
 * 
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "miramon_p.h"

#ifdef MSVC
#include "..\miramon_common\mm_gdal_functions.h"  // For MMCheck_REL_FILE()
//#include "..\miramon_common\mm_gdal_constants.h"
#else
#include "../miramon_common/mm_gdal_functions.h"  // For MMCheck_REL_FILE()
//#include "../miramon_common/mm_gdal_constants.h"
#endif

#include "miramon_rastertools.h"

/************************************************************************/
/*                              MMRRel()                               */
/************************************************************************/

MMRRel::MMRRel(CPLString pszRELFilenameIn) : pszRelFileName(pszRELFilenameIn)
{
}

/************************************************************************/
/*                              ~MMRRel()                              */
/************************************************************************/

MMRRel::~MMRRel()

{
}

/************************************************************************/
/*                     GetMetadataValue()                                    */
/************************************************************************/
char *MMRRel::GetMetadataValue(const char *pszMainSection,
                               const char *pszBandSection, const char *pszKey)
{
    // Example: [pszMainSection:pszBandSection]
    CPLString szAtributeDataName;
    szAtributeDataName = pszMainSection;
    szAtributeDataName.append(":");
    szAtributeDataName.append(pszBandSection);

    char *pszCompType = MMReturnValueFromSectionINIFile(
        pszRelFileName, szAtributeDataName, pszKey);
    if (pszCompType)
        return pszCompType;

    pszCompType =
        MMReturnValueFromSectionINIFile(pszRelFileName, pszMainSection, pszKey);

    return pszCompType;
}
