/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Convenience functions declarations.
 *           This is intended to remain light weight.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_CONV_H_INCLUDED
#define CPL_CONV_H_INCLUDED

#include "cpl_port.h"
#include "cpl_vsi.h"
#include "cpl_error.h"

#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS)
#include <cstdint>
#endif

/**
 * \file cpl_conv.h
 *
 * Various convenience functions for CPL.
 *
 */

/* -------------------------------------------------------------------- */
/*      Runtime check of various configuration items.                   */
/* -------------------------------------------------------------------- */
CPL_C_START

/*! @cond Doxygen_Suppress */
void CPL_DLL CPLVerifyConfiguration(void);
/*! @endcond */

bool CPL_DLL CPLIsDebugEnabled(void);

const char CPL_DLL *CPL_STDCALL CPLGetConfigOption(const char *, const char *)
    CPL_WARN_UNUSED_RESULT;
const char CPL_DLL *CPL_STDCALL CPLGetThreadLocalConfigOption(
    const char *, const char *) CPL_WARN_UNUSED_RESULT;
const char CPL_DLL *CPL_STDCALL
CPLGetGlobalConfigOption(const char *, const char *) CPL_WARN_UNUSED_RESULT;
void CPL_DLL CPL_STDCALL CPLSetConfigOption(const char *, const char *);
void CPL_DLL CPL_STDCALL CPLSetThreadLocalConfigOption(const char *pszKey,
                                                       const char *pszValue);
void CPL_DLL CPLDeclareKnownConfigOption(const char *pszKey,
                                         const char *pszDefinition);
char CPL_DLL **CPLGetKnownConfigOptions(void);

/** Callback for CPLSubscribeToSetConfigOption() */
typedef void (*CPLSetConfigOptionSubscriber)(const char *pszKey,
                                             const char *pszValue,
                                             bool bThreadLocal,
                                             void *pUserData);
int CPL_DLL CPLSubscribeToSetConfigOption(
    CPLSetConfigOptionSubscriber pfnCallback, void *pUserData);
void CPL_DLL CPLUnsubscribeToSetConfigOption(int nSubscriberId);

/*! @cond Doxygen_Suppress */
void CPL_DLL CPL_STDCALL CPLFreeConfig(void);
/*! @endcond */
char CPL_DLL **CPLGetConfigOptions(void);
void CPL_DLL CPLSetConfigOptions(const char *const *papszConfigOptions);
char CPL_DLL **CPLGetThreadLocalConfigOptions(void);
void CPL_DLL
CPLSetThreadLocalConfigOptions(const char *const *papszConfigOptions);
void CPL_DLL CPLLoadConfigOptionsFromFile(const char *pszFilename,
                                          int bOverrideEnvVars);
void CPL_DLL CPLLoadConfigOptionsFromPredefinedFiles(void);

/* -------------------------------------------------------------------- */
/*      Safe malloc() API.  Thin cover over VSI functions with fatal    */
/*      error reporting if memory allocation fails.                     */
/* -------------------------------------------------------------------- */
void CPL_DLL *CPLMalloc(size_t) CPL_WARN_UNUSED_RESULT;
void CPL_DLL *CPLCalloc(size_t, size_t) CPL_WARN_UNUSED_RESULT;
void CPL_DLL *CPLRealloc(void *, size_t) CPL_WARN_UNUSED_RESULT;
char CPL_DLL *
CPLStrdup(const char *) CPL_WARN_UNUSED_RESULT CPL_RETURNS_NONNULL;
char CPL_DLL *CPLStrlwr(char *);

/** Alias of VSIFree() */
#define CPLFree VSIFree

/* -------------------------------------------------------------------- */
/*      Read a line from a text file, and strip of CR/LF.               */
/* -------------------------------------------------------------------- */
char CPL_DLL *CPLFGets(char *, int, FILE *);
const char CPL_DLL *CPLReadLine(FILE *);
const char CPL_DLL *CPLReadLineL(VSILFILE *);
const char CPL_DLL *CPLReadLine2L(VSILFILE *, int, CSLConstList);
const char CPL_DLL *CPLReadLine3L(VSILFILE *, int, int *, CSLConstList);

/* -------------------------------------------------------------------- */
/*      Convert ASCII string to floating point number                  */
/*      (THESE FUNCTIONS ARE NOT LOCALE AWARE!).                        */
/* -------------------------------------------------------------------- */
double CPL_DLL CPLAtof(const char *);
double CPL_DLL CPLAtofDelim(const char *, char);
double CPL_DLL CPLStrtod(const char *, char **);
double CPL_DLL CPLStrtodM(const char *, char **);
double CPL_DLL CPLStrtodDelim(const char *, char **, char);
float CPL_DLL CPLStrtof(const char *, char **);
float CPL_DLL CPLStrtofDelim(const char *, char **, char);

