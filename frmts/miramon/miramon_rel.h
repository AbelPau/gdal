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

class MMRRel;
class MMRBand;

/************************************************************************/
/*                         class MMRInfo                                */
/*                                                                      */
/*      A class that holds all information of a subdataset              */
/*      dataset within miramonopen.cpp                                  */
/************************************************************************/
class MMRInfo
{
  public:
    MMRInfo() = default;

    MMRInfo(const MMRInfo &) =
        delete;  // I don't want to construct a MMRInfo from another MMRInfo (effc++)
    MMRInfo &operator=(const MMRInfo &) =
        delete;  // I don't want to assing a MMRInfo to another MMRInfo (effc++)

    ~MMRInfo();
    /*
        When it is known that the file is a REL file (a
        format not used by any other driver), or if it is
        an IMG file accompanied by a sibling I.rel file that
        references this IMG file, special care must be taken.
        Since the Identify method returns UNKNOWN for IMG files,
        it is necessary to be cautious before assuming that any
        IMG file belongs to this driver.
    */
    bool bIsAMiraMonFile = false;

    CPLString osRELFileName = "";
    MMRRel *fRel = nullptr;  // Access stuff to REL file

    int nXSize = 0;
    int nYSize = 0;

    // List of rawBandNames in a subdataset
    std::vector<CPLString> papoSDSBands{};

    int nBands = 0;
    MMRBand **papoBand = nullptr;
};

class MMRRel
{
  public:
    CPLErr SetInfoFromREL(const char *pszFileName, MMRInfo &hMMR);
    static CPLString GetAssociatedMetadataFileName(const char *pszFileName,
                                                   bool &bIsAMiraMonFile);
    static int IdentifySubdataSetFile(const CPLString pszFileName);
    static int IdentifyFile(GDALOpenInfo *poOpenInfo);
    static int GetDataTypeAndBytesPerPixel(const char *pszCompType,
                                           MMDataType *nCompressionType,
                                           MMBytesPerPixel *nBytesPerPixel);
    CPLString GetMetadataValue(const CPLString osMainSection,
                               const CPLString osSubSection,
                               const CPLString osSubSubSection,
                               const CPLString osKey);
    CPLString GetMetadataValue(const CPLString osMainSection,
                               const CPLString osSubSection,
                               const CPLString osKey);
    CPLString GetMetadataValue(const CPLString osSection,
                               const CPLString osKey);
    static CPLString GetMetadataValueDirectly(const char *pszRELFile,
                                              const char *pszSection,
                                              const char *pszKey);
    const char *GetRELNameChar();
    static CPLString MMRGetFileNameFromRelName(const char *pszRELFile);
    void SetRELNameChar(CPLString osRelFileNameIn);
    static CPLErr ParseBandInfo(MMRInfo &hMMR);
    static CPLString
    RemoveWhitespacesFromEndOfString(CPLString osInputWithSpaces);

    explicit MMRRel(CPLString);
    ~MMRRel();

  private:
    CPLString osRelFileName;

    static CPLErr CheckBandInRel(const char *pszRELFileName,
                                 const char *pszIMGFile);
    static CPLString MMRGetSimpleMetadataName(const char *pszLayerName);
    static MMRNomFitxerState
    MMRStateOfNomFitxerInSection(const char *pszLayerName,
                                 const char *pszSection,
                                 const char *pszRELFile);
    static CPLString MMRGetAReferenceToIMGFile(const char *pszLayerName,
                                               const char *pszRELFile,
                                               bool &bIsAMiraMonFile);
};

#endif /* ndef MMR_REL_H_INCLUDED */
