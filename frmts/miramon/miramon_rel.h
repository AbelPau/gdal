/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements MMRRel class who gets all information from MiraMon
 *           metadata REL files
 * Author:   Abel Pau
 *
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MMR_REL_H_INCLUDED
#define MMR_REL_H_INCLUDED

#include "cpl_string.h"
#include "gdal_priv.h"
#include "miramon_info.h"  // For MMRInfo

class MMRInfo;

/************************************************************************/
/*                               MMRRel                                */
/************************************************************************/

enum class MMRNomFitxerState
{
    NOMFITXER_NOT_FOUND,        // There is no NomFitxer key
    NOMFITXER_VALUE_EXPECTED,   // The NomFitxer value is the expected
    NOMFITXER_VALUE_EMPTY,      // The NomFitxer value is empty
    NOMFITXER_VALUE_UNEXPECTED  // The NomFitxer value is unexpected
};

enum class MMDataType
{
    DATATYPE_AND_COMPR_UNDEFINED = -1,
    DATATYPE_AND_COMPR_MIN = 0,
    DATATYPE_AND_COMPR_STRING = 0,
    DATATYPE_AND_COMPR_BIT = 1,
    DATATYPE_AND_COMPR_BIT_VELL = 2,  // Not supported
    DATATYPE_AND_COMPR_BYTE = 3,
    DATATYPE_AND_COMPR_INTEGER = 4,
    DATATYPE_AND_COMPR_UINTEGER = 5,
    DATATYPE_AND_COMPR_LONG = 6,
    DATATYPE_AND_COMPR_INTEGER_ASCII = 7,
    DATATYPE_AND_COMPR_REAL = 8,
    DATATYPE_AND_COMPR_DOUBLE = 9,
    DATATYPE_AND_COMPR_REAL_ASCII = 10,
    DATATYPE_AND_COMPR_BYTE_RLE = 11,
    DATATYPE_AND_COMPR_INTEGER_RLE = 12,
    DATATYPE_AND_COMPR_UINTEGER_RLE = 13,
    DATATYPE_AND_COMPR_LONG_RLE = 14,
    DATATYPE_AND_COMPR_REAL_RLE = 15,
    DATATYPE_AND_COMPR_DOUBLE_RLE = 16,
    DATATYPE_AND_COMPR_MAX = 16
};

enum class MMBytesPerPixel
{
    TYPE_BYTES_PER_PIXEL_UNDEFINED = -1,
    TYPE_BYTES_PER_PIXEL_STRING = 0,
    TYPE_BYTES_PER_PIXEL_BIT = 0,
    TYPE_BYTES_PER_PIXEL_BYTE_I_RLE = 1,
    TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE = 2,
    TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE = 4,
    TYPE_BYTES_PER_PIXEL_DOUBLE_I_RLE = 8
};

class MMRBand;

class MMRRel
{
  public:
    CPLErr UpdateInfoFromREL(const char *pszFileName, MMRInfo &hMMR);
    static CPLString GetAssociatedMetadataFileName(const char *pszFileName,
                                                   bool &bIsAMiraMonFile);
    static int IdentifySubdataSetFile(const CPLString pszFileName);
    static int IdentifyFile(GDALOpenInfo *poOpenInfo);
    static int UpdateDataTypeAndBytesPerPixel(const char *pszCompType,
                                              MMDataType *nCompressionType,
                                              MMBytesPerPixel *nBytesPerPixel);
    CPLString GetMetadataValue(const CPLString osMainSection,
                               const CPLString osSubSection,
                               const CPLString osSubSubSection,
                               const CPLString osKey) const;
    CPLString GetMetadataValue(const CPLString osMainSection,
                               const CPLString osSubSection,
                               const CPLString osKey) const;
    CPLString GetMetadataValue(const CPLString osSection,
                               const CPLString osKey) const;
    static CPLString GetMetadataValueDirectly(const char *pszRELFile,
                                              const char *pszSection,
                                              const char *pszKey);
    const char *GetRELNameChar() const;
    static CPLString MMRGetFileNameFromRelName(const char *pszRELFile);
    void UpdateRELNameChar(CPLString osRelFileNameIn);
    static CPLErr ParseBandInfo(MMRInfo &hMMR);
    static CPLString
    RemoveWhitespacesFromEndOfString(CPLString osInputWithSpaces);
    int GetColumnsNumberFromREL() const;
    int GetRowsNumberFromREL() const;

    explicit MMRRel(CPLString);
    ~MMRRel();

  private:
    CPLString osRelFileName;

    static CPLErr CheckBandInRel(const char *pszRELFileName,
                                 const char *pszIMGFile);
    static CPLString MMRGetSimpleMetadataName(const char *pszLayerName);
    static bool SameFile(CPLString osFile1, CPLString osFile2);
    static MMRNomFitxerState
    MMRStateOfNomFitxerInSection(const char *pszLayerName,
                                 const char *pszSection,
                                 const char *pszRELFile);
    static CPLString MMRGetAReferenceToIMGFile(const char *pszLayerName,
                                               const char *pszRELFile,
                                               bool &bIsAMiraMonFile);
};

#endif /* ndef MMR_REL_H_INCLUDED */