/* -------------------------------------------------------------------- */
/*      Convert number to string.  This function is locale agnostic     */
/*      (i.e. it will support "," or "." regardless of current locale)  */
/* -------------------------------------------------------------------- */
double CPL_DLL CPLAtofM(const char *);

/* -------------------------------------------------------------------- */
/*      Read a numeric value from an ASCII character string.            */
/* -------------------------------------------------------------------- */
char CPL_DLL *CPLScanString(const char *, int, int, int);
double CPL_DLL CPLScanDouble(const char *, int);
long CPL_DLL CPLScanLong(const char *, int);
unsigned long CPL_DLL CPLScanULong(const char *, int);
GUIntBig CPL_DLL CPLScanUIntBig(const char *, int);
GIntBig CPL_DLL CPLAtoGIntBig(const char *pszString);
GIntBig CPL_DLL CPLAtoGIntBigEx(const char *pszString, int bWarn,
                                int *pbOverflow);
void CPL_DLL *CPLScanPointer(const char *, int);

/* -------------------------------------------------------------------- */
/*      Print a value to an ASCII character string.                     */
/* -------------------------------------------------------------------- */
int CPL_DLL CPLPrintString(char *, const char *, int);
int CPL_DLL CPLPrintStringFill(char *, const char *, int);
int CPL_DLL CPLPrintInt32(char *, GInt32, int);
int CPL_DLL CPLPrintUIntBig(char *, GUIntBig, int);
int CPL_DLL CPLPrintDouble(char *, const char *, double, const char *);
int CPL_DLL CPLPrintTime(char *, int, const char *, const struct tm *,
                         const char *);
int CPL_DLL CPLPrintPointer(char *, void *, int);

#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS)
extern "C++"
{
    std::string CPL_DLL CPLFormatReadableFileSize(uint64_t nSizeInBytes);
    std::string CPL_DLL CPLFormatReadableFileSize(double dfSizeInBytes);
}
#endif

/* -------------------------------------------------------------------- */
/*      Fetch a function from DLL / so.                                 */
/* -------------------------------------------------------------------- */

void CPL_DLL *CPLGetSymbol(const char *, const char *);

/* -------------------------------------------------------------------- */
/*      Fetch executable path.                                          */
/* -------------------------------------------------------------------- */
int CPL_DLL CPLGetExecPath(char *pszPathBuf, int nMaxLength);

/* -------------------------------------------------------------------- */
/*      Filename handling functions.                                    */
/* -------------------------------------------------------------------- */

#if defined(DOXYGEN_SKIP) || !defined(__cplusplus) ||                          \
    !defined(GDAL_COMPILATION) ||                                              \
    (defined(__cplusplus) && defined(ALLOW_DEPRECATED_CPL_PATH_FUNCTIONS))
const char CPL_DLL *
CPLGetPath(const char *) CPL_WARN_UNUSED_RESULT CPL_RETURNS_NONNULL;
const char CPL_DLL *
CPLGetDirname(const char *) CPL_WARN_UNUSED_RESULT CPL_RETURNS_NONNULL;
const char CPL_DLL *
CPLGetBasename(const char *) CPL_WARN_UNUSED_RESULT CPL_RETURNS_NONNULL;
const char CPL_DLL *
CPLGetExtension(const char *) CPL_WARN_UNUSED_RESULT CPL_RETURNS_NONNULL;
const char CPL_DLL *CPLFormFilename(
    const char *pszPath, const char *pszBasename,
    const char *pszExtension) CPL_WARN_UNUSED_RESULT CPL_RETURNS_NONNULL;
const char CPL_DLL *CPLFormCIFilename(
    const char *pszPath, const char *pszBasename,
    const char *pszExtension) CPL_WARN_UNUSED_RESULT CPL_RETURNS_NONNULL;
const char CPL_DLL *CPLResetExtension(const char *, const char *)
    CPL_WARN_UNUSED_RESULT CPL_RETURNS_NONNULL;
const char CPL_DLL *CPLProjectRelativeFilename(const char *pszProjectDir,
                                               const char *pszSecondaryFilename)
    CPL_WARN_UNUSED_RESULT CPL_RETURNS_NONNULL;
