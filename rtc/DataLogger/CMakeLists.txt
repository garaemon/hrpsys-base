set(comp_sources DataLogger.cpp DataLoggerService_impl.cpp)
set(libs hrpsysBaseStub)
add_library(DataLogger SHARED ${comp_sources})
target_link_libraries(DataLogger ${libs})
set_target_properties(DataLogger PROPERTIES PREFIX "")

add_executable(DataLoggerComp DataLoggerComp.cpp ${comp_sources})
target_link_libraries(DataLoggerComp ${libs})

find_package(PCL)
if (PCL_FOUND AND "${PCL_VERSION_MINOR}" GREATER 6)
  include_directories(${PCL_INCLUDE_DIRS})
  link_directories(${PCL_LIBRARY_DIRS})
  add_executable(PointCloudLogViewer PointCloudLogViewer)
  target_link_libraries(PointCloudLogViewer ${PCL_LIBRARIES})
  set(target DataLogger DataLoggerComp PointCloudLogViewer)
else()
  set(target DataLogger DataLoggerComp)
endif()

install(TARGETS ${target}
  RUNTIME DESTINATION bin CONFIGURATIONS Release Debug
  LIBRARY DESTINATION lib CONFIGURATIONS Release Debug
)

