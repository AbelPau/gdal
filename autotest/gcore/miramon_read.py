#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for all datatypes from a MiraMon file.
# Author:   Abel Pau <a.pau@creaf.cat>
#
###############################################################################
# Copyright (c) 2025, Abel Pau <a.pau@creaf.cat>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

# from osgeo import gdal

init_list = [
    # ("byte_2x3_6_categs_RLE.img", 15),
    ("byte_2x3_6_categs.img", 15),
]


@pytest.mark.parametrize(
    "filename,checksum",
    init_list,
    ids=[tup[0].split(".")[0] for tup in init_list],
)
@pytest.mark.require_driver("MiraMonRaster")
def test_hfa_open(filename, checksum):
    ut = gdaltest.GDALTest("MiraMonRaster", filename, 1, checksum)
    ut.testOpen()
