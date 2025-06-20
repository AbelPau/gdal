/******************************************************************************
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Private class declarations for the MiraMonRaster classes used
 *           to read MiraMon (.img) files.  Public (C callable) declarations
 *           are in miramon.h.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Intergraph Corporation
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MMR_P_H_INCLUDED
#define MMR_P_H_INCLUDED

#include "cpl_port.h"
#include "miramon.h"

#include <cstdio>
#include <memory>
#include <vector>
#include <array>
#include <set>

#include "cpl_error.h"
#include "cpl_vsi.h"
#include "ogr_spatialref.h"

#ifdef CPL_LSB
#define MMRStandard(n, p)                                                      \
    {                                                                          \
    }
#else
void MMRStandard(int, void *);
#endif

#include "miramon.h"
#include "miramonrel.h"

#ifdef MSVC
#include "..\miramon_common\mm_gdal_constants.h"
#else
#include "../miramon_common/mm_gdal_constants.h"
#endif

class MMRBand;
class MMRDictionary;
class MMREntry;
class MMRSpillFile;
class MMRType;

constexpr auto pszExtRaster = ".img";
constexpr auto pszExtRasterREL = "I.rel";

/************************************************************************/
/*      Flag indicating read/write, or read-only access to data.        */
/************************************************************************/
enum class MMRAccess
{
    /*! Read only (no update) access */ MMR_ReadOnly = 0,
    /*! Read/write access. */ MMR_Update = 1
};

/************************************************************************/
/*                              MMRInfo_t                               */
/*                                                                      */
/*      This is just a structure, and used hold info about the whole    */
/*      dataset within miramonopen.cpp                                  */
/************************************************************************/
struct mmrinfo
{
    VSILFILE *fp = nullptr;

    CPLString osRELFileName;
    MMRRel *fRel = nullptr;  // Access stuff to REL file

    MMRAccess eAccess;

    GUInt32 nEndOfFile;
    GUInt32 nRootPos;
    GUInt32 nDictionaryPos;

    GInt16 nEntryHeaderLength;
    GInt32 nVersion;

    bool bTreeDirty;
    MMREntry *poRoot = nullptr;

    MMRDictionary *poDictionary = nullptr;
    char *pszDictionary = nullptr;

    int nXSize;
    int nYSize;

    // List of rawBandNames in a subdataset
    std::vector<CPLString> papoSDSBands;

    int nBands;
    MMRBand **papoBand = nullptr;

    void *pMapInfo = nullptr;
    void *pDatum = nullptr;
    void *pProParameters = nullptr;

    //struct mmrinfo *psDependent;
};

GUInt32 MMRAllocateSpace(MMRInfo_t *, GUInt32);
//bool MMRCreateSpillStack(MMRInfo_t *, int nXSize, int nYSize, int nLayers,
//                         int nBlockSize, EPTType eDataType,
//                         GIntBig *pnValidFlagsOffset, GIntBig *pnDataOffset);

const char *const *GetMMRAuxMetaDataList();

double *MMRReadBFUniqueBins(MMREntry *poBinFunc, int nPCTColors);

int CPL_DLL MMRCreateLayer(MMRHandle psInfo, MMREntry *poParent,
                           const char *pszLayerName, int bOverview,
                           int nBlockSize, int bCreateCompressed,
                           int bCreateLargeRaster, int bDependentLayer,
                           int nXSize, int nYSize, EPTType eDataType,
                           char **papszOptions,

                           // These are only related to external (large) files.
                           //GIntBig nStackValidFlagsOffset,
                           //GIntBig nStackDataOffset,
                           int nStackCount, int nStackIndex);

/************************************************************************/
/*                               MMRBand                                */
/************************************************************************/

class MMRBand
{
    VSILFILE *pfIMG;  // Point to IMG file
    MMRRel *pfRel;    // Rel where metadata is readed from

    int nBlocks;

    // Used for single-file modification.
    vsi_l_offset *panBlockStart;
    int *panBlockSize;
    int *panBlockFlag;

    // Used for spill-file modification.
    vsi_l_offset nBlockStart;
    vsi_l_offset nBlockSize;
    int nLayerStackCount;
    int nLayerStackIndex;

