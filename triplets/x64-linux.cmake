set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# 只构建 Release，避免 vcpkg 默认的 debug+release 双份耗时
set(VCPKG_BUILD_TYPE release)
