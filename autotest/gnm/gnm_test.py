#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic GNMGdalNetwork class functionality.
# Authors:  Mikhail Gusev (gusevmihs at gmail dot com)
#           Dmitry Baryshnikov, polimax@mail.ru
#
###############################################################################
# Copyright (c) 2014, Mikhail Gusev
# Copyright (c) 2014-2015, NextGIS <info@nextgis.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import shutil

import pytest

from osgeo import gdal, gnm

pytestmark = pytest.mark.require_driver("GNMFile")

###############################################################################
# Create file base network


def test_gnm_filenetwork_create():

    try:
        shutil.rmtree("tmp/test_gnm")
    except OSError:
        pass

    drv = gdal.GetDriverByName("GNMFile")
    with drv.Create(
        "tmp/",
        0,
        0,
        0,
        gdal.GDT_Unknown,
        options=[
            "net_name=test_gnm",
            "net_description=Test file based GNM",
            "net_srs=EPSG:4326",
        ],
    ) as ds:
        # cast to GNM
        dn = gnm.CastToNetwork(ds)
        assert dn is not None
        assert dn.GetVersion() == 100, "GNM: Check GNM version failed"
        assert dn.GetName() == "test_gnm", "GNM: Check GNM name failed"
        assert (
            dn.GetDescription() == "Test file based GNM"
        ), "GNM: Check GNM description failed"


###############################################################################
# Open file base network


def test_gnm_filenetwork_open():

    ds = gdal.OpenEx("tmp/test_gnm")
    # cast to GNM
    dn = gnm.CastToNetwork(ds)
    assert dn is not None
    assert dn.GetVersion() == 100, "GNM: Check GNM version failed"
    assert dn.GetName() == "test_gnm", "GNM: Check GNM name failed"
    assert (
        dn.GetDescription() == "Test file based GNM"
    ), "GNM: Check GNM description failed"

    dn = None


###############################################################################
# Import layers into file base network


def test_gnm_import():

    ds = gdal.OpenEx("tmp/test_gnm")

    # pipes
    dspipes = gdal.OpenEx("data/pipes.shp", gdal.OF_VECTOR)
    lyrpipes = dspipes.GetLayerByIndex(0)
    new_lyr = ds.CopyLayer(lyrpipes, "pipes")
    assert new_lyr is not None, "failed to import pipes"
    dspipes = None
    new_lyr = None

    # wells
    dswells = gdal.OpenEx("data/wells.shp", gdal.OF_VECTOR)
    lyrwells = dswells.GetLayerByIndex(0)
    new_lyr = ds.CopyLayer(lyrwells, "wells")
    assert new_lyr is not None, "failed to import wells"
    dswells = None
    new_lyr = None

    assert ds.GetLayerCount() == 2, "expected 2 layers"

    ds = None


###############################################################################
# autoconnect


def test_gnm_autoconnect():

    ds = gdal.OpenEx("tmp/test_gnm")
    dgn = gnm.CastToGenericNetwork(ds)
    assert dgn is not None, "cast to GNMGenericNetwork failed"

    ret = dgn.ConnectPointsByLines(
        ["pipes", "wells"], 0.000001, 1, 1, gnm.GNM_EDGE_DIR_BOTH
    )
    assert ret == 0, "failed to connect"

    dgn = None


###############################################################################
# Dijkstra shortest path


def test_gnm_graph_dijkstra():

    ds = gdal.OpenEx("tmp/test_gnm")
    dn = gnm.CastToNetwork(ds)
    assert dn is not None, "cast to GNMNetwork failed"

    lyr = dn.GetPath(61, 50, gnm.GATDijkstraShortestPath)
    assert lyr is not None, "failed to get path"

    if lyr.GetFeatureCount() == 0:
        dn.ReleaseResultSet(lyr)
        pytest.fail("failed to get path")

    dn.ReleaseResultSet(lyr)
    dn = None


###############################################################################
# KShortest Paths


def test_gnm_graph_kshortest():

    ds = gdal.OpenEx("tmp/test_gnm")
    dn = gnm.CastToNetwork(ds)
    assert dn is not None, "cast to GNMNetwork failed"

    lyr = dn.GetPath(61, 50, gnm.GATKShortestPath, options=["num_paths=3"])
    assert lyr is not None, "failed to get path"

    if lyr.GetFeatureCount() < 20:
        dn.ReleaseResultSet(lyr)
        pytest.fail("failed to get path")

    dn.ReleaseResultSet(lyr)
    dn = None


###############################################################################
# ConnectedComponents


def test_gnm_graph_connectedcomponents():

    ds = gdal.OpenEx("tmp/test_gnm")
    dn = gnm.CastToNetwork(ds)
    assert dn is not None, "cast to GNMNetwork failed"

    lyr = dn.GetPath(61, 50, gnm.GATConnectedComponents)
    assert lyr is not None, "failed to get path"

    if lyr.GetFeatureCount() == 0:
        dn.ReleaseResultSet(lyr)
        pytest.fail("failed to get path")

    dn.ReleaseResultSet(lyr)
    dn = None


###############################################################################
# Network deleting


def test_gnm_delete():

    gdal.GetDriverByName("GNMFile").Delete("tmp/test_gnm")

    assert not os.path.exists("tmp/test_gnm")
