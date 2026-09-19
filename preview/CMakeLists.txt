set(PREVIEW_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/preview")

add_library(HFWMenuPreview SHARED
    PreviewDll.cpp
    PreviewRenderer.cpp
    PreviewRenderer.h
    PreviewUI.cpp
    PreviewUI.h
)

target_compile_features(HFWMenuPreview PRIVATE cxx_std_23)
target_compile_definitions(HFWMenuPreview PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(HFWMenuPreview PRIVATE imgui::imgui d3d12 dxgi dxguid)
set_target_properties(HFWMenuPreview PROPERTIES
    OUTPUT_NAME "HFWMenuPreview"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${PREVIEW_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${PREVIEW_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${PREVIEW_OUTPUT_DIRECTORY}"
)

add_executable(HFWMenuPreviewHost WIN32 PreviewHost.cpp)
target_compile_features(HFWMenuPreviewHost PRIVATE cxx_std_23)
target_compile_definitions(HFWMenuPreviewHost PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
set_target_properties(HFWMenuPreviewHost PROPERTIES
    OUTPUT_NAME "HFWMenuPreview"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${PREVIEW_OUTPUT_DIRECTORY}"
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(HFWMenuPreview PRIVATE /utf-8 /W4 /permissive- /EHsc)
    target_compile_options(HFWMenuPreviewHost PRIVATE /utf-8 /W4 /permissive- /EHsc)
endif()