    // indexed-RLE format
    std::vector<vsi_l_offset> aFileOffsets;

#define BFLG_VALID 0x01
#define BFLG_COMPRESSED 0x02

    // Palette info
    std::array<std::vector<double>, 4> aadfPaletteColors;

    int nNoDataOriginalIndex;
    bool bPaletteHasNodata;

    std::array<std::vector<double>, 4> aadfPCT;
    int nNoDataPaletteIndex;

    // Assigned Subdataset for this band.
    int nAssignedSDS;

    // Section in REL file that give information about the band
    CPLString osBandSection;
    // REL file name that contains the band
    CPLString osRELFileName;
    // File name relative to REL file with banda data
    CPLString osRawBandFileName;
    // Friendly osRawBandFileName
    CPLString osBandFileName;
    // Name of the band documented in REL metadata file.
    CPLString osBandName;
    // Descripcion of the band, not the name
    CPLString osFriendlyDescription;

    MMDataType eMMDataType;
    MMBytesPerPixel eMMBytesPerPixel;
    bool bIsCompressed;

    // Min and Max values from metadata:  This value should correspond
    // to the actual minimum and maximum, not to an approximation.
    // However, MiraMon is proof to approximate values. The minimum
    // and maximum values are useful, for example, to properly scale
    // colors, etc.
    bool bMinSet;
    double dfMin;
    bool bMaxSet;
    double dfMax;
    //
    bool bMinVisuSet;
    double dfVisuMin;  // Key Color_ValorColor_0 in COLOR_TEXT
    bool bMaxVisuSet;
    double dfVisuMax;  // Key Color_ValorColor_n_1 COLOR_TEXT

    CPLString pszRefSystem;

    // Extent values of the band:
    // They always refer to extreme outer coordinates,
    // not to cell centers.
    double dfBBMinX;
    double dfBBMinY;
    double dfBBMaxX;
    double dfBBMaxY;

    // Resolution of the pixel
    double nResolution;

    CPLErr LoadBlockInfo();
    void ReAllocBlock(int iBlock, int nSize);

  public:
    MMRBand(MMRInfo_t *, const char *pszSection);
    ~MMRBand();

    const int Get_ATTRIBUTE_DATA_or_OVERVIEW_ASPECTES_TECNICS_int(
        const char *pszSection, const char *pszKey, int *nValue,
        const char *pszErrorMessage);
    const int GetDataTypeFromREL(const char *pszSection);
    const int GetResolutionFromREL(const char *pszSection);
    const int GetColumnsNumberFromREL(const char *pszSection);
    const int GetRowsNumberFromREL(const char *pszSection);
    void GetNoDataValue(const char *pszSection);
    void GetNoDataDefinitionFromREL(const char *pszSection);
    void GetBoundingBoxFromREL(const char *pszSection);
    void GetReferenceSystemFromREL();
    void GetMinMaxValuesFromREL(const char *pszSection);
    void GetMinMaxVisuValuesFromREL(const char *pszSection);
    void GetFriendlyDescriptionFromREL(const char *pszSection);

    const std::vector<double> &GetPCT_Red() const
    {
        return aadfPCT[0];
    }

    const std::vector<double> &GetPCT_Green() const
    {
        return aadfPCT[1];
    }

    const std::vector<double> &GetPCT_Blue() const
    {
        return aadfPCT[2];
    }

    const std::vector<double> &GetPCT_Alpha() const
    {
        return aadfPCT[3];
    }

    int GetAssignedSubDataSet()
    {
        return nAssignedSDS;
    }

    void AssignSubDataSet(int nAssignedSDSIn)
    {
        nAssignedSDS = nAssignedSDSIn;
    }

    const CPLString &GetBandName() const
    {
        return osBandName;
    }

    const CPLString &GetRELFileName() const
    {
        return osRELFileName;
    }

    void SetRELFileName(CPLString osRELFileNameIn)
    {
        osRELFileName = osRELFileNameIn;
    }

    const CPLString &GetRawBandFileName() const
    {
        return osRawBandFileName;
    }

    const CPLString &GetFriendlyDescription() const
    {
        return osFriendlyDescription;
    }

    MMDataType GeteMMDataType()
    {
        return eMMDataType;
    }

    MMBytesPerPixel GeteMMBytesPerPixel()
    {
        return eMMBytesPerPixel;
    }

