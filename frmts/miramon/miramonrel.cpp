/******************************************************************************
 *
 * Project:  MiraMonRaster driver
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
#include "miramonrel.h"

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

MMRRel::MMRRel(CPLString osRELFilenameIn) : osRelFileName("")
{
    osRelFileName = osRELFilenameIn;
}

/************************************************************************/
/*                              ~MMRRel()                              */
/************************************************************************/

MMRRel::~MMRRel()
{
}

/************************************************************************/
/*                  MMGetDataTypeAndBytesPerPixel()                     */
/************************************************************************/
int MMRRel::MMGetDataTypeAndBytesPerPixel(const char *pszCompType,
                                          MMDataType *nCompressionType,
                                          MMBytesPerPixel *nBytesPerPixel)
{
    if (!nCompressionType || !nBytesPerPixel || !pszCompType)
        return 1;

    if (EQUAL(pszCompType, "bit"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_BIT;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_BYTE_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "byte"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_BYTE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_BYTE_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "byte-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_BYTE_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_BYTE_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "integer"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_INTEGER;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "integer-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "uinteger"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_UINTEGER;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "uinteger-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "long"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_LONG;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "long-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_LONG_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "real"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_REAL;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "real-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_REAL_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "double"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_DOUBLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_DOUBLE_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "double-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_DOUBLE_I_RLE;
        return 0;
    }

    return 1;
}

/************************************************************************/
/*                     GetMetadataValue()                               */
/************************************************************************/
char *MMRRel::GetMetadataValue(const char *pszMainSection,
                               const char *pszSubSection, const char *pszKey)
{
    // Searches in [pszMainSection:pszSubSection]
    CPLString szAtributeDataName;
    szAtributeDataName = pszMainSection;
    szAtributeDataName.append(":");
    szAtributeDataName.append(pszSubSection);

    char *pszValue = MMReturnValueFromSectionINIFile(
        osRelFileName, szAtributeDataName, pszKey);
    if (pszValue)
        return pszValue;

    // If the value is not found then searches in [pszMainSection]
    return MMReturnValueFromSectionINIFile(osRelFileName, pszMainSection,
                                           pszKey);
}

char *MMRRel::GetMetadataValue(const char *pszSection, const char *pszKey)
{
    return MMReturnValueFromSectionINIFile(osRelFileName, pszSection, pszKey);
}