const char CPL_DLL *
CPLCleanTrailingSlash(const char *) CPL_WARN_UNUSED_RESULT CPL_RETURNS_NONNULL;
const char CPL_DLL *CPLGenerateTempFilename(const char *pszStem)
    CPL_WARN_UNUSED_RESULT CPL_RETURNS_NONNULL;
const char CPL_DLL *CPLExpandTilde(const char *pszFilename)
    CPL_WARN_UNUSED_RESULT CPL_RETURNS_NONNULL;
const char CPL_DLL *
CPLLaunderForFilename(const char *pszName,
                      const char *pszOutputPath) CPL_WARN_UNUSED_RESULT;
#endif

char CPL_DLL *CPLGetCurrentDir(void);
const char CPL_DLL *
CPLGetFilename(const char *) CPL_WARN_UNUSED_RESULT CPL_RETURNS_NONNULL;
int CPL_DLL CPLIsFilenameRelative(const char *pszFilename);
const char CPL_DLL *CPLExtractRelativePath(const char *, const char *, int *)
    CPL_WARN_UNUSED_RESULT CPL_RETURNS_NONNULL;
char CPL_DLL **
CPLCorrespondingPaths(const char *pszOldFilename, const char *pszNewFilename,
                      char **papszFileList) CPL_WARN_UNUSED_RESULT;
int CPL_DLL CPLCheckForFile(char *pszFilename, char **papszSiblingList);

const char CPL_DLL *CPLGetHomeDir(void) CPL_WARN_UNUSED_RESULT;

#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS)

extern "C++"
{
    std::string CPL_DLL CPLGetPathSafe(const char *) CPL_WARN_UNUSED_RESULT;
    std::string CPL_DLL CPLGetDirnameSafe(const char *) CPL_WARN_UNUSED_RESULT;
    std::string CPL_DLL CPLGetBasenameSafe(const char *) CPL_WARN_UNUSED_RESULT;
    std::string CPL_DLL CPLGetExtensionSafe(const char *)
        CPL_WARN_UNUSED_RESULT;
    std::string CPL_DLL CPLFormFilenameSafe(
        const char *pszPath, const char *pszBasename,
        const char *pszExtension = nullptr) CPL_WARN_UNUSED_RESULT;
    std::string CPL_DLL CPLFormCIFilenameSafe(
        const char *pszPath, const char *pszBasename,
        const char *pszExtension = nullptr) CPL_WARN_UNUSED_RESULT;
    std::string CPL_DLL CPLResetExtensionSafe(const char *, const char *)
        CPL_WARN_UNUSED_RESULT;
    std::string CPL_DLL CPLProjectRelativeFilenameSafe(
        const char *pszProjectDir,
        const char *pszSecondaryFilename) CPL_WARN_UNUSED_RESULT;
    std::string CPL_DLL CPLCleanTrailingSlashSafe(const char *pszPath)
        CPL_WARN_UNUSED_RESULT;
    std::string CPL_DLL CPLGenerateTempFilenameSafe(const char *pszStem)
        CPL_WARN_UNUSED_RESULT;
    std::string CPL_DLL CPLExpandTildeSafe(const char *pszFilename)
        CPL_WARN_UNUSED_RESULT;
    std::string CPL_DLL CPLLaunderForFilenameSafe(
        const char *pszName, const char *pszOutputPath) CPL_WARN_UNUSED_RESULT;
}

#endif  // defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS)

/* -------------------------------------------------------------------- */
/*      Find File Function                                              */
/* -------------------------------------------------------------------- */

/** Callback for CPLPushFileFinder */
typedef char const *(*CPLFileFinder)(const char *, const char *);

const char CPL_DLL *CPLFindFile(const char *pszClass, const char *pszBasename);
const char CPL_DLL *CPLDefaultFindFile(const char *pszClass,
                                       const char *pszBasename);
void CPL_DLL CPLPushFileFinder(CPLFileFinder pfnFinder);
CPLFileFinder CPL_DLL CPLPopFileFinder(void);
void CPL_DLL CPLPushFinderLocation(const char *);
void CPL_DLL CPLPopFinderLocation(void);
void CPL_DLL CPLFinderClean(void);

/* -------------------------------------------------------------------- */
/*      Safe version of stat() that works properly on stuff like "C:".  */
/* -------------------------------------------------------------------- */
int CPL_DLL CPLStat(const char *, VSIStatBuf *) CPL_WARN_UNUSED_RESULT;