    bool GetMinSet()
    {
        return bMinSet;
    }

    double GetMin()
    {
        return dfMin;
    }

    bool GetMaxSet()
    {
        return bMaxSet;
    }

    double GetMax()
    {
        return dfMax;
    }

    bool GetMinVisuSet()
    {
        return bMinVisuSet;
    }

    double GetVisuMin()
    {
        return dfVisuMin;
    }

    bool GetMaxVisuSet()
    {
        return bMaxVisuSet;
    }

    double GetVisuMax()
    {
        return dfVisuMax;
    }

    double GetBoundingBoxMinX()
    {
        return dfBBMinX;
    }

    double GetBoundingBoxMaxX()
    {
        return dfBBMaxX;
    }

    double GetBoundingBoxMinY()
    {
        return dfBBMinY;
    }

    double GetBoundingBoxMaxY()
    {
        return dfBBMaxY;
    }

    double GetPixelResolution()
    {
        return nResolution;
    }

    template <typename TYPE> CPLErr UncompressRow(void *rowBuffer);
    bool AcceptedDataType();
    CPLErr FillRowFromExtendedParam(void *rowBuffer);
    int PositionAtStartOfRowOffsetsInFile();
    bool FillRowOffsets();
    CPLErr GetRasterBlock(int nXBlock, int nYBlock, void *pData, int nDataSize);
    CPLErr SetRasterBlock(int nXBlock, int nYBlock, void *pData);

    void SetBandName(const char *pszName);
    CPLErr SetNoDataValue(double dfValue);

    void AssignRGBColor(int nIndexDstPalete, int nIndexSrcPalete);
    void AssignRGBColorDirectly(int nIndexDstPalete, double dfValue);
    CPLErr ConvertPaletteColors();
    CPLErr GetPCT();
    CPLErr GetPaletteColors_DBF_Indexs(struct MM_DATA_BASE_XP &oColorTable,
                                       MM_EXT_DBF_N_FIELDS &nClauSimbol,
                                       MM_EXT_DBF_N_FIELDS &nRIndex,
                                       MM_EXT_DBF_N_FIELDS &nGIndex,
                                       MM_EXT_DBF_N_FIELDS &nBIndex);
    CPLErr GetPaletteColors_DBF(CPLString os_Color_Paleta_DBF);
    CPLErr GetPaletteColors_PAL_P25_P65(CPLString os_Color_Paleta_DBF);
    CPLErr SetPCT(int, const double *, const double *, const double *,
                  const double *);

    MMRInfo_t *psInfo;

    EPTType eDataType;

    MMREntry *poNode;

    int nBlockXSize;
    int nBlockYSize;

    int nWidth;   // Number of columns
    int nHeight;  // Number of rows

    int nBlocksPerRow;
    int nBlocksPerColumn;

    bool bNoDataSet;         // There is nodata?
    CPLString pszNodataDef;  // Definition of nodata
    double dfNoData;         // Value of nodata
};

/************************************************************************/
/*                               MMREntry                               */
/*                                                                      */
/*      Base class for all entry types.  Most entry types do not        */
/*      have a subclass, and are just handled generically with this     */
/*      class.                                                          */
/************************************************************************/
class MMREntry
{
    bool bDirty;
    GUInt32 nFilePos;

    MMRInfo_t *psMMR;
    MMREntry *poParent;
    MMREntry *poPrev;

    GUInt32 nNextPos;
    MMREntry *poNext;

    GUInt32 nChildPos;
    MMREntry *poChild;

    char szName[64];
    char szType[32];

    MMRType *poType;

    GUInt32 nDataPos;
    GUInt32 nDataSize;
    GByte *pabyData;

    void LoadData();

    bool GetFieldValue(const char *, char, void *, int *pnRemainingDataSize);
    CPLErr SetFieldValue(const char *, char, void *);

    bool bIsMIFObject;

    MMREntry();
    MMREntry(const char *pszDictionary, const char *pszTypeName,
             int nDataSizeIn, GByte *pabyDataIn);
    std::vector<MMREntry *> FindChildren(const char *pszName,
                                         const char *pszType, int nRecLevel,
                                         int *pbErrorDetected);

  public:
    static MMREntry *New(MMRInfo_t *psMMR, GUInt32 nPos, MMREntry *poParent,
                         MMREntry *poPrev) CPL_WARN_UNUSED_RESULT;

