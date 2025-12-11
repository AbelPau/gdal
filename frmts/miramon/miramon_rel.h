/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements MMRRel: provides access to the REL file, which
 *           holds all the necessary metadata to correctly interpret and
 *           access the associated raw data.
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

#ifdef MSVC
#include "..\miramon_common\mm_gdal_driver_structs.h"  // For SECTION_VERSIO
#else
#include "../miramon_common/mm_gdal_driver_structs.h"  // For SECTION_VERSIO
#endif

constexpr auto pszExtRaster = ".img";
constexpr auto pszExtRasterREL = "I.rel";
constexpr auto pszExtREL = ".rel";
constexpr auto LineReturn = "\r\n";

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

using ExcludedEntry = std::pair<CPLString, CPLString>;

class MMRRel
{
  public:
    MMRRel(const CPLString &, bool);  // Used in reading
    MMRRel(const CPLString &, const CPLString &osEPSG, int nWidth, int nHeight,
           double dfMinX, double dfMaxX, double dfMinY, double dfMaxY,
           std::vector<MMRBand> &&oBands);  // Used in writing
    MMRRel(const MMRRel &) =
        delete;  // I don't want to construct a MMRDataset from another MMRDataset (effc++)
    MMRRel &operator=(const MMRRel &) =
        delete;  // I don't want to assign a MMRDataset to another MMRDataset (effc++)
    ~MMRRel();

    static CPLString
    GetValueFromSectionKeyPriorToREL(const CPLString &osPriorRelName,
                                     const CPLString &osSection,
                                     const CPLString &osKey);
    CPLString GetValueFromSectionKeyFromREL(const CPLString &osSection,
                                            const CPLString &osKey);
    static CPLString GetValueFromSectionKey(VSILFILE *pf,
                                            const CPLString &osSection,
                                            const CPLString &osKey);
    bool GetMetadataValue(const CPLString &osMainSection,
                          const CPLString &osSubSection,
                          const CPLString &osSubSubSection,
                          const CPLString &osKey, CPLString &osValue);
    bool GetMetadataValue(const CPLString &osMainSection,
                          const CPLString &osSubSection, const CPLString &osKey,
                          CPLString &osValue);
    bool GetMetadataValue(const CPLString &osSection, const CPLString &osKey,
                          CPLString &osValue);
    bool GetAndExcludeMetadataValueDirectly(const CPLString &osRELFile,
                                            const CPLString &osSection,
                                            const CPLString &osKey,
                                            CPLString &osValue);
    static bool GetMetadataValueDirectly(const CPLString &osRELFile,
                                         const CPLString &osSection,
                                         const CPLString &osKey,
                                         CPLString &osValue);
    void RELToGDALMetadata(GDALDataset *poDS);

    static CPLString MMRGetFileNameFromRelName(const CPLString &osRELFile);
    int GetColumnsNumberFromREL();
    int GetRowsNumberFromREL();
    static int IdentifySubdataSetFile(const CPLString &osFileName);
    static int IdentifyFile(const GDALOpenInfo *poOpenInfo);

    bool Write();
    void WriteMETADADES();
    void WriteIDENTIFICATION();
    void WriteOVERVIEW_ASPECTES_TECNICS();
    void WriteSPATIAL_REFERENCE_SYSTEM_HORIZONTAL();
    void WriteEXTENT();
    void WriteOVERVIEW();
    void WriteATTRIBUTE_DATA();

    // Used when writting bands. If dimensions are the same
    // for all bands, then they has been written in the main section
    bool GetDimWrittenInOverview() const
    {
        return m_bDimWrittenInOverview;
    }

    // Used when writting bands. If data type is the same
    // for all bands, then it has been written in the main section
    bool GetDataTypeWrittenInAtributeData() const
    {
        return m_bDataTypeWrittenInAtributeData;
    }

    bool IsValid() const
    {
        return m_bIsValid;
    }