/* -------------------------------------------------------------------- */
/*      Reference counted file handle manager.  Makes sharing file      */
/*      handles more practical.                                         */
/* -------------------------------------------------------------------- */

/** Information on a shared file */
typedef struct
{
    FILE *fp;          /**< File pointer */
    int nRefCount;     /**< Reference counter */
    int bLarge;        /**< Whether fp must be interpreted as VSIFILE* */
    char *pszFilename; /**< Filename */
    char *pszAccess;   /**< Access mode */
} CPLSharedFileInfo;

FILE CPL_DLL *CPLOpenShared(const char *, const char *, int);
void CPL_DLL CPLCloseShared(FILE *);
CPLSharedFileInfo CPL_DLL *CPLGetSharedList(int *);
void CPL_DLL CPLDumpSharedList(FILE *);
/*! @cond Doxygen_Suppress */
void CPL_DLL CPLCleanupSharedFileMutex(void);
/*! @endcond */

/* -------------------------------------------------------------------- */
/*      DMS to Dec to DMS conversion.                                   */
/* -------------------------------------------------------------------- */
double CPL_DLL CPLDMSToDec(const char *is);
const char CPL_DLL *CPLDecToDMS(double dfAngle, const char *pszAxis,
                                int nPrecision);
double CPL_DLL CPLPackedDMSToDec(double);
double CPL_DLL CPLDecToPackedDMS(double dfDec);

CPLErr CPL_DLL CPLStringToComplex(const char *pszString, double *pdfReal,
                                  double *pdfImag);

/* -------------------------------------------------------------------- */
/*      Misc other functions.                                           */
/* -------------------------------------------------------------------- */
int CPL_DLL CPLUnlinkTree(const char *);
int CPL_DLL CPLCopyFile(const char *pszNewPath, const char *pszOldPath);
int CPL_DLL CPLCopyTree(const char *pszNewPath, const char *pszOldPath);
int CPL_DLL CPLMoveFile(const char *pszNewPath, const char *pszOldPath);
int CPL_DLL CPLSymlink(const char *pszOldPath, const char *pszNewPath,
                       CSLConstList papszOptions);
int CPL_DLL CPLGetRemainingFileDescriptorCount(void);

/* -------------------------------------------------------------------- */
/*      Lock related functions.                                         */
/* -------------------------------------------------------------------- */

/** Return code of CPLLockFileEx(). */
typedef enum
{
    CLFS_OK,                     /**< CPLLockFileEx() succeeded. */
    CLFS_CANNOT_CREATE_LOCK,     /**< Lock file creation failed. */
    CLFS_LOCK_BUSY,              /**< Lock already taken (and still alive). */
    CLFS_API_MISUSE,             /**< API misuse. */
    CLFS_THREAD_CREATION_FAILED, /**< Thread creation failed. */
} CPLLockFileStatus;

/** Handle type returned by CPLLockFileEx(). */
typedef struct CPLLockFileStruct *CPLLockFileHandle;

CPLLockFileStatus CPL_DLL CPLLockFileEx(const char *pszLockFileName,
                                        CPLLockFileHandle *phLockFileHandle,
                                        CSLConstList papszOptions);

void CPL_DLL CPLUnlockFileEx(CPLLockFileHandle hLockFileHandle);

/* -------------------------------------------------------------------- */
/*      ZIP Creation.                                                   */
/* -------------------------------------------------------------------- */

/*! @cond Doxygen_Suppress */
#define CPL_ZIP_API_OFFERED
/*! @endcond */
void CPL_DLL *CPLCreateZip(const char *pszZipFilename, char **papszOptions);
CPLErr CPL_DLL CPLCreateFileInZip(void *hZip, const char *pszFilename,
                                  char **papszOptions);
CPLErr CPL_DLL CPLWriteFileInZip(void *hZip, const void *pBuffer,
                                 int nBufferSize);
CPLErr CPL_DLL CPLCloseFileInZip(void *hZip);
CPLErr CPL_DLL CPLAddFileInZip(void *hZip, const char *pszArchiveFilename,
                               const char *pszInputFilename, VSILFILE *fpInput,
                               CSLConstList papszOptions,
                               GDALProgressFunc pProgressFunc,
                               void *pProgressData);
CPLErr CPL_DLL CPLCloseZip(void *hZip);

/* -------------------------------------------------------------------- */
/*      ZLib compression                                                */
/* -------------------------------------------------------------------- */

void CPL_DLL *CPLZLibDeflate(const void *ptr, size_t nBytes, int nLevel,
                             void *outptr, size_t nOutAvailableBytes,
                             size_t *pnOutBytes);