    MMREntry(MMRInfo_t *psMMR, const char *pszNodeName, const char *pszTypeName,
             MMREntry *poParent);

    static MMREntry *New(MMRInfo_t *psMMR, const char *pszNodeName,
                         const char *pszTypeName,
                         MMREntry *poParent) CPL_WARN_UNUSED_RESULT;

    virtual ~MMREntry();

    static MMREntry *BuildEntryFromMIFObject(MMREntry *poContainer,
                                             const char *pszMIFObjectPath)
        CPL_WARN_UNUSED_RESULT;

    CPLErr RemoveAndDestroy();

    GUInt32 GetFilePos() const CPL_WARN_UNUSED_RESULT
    {
        return nFilePos;
    }

    const char *GetName() const CPL_WARN_UNUSED_RESULT
    {
        return szName;
    }

    void SetName(const char *pszNodeName);

    const char *GetType() const CPL_WARN_UNUSED_RESULT
    {
        return szType;
    }

    MMRType *GetTypeObject() CPL_WARN_UNUSED_RESULT;

    GByte *GetData() CPL_WARN_UNUSED_RESULT
    {
        LoadData();
        return pabyData;
    }

    GUInt32 GetDataPos() const CPL_WARN_UNUSED_RESULT
    {
        return nDataPos;
    }

    GUInt32 GetDataSize() const CPL_WARN_UNUSED_RESULT
    {
        return nDataSize;
    }

    MMREntry *GetChild() CPL_WARN_UNUSED_RESULT;
    MMREntry *GetNext() CPL_WARN_UNUSED_RESULT;
    MMREntry *GetNamedChild(const char *) CPL_WARN_UNUSED_RESULT;
    std::vector<MMREntry *>
    FindChildren(const char *pszName,
                 const char *pszType) CPL_WARN_UNUSED_RESULT;

    GInt32 GetIntField(const char *, CPLErr * = nullptr) CPL_WARN_UNUSED_RESULT;
    double GetDoubleField(const char *,
                          CPLErr * = nullptr) CPL_WARN_UNUSED_RESULT;
    const char *
    GetStringField(const char *, CPLErr * = nullptr,
                   int *pnRemainingDataSize = nullptr) CPL_WARN_UNUSED_RESULT;
    GIntBig GetBigIntField(const char *,
                           CPLErr * = nullptr) CPL_WARN_UNUSED_RESULT;
    int GetFieldCount(const char *, CPLErr * = nullptr) CPL_WARN_UNUSED_RESULT;

    CPLErr SetIntField(const char *, int);
    CPLErr SetDoubleField(const char *, double);
    CPLErr SetStringField(const char *, const char *);

    void DumpFieldValues(FILE *, const char * = nullptr);

    void SetPosition();
    CPLErr FlushToDisk();

    void MarkDirty();
    GByte *MakeData(int nSize = 0);
};

/************************************************************************/
/*                               MMRField                               */
/*                                                                      */
/*      A field in a MMRType in the dictionary.                         */
/************************************************************************/

class MMRField
{
  public:
    int nBytes;

    int nItemCount;
    // TODO(schwehr): Rename chPointer to something more meaningful.
    // It's not a pointer.
    char chPointer;   // '\0', '*' or 'p'
    char chItemType;  // 1|2|4|e|...

    char *pszItemObjectType;  // if chItemType == 'o'
    MMRType *poItemObjectType;

    char **papszEnumNames;  // Normally NULL if not an enum.

    char *pszFieldName;

    char szNumberString[36];  // Buffer used to return int as a string.

    MMRField();
    ~MMRField();

    const char *Initialize(const char *);

    bool CompleteDefn(MMRDictionary *);

    void Dump(FILE *);

    bool ExtractInstValue(const char *pszField, int nIndexValue,
                          GByte *pabyData, GUInt32 nDataOffset, int nDataSize,
                          char chReqType, void *pReqReturn,
                          int *pnRemainingDataSize = nullptr);

    CPLErr SetInstValue(const char *pszField, int nIndexValue, GByte *pabyData,
                        GUInt32 nDataOffset, int nDataSize, char chReqType,
                        void *pValue);

