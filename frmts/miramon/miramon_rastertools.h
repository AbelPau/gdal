/******************************************************************************
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Public (C callable) interface for the Erdas Imagine reading
 *           code.  This include files, and its implementing code depends
 *           on CPL, but not GDAL.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Intergraph Corporation
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MMRRASTERTOOLS_H_INCLUDED
#define MMRRASTERTOOLS_H_INCLUDED

#include <cstdio>

#include "cpl_conv.h"
#include "cpl_string.h"

CPLString MMRGetFileNameFromRelName(const char *pszRELFile);

#endif /* ndef MMRRASTERTOOLS_H_INCLUDED */