    void SetIsValid(bool bIsValidIn)
    {
        m_bIsValid = bIsValidIn;
    }

    VSILFILE *GetRELFile() const
    {
        return m_pRELFile;
    }

    bool OpenRELFile()
    {
        if (m_osRelFileName.empty())
            return false;

        m_pRELFile = VSIFOpenL(m_osRelFileName, "rb");
        if (m_pRELFile)
            return true;
        return false;
    }

    bool CreateRELFile()
    {
        if (m_osRelFileName.empty())
            return false;

        m_pRELFile = VSIFOpenL(m_osRelFileName, "wb");
        if (m_pRELFile)
            return true;
        return false;
    }

    void AddVersion()
    {
        if (!m_pRELFile)
            return;

        // Writing MiraMon version section
        VSIFPrintfL(m_pRELFile, "[%s]%s", SECTION_VERSIO, LineReturn);

        VSIFPrintfL(m_pRELFile, "%s=%u%s", KEY_VersMetaDades,
                    static_cast<unsigned>(MM_VERS_METADADES), LineReturn);
        VSIFPrintfL(m_pRELFile, "%s=%u%s", KEY_SubVersMetaDades,
                    static_cast<unsigned>(MM_SUBVERS_METADADES), LineReturn);

        VSIFPrintfL(m_pRELFile, "%s=%u%s", KEY_Vers,
                    static_cast<unsigned>(MM_VERS), LineReturn);
        VSIFPrintfL(m_pRELFile, "%s=%u%s", KEY_SubVers,
                    static_cast<unsigned>(MM_SUBVERS), LineReturn);

        VSIFPrintfL(m_pRELFile, "%s", LineReturn);
    }

    void AddSectionStart(const CPLString osSection)
    {
        if (!m_pRELFile)
            return;

        VSIFPrintfL(m_pRELFile, "[%s]%s", osSection.c_str(), LineReturn);
    }

    void AddSectionStart(const CPLString osSectionP1,
                         const CPLString osSectionP2)
    {
        if (!m_pRELFile)
            return;

        VSIFPrintfL(m_pRELFile, "[%s:%s]%s", osSectionP1.c_str(),
                    osSectionP2.c_str(), LineReturn);
    }

    void AddSectionEnd()
    {
        if (!m_pRELFile)
            return;

        VSIFPrintfL(m_pRELFile, LineReturn);
    }

    void AddKey(const CPLString osKey)
    {
        if (!m_pRELFile)
            return;

        char *pzsKey = CPLRecode(osKey, CPL_ENC_UTF8, "CP1252");
        VSIFPrintfL(m_pRELFile, "%s=%s", pzsKey ? pzsKey : osKey.c_str(),
                    LineReturn);
        CPLFree(pzsKey);
    }

    void AddKeyValue(const CPLString osKey, const CPLString osValue)
    {
        if (!m_pRELFile)
            return;

        char *pzsKey = CPLRecode(osKey, CPL_ENC_UTF8, "CP1252");
        char *pzsValue = CPLRecode(osValue, CPL_ENC_UTF8, "CP1252");
        VSIFPrintfL(m_pRELFile, "%s=%s%s", pzsKey ? pzsKey : osKey.c_str(),
                    pzsValue ? pzsValue : osValue.c_str(), LineReturn);

        CPLFree(pzsKey);
        CPLFree(pzsValue);
    }

    void AddKeyValue(const CPLString osKey, const int nValue)
    {
        if (!m_pRELFile)
            return;

        char *pzsKey = CPLRecode(osKey, CPL_ENC_UTF8, "CP1252");
        VSIFPrintfL(m_pRELFile, "%s=%d%s", pzsKey ? pzsKey : osKey.c_str(),
                    nValue, LineReturn);

        CPLFree(pzsKey);
    }