    void DumpInstValue(FILE *fpOut, GByte *pabyData, GUInt32 nDataOffset,
                       int nDataSize, const char *pszPrefix = nullptr);

    int GetInstBytes(GByte *, int, std::set<MMRField *> &oVisitedFields);
    int GetInstCount(GByte *pabyData, int nDataSize) const;
};

/************************************************************************/
/*                               MMRType                                */
/*                                                                      */
/*      A type in the dictionary.                                       */
/************************************************************************/

class MMRType
{
    bool bInCompleteDefn;

  public:
    int nBytes;

    std::vector<std::unique_ptr<MMRField>> apoFields;

    char *pszTypeName;

    MMRType();
    ~MMRType();

    const char *Initialize(const char *);

    bool CompleteDefn(MMRDictionary *);

    void Dump(FILE *);

    int GetInstBytes(GByte *, int, std::set<MMRField *> &oVisitedFields) const;
    int GetInstCount(const char *pszField, GByte *pabyData, GUInt32 nDataOffset,
                     int nDataSize);
    bool ExtractInstValue(const char *pszField, GByte *pabyData,
                          GUInt32 nDataOffset, int nDataSize, char chReqType,
                          void *pReqReturn, int *pnRemainingDataSize);
    CPLErr SetInstValue(const char *pszField, GByte *pabyData,
                        GUInt32 nDataOffset, int nDataSize, char chReqType,
                        void *pValue);
    void DumpInstValue(FILE *fpOut, GByte *pabyData, GUInt32 nDataOffset,
                       int nDataSize, const char *pszPrefix = nullptr) const;
};

/************************************************************************/
/*                            MMRDictionary                             */
/************************************************************************/

class MMRDictionary
{
  public:
    explicit MMRDictionary(const char *pszDict);
    ~MMRDictionary();

    MMRType *FindType(const char *);
    void AddType(MMRType *);

    static int GetItemSize(char);

    void Dump(FILE *);

  private:
    int nTypes;
    int nTypesMax;
    MMRType **papoTypes;

  public:
    // TODO(schwehr): Make these members private.
    CPLString osDictionaryText;
    bool bDictionaryTextDirty;
};

/************************************************************************/
/*                             MMRCompress                              */
/*                                                                      */
/*      Class that given a block of memory compresses the contents      */
/*      using run length encoding (RLE) as used by Imagine.             */
/************************************************************************/

class MMRCompress
{
  public:
    MMRCompress(void *pData, GUInt32 nBlockSize, EPTType eDataType);
    ~MMRCompress();

    // This is the method that does the work.
    bool compressBlock();

    // Static method to allow us to query whether MiraMon Raster type supported.
    static bool QueryDataTypeSupported(EPTType eMMRDataType);

    // Get methods - only valid after compressBlock has been called.
    GByte *getCounts() const
    {
        return m_pCounts;
    }

    GUInt32 getCountSize() const
    {
        return m_nSizeCounts;
    }

    GByte *getValues() const
    {
        return m_pValues;
    }

    GUInt32 getValueSize() const
    {
        return m_nSizeValues;
    }

    GUInt32 getMin() const
    {
        return m_nMin;
    }

    GUInt32 getNumRuns() const
    {
        return m_nNumRuns;
    }

    GByte getNumBits() const
    {
        return m_nNumBits;
    }

  private:
    static void makeCount(GUInt32 count, GByte *pCounter, GUInt32 *pnSizeCount);
    GUInt32 findMin(GByte *pNumBits);
    GUInt32 valueAsUInt32(GUInt32 index);
    void encodeValue(GUInt32 val, GUInt32 repeat);

    void *m_pData;
    GUInt32 m_nBlockSize;
    GUInt32 m_nBlockCount;
    EPTType m_eDataType;
    // The number of bits the datatype we are trying to compress takes.
    int m_nDataTypeNumBits;

    GByte *m_pCounts;
    GByte *m_pCurrCount;
    GUInt32 m_nSizeCounts;

    GByte *m_pValues;
    GByte *m_pCurrValues;
    GUInt32 m_nSizeValues;

    GUInt32 m_nMin;
    GUInt32 m_nNumRuns;
    // The number of bits needed to compress the range of values in the block.
    GByte m_nNumBits;
};

#endif /* ndef MMR_P_H_INCLUDED */
