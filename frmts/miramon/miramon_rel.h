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
#include "miramon_band.h"  // For MMRBand

constexpr auto pszExtRaster = ".img";
constexpr auto pszExtRasterREL = "I.rel";
constexpr auto pszExtREL = ".rel";

class MMRBand;

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

class MMRRel
{
  public:
    explicit MMRRel(CPLString);
    MMRRel(const MMRRel &) =
        delete;  // I don't want to construct a MMRDataset from another MMRDataset (effc++)
    MMRRel &operator=(const MMRRel &) =
        delete;  // I don't want to assing a MMRDataset to another MMRDataset (effc++)
    ~MMRRel();

    static CPLString GetAssociatedMetadataFileName(const char *pszFileName,
                                                   bool &bIsAMiraMonFile);
    static int IdentifySubdataSetFile(const CPLString pszFileName);
    static int IdentifyFile(GDALOpenInfo *poOpenInfo);
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

    static CPLString MMRGetFileNameFromRelName(const char *pszRELFile);
    void UpdateRELNameChar(CPLString osRelFileNameIn);
    CPLErr ParseBandInfo();
    static CPLString
    RemoveWhitespacesFromEndOfString(CPLString osInputWithSpaces);
    int GetColumnsNumberFromREL() const;
    int GetRowsNumberFromREL() const;

    char GetIsValid() const
    {
        return bIsValid;
    }

    void SetIsValid(bool bIsValidIn)
    {
        bIsValid = bIsValidIn;
    }

    const char *GetRELNameChar() const
    {
        return osRelFileName.c_str();
    }

    const char *GetRELName() const
    {
        return osRelFileName;
    }

    int GetXSize() const
    {
        return nXSize;
    }

    int GetYSize() const
    {
        return nYSize;
    }

    int GetNBands() const
    {
        return nBands;
    }

    MMRBand **GetBands() const
    {
        return papoBand;
    }

    MMRBand *GetBand(int nIBand) const
    {
        if (nIBand < 0 || nIBand > nBands)
        {
            CPLAssert(false);
            return nullptr;
        }
        return papoBand[nIBand];
    }

    int isAMiraMonFile() const
    {
        return bIsAMiraMonFile;
    }

    void SetXSize(int nXSizeIn)
    {
        nXSize = nXSizeIn;
    }

    void SetYSize(int nYSizeIn)
    {
        nYSize = nYSizeIn;
    }

  private:
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

    CPLString osRelFileName = "";
    bool bIsValid = false;  // Determines if the created object is valid or not.
    bool bIsAMiraMonFile = false;

    int nXSize = 0;
    int nYSize = 0;

    // List of rawBandNames in a subdataset
    std::vector<CPLString> papoSDSBands{};

    int nBands = 0;
    MMRBand **papoBand = nullptr;
};

#endif /* ndef MMR_REL_H_INCLUDED */
