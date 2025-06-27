# depend from gcore
add_gdal_driver(
  TARGET gdal_MiraMon
  SOURCES miramon.h
          miramon_p.h
          miramonrel.h
          miramondataset.h
          miramon_rastertools.h
          miramonband.cpp
          miramonrel.cpp
          miramondataset.cpp
          miramonopen.cpp
          miramon_rastertools.cpp
  PLUGIN_CAPABLE
  NO_DEPS
)
gdal_standard_includes(gdal_MiraMon)

# Because linking an OBJECT library
#add_executable(mm_gdal_functions EXCLUDE_FROM_ALL mm_gdal_functions.cpp)
#target_include_directories(mm_gdal_functions PRIVATE $<TARGET_PROPERTY:miramon_common,SOURCE_DIR>)
#target_link_libraries(mm_gdal_functions PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> miramon_common)

gdal_target_link_libraries(gdal_MiraMon PRIVATE PROJ::proj)