    void AddKeyValue(const CPLString osKey, const double nValue)
    {
        if (!m_pRELFile)
            return;

        char *pzsKey = CPLRecode(osKey, CPL_ENC_UTF8, "CP1252");
        VSIFPrintfL(m_pRELFile, "%s=%lf%s", pzsKey ? pzsKey : osKey.c_str(),
                    nValue, LineReturn);

        CPLFree(pzsKey);
    }

    void CloseRELFile()
    {
        if (!m_pRELFile)
            return;

        VSIFCloseL(m_pRELFile);
        m_pRELFile = nullptr;
    }

    const char *GetRELNameChar() const
    {
        return m_osRelFileName.c_str();
    }

    CPLString GetRELName() const
    {
        return m_osRelFileName;
    }

    int GetNBands() const
    {
        return m_nBands;
    }

    MMRBand *GetBand(int nIBand)
    {
        if (nIBand < 0 || nIBand >= m_nBands)
            return nullptr;

        return &m_oBands[nIBand];
    }

    int isAMiraMonFile() const
    {
        return m_bIsAMiraMonFile;
    }

    void addExcludedSectionKey(const CPLString &section, const CPLString &key)
    {
        m_ExcludedSectionKey.emplace(section, key);
    }

    std::set<ExcludedEntry> GetExcludedMetadata() const
    {
        return m_ExcludedSectionKey;
    }

  private:
    static CPLErr CheckBandInRel(const CPLString &osRELFileName,
                                 const CPLString &osIMGFile);
    static CPLString MMRGetSimpleMetadataName(const CPLString &osLayerName);
    static bool SameFile(const CPLString &osFile1, const CPLString &osFile2);
    MMRNomFitxerState MMRStateOfNomFitxerInSection(const CPLString &osLayerName,
                                                   const CPLString &osSection,
                                                   const CPLString &osRELFile,
                                                   bool bNomFitxerMustExist);
    CPLString MMRGetAReferenceToIMGFile(const CPLString &osLayerName,
                                        const CPLString &osRELFile);

    CPLString GetAssociatedMetadataFileName(const CPLString &osFileName);

    void UpdateRELNameChar(const CPLString &osRelFileNameIn);
    CPLErr ParseBandInfo();

    CPLString m_osRelFileName = "";
    VSILFILE *m_pRELFile = nullptr;
    static CPLString m_szImprobableRELChain;

    char m_szFileIdentifier[MM_MAX_LEN_LAYER_IDENTIFIER];

    bool m_bIsValid =
        false;  // Determines if the created object is valid or not.
    bool m_bIsAMiraMonFile = false;

    // List of rawBandNames in a subdataset
    std::vector<CPLString> m_papoSDSBands{};

    int m_nBands = 0;
    std::vector<MMRBand> m_oBands{};

    // Used when writting bands. If dimensions are the same
    // for all bands, then they has been written in the main section
    bool m_bDimWrittenInOverview = FALSE;

    // Used when writting bands. If data type is the same
    // for all bands, then it has been written in the main section
    bool m_bDataTypeWrittenInAtributeData = TRUE;

    // Preserving metadata

    // Domain
    static constexpr const char *m_kMetadataDomain = "MIRAMON";

    // Used to join Section and Key in a single
    // name for SetMetadataItem(Name, Value)
    static constexpr const char *m_SecKeySeparator = "[$$$]";

    // List of excluded pairs {Section, Key} to be added to metadata
    // Empty Key means all section
    std::set<ExcludedEntry> m_ExcludedSectionKey = {};

    // For writing part
    // EPSG number
    CPLString m_osEPSG = "";

    // Global raster dimensions
    int m_nWidth = 0;
    int m_nHeight = 0;

    double m_dfMinX = MM_UNDEFINED_STATISTICAL_VALUE;
    double m_dfMaxX = -MM_UNDEFINED_STATISTICAL_VALUE;
    double m_dfMinY = MM_UNDEFINED_STATISTICAL_VALUE;
    double m_dfMaxY = -MM_UNDEFINED_STATISTICAL_VALUE;
};

#endif /* ndef MMR_REL_H_INCLUDED */
