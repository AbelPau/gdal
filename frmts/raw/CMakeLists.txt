add_gdal_driver(
  TARGET gdal_raw
  SOURCES atlsci_spheroid.h
          ace2dataset.cpp
          atlsci_spheroid.cpp
          cpgdataset.cpp
          doq1dataset.cpp
          doq2dataset.cpp
          ehdrdataset.cpp
          eirdataset.cpp
          envidataset.cpp
          fastdataset.cpp
          genbindataset.cpp
          gscdataset.cpp
          gtxdataset.cpp
          hkvdataset.cpp
          krodataset.cpp
          iscedataset.cpp
          landataset.cpp
          lcpdataset.cpp
          loslasdataset.cpp
          mffdataset.cpp
          ndfdataset.cpp
          noaabdataset.cpp
          ntv2dataset.cpp
          pauxdataset.cpp
          pnmdataset.cpp
          roipacdataset.cpp
          rrasterdataset.cpp
          snodasdataset.cpp
          byndataset.cpp
          envidataset.cpp
          nsidcbindataset.cpp
          rawdrivers.cpp
  PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(gdal_raw)
target_include_directories(gdal_raw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