void CPL_DLL *CPLZLibInflate(const void *ptr, size_t nBytes, void *outptr,
                             size_t nOutAvailableBytes, size_t *pnOutBytes);
void CPL_DLL *CPLZLibInflateEx(const void *ptr, size_t nBytes, void *outptr,
                               size_t nOutAvailableBytes,
                               bool bAllowResizeOutptr, size_t *pnOutBytes);

/* -------------------------------------------------------------------- */
/*      XML validation.                                                 */
/* -------------------------------------------------------------------- */
int CPL_DLL CPLValidateXML(const char *pszXMLFilename,
                           const char *pszXSDFilename,
                           CSLConstList papszOptions);

/* -------------------------------------------------------------------- */
/*      Locale handling. Prevents parallel executions of setlocale().   */
/* -------------------------------------------------------------------- */
char *CPLsetlocale(int category, const char *locale);
/*! @cond Doxygen_Suppress */
void CPLCleanupSetlocaleMutex(void);
/*! @endcond */

/*!
    CPLIsPowerOfTwo()
    @param i - tested number
    @return TRUE if i is power of two otherwise return FALSE
*/
int CPL_DLL CPLIsPowerOfTwo(unsigned int i);

/* -------------------------------------------------------------------- */
/*      Terminal related                                                */
/* -------------------------------------------------------------------- */

bool CPL_DLL CPLIsInteractive(FILE *f);

CPL_C_END

/* -------------------------------------------------------------------- */
/*      C++ object for temporarily forcing a LC_NUMERIC locale to "C".  */
/* -------------------------------------------------------------------- */

//! @cond Doxygen_Suppress
#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS)

extern "C++"
{
    class CPL_DLL CPLLocaleC
    {
        CPL_DISALLOW_COPY_ASSIGN(CPLLocaleC)
      public:
        CPLLocaleC();
        ~CPLLocaleC();

      private:
        char *pszOldLocale;
    };

    // Does the same as CPLLocaleC except that, when available, it tries to
    // only affect the current thread. But code that would be dependent of
    // setlocale(LC_NUMERIC, NULL) returning "C", such as current proj.4
    // versions, will not work depending on the actual implementation
    class CPLThreadLocaleCPrivate;

    class CPL_DLL CPLThreadLocaleC
    {
        CPL_DISALLOW_COPY_ASSIGN(CPLThreadLocaleC)

      public:
        CPLThreadLocaleC();
        ~CPLThreadLocaleC();

      private:
        CPLThreadLocaleCPrivate *m_private;
    };
}

#endif /* def __cplusplus */
//! @endcond

/* -------------------------------------------------------------------- */
/*      C++ object for temporarily forcing a config option              */
/* -------------------------------------------------------------------- */

//! @cond Doxygen_Suppress
#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS)

extern "C++"
{
    class CPL_DLL CPLConfigOptionSetter
    {
        CPL_DISALLOW_COPY_ASSIGN(CPLConfigOptionSetter)
      public:
        CPLConfigOptionSetter(const char *pszKey, const char *pszValue,
                              bool bSetOnlyIfUndefined);
        ~CPLConfigOptionSetter();

      private:
        char *m_pszKey;
        char *m_pszOldValue;
        bool m_bRestoreOldValue;
    };
}

#endif /* def __cplusplus */
//! @endcond

#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS)

extern "C++"
{

#ifndef DOXYGEN_SKIP
#include <type_traits>  // for std::is_base_of
#endif

    namespace cpl
    {
    /** Use cpl::down_cast<Derived*>(pointer_to_base) as equivalent of
     * static_cast<Derived*>(pointer_to_base) with safe checking in debug
     * mode.
     *
     * Only works if no virtual inheritance is involved.
     *
     * @param f pointer to a base class
     * @return pointer to a derived class
     */
    template <typename To, typename From> inline To down_cast(From *f)
    {
        static_assert(
            (std::is_base_of<From,
                             typename std::remove_pointer<To>::type>::value),
            "target type not derived from source type");
        CPLAssert(f == nullptr || dynamic_cast<To>(f) != nullptr);
        return static_cast<To>(f);
    }

    /** Computes ceil(a/b) where a and b are integers */
    template <class T, class U> inline T div_round_up(T a, U b)
    {
        return a / b + (((a % b) == 0) ? 0 : 1);
    }

    }  // namespace cpl
}  // extern "C++"

#endif /* def __cplusplus */

#endif /* ndef CPL_CONV_H_INCLUDED */
