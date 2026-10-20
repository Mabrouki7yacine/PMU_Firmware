# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/yacine/esp/esp-idf/components/bootloader/subproject"
  "/home/yacine/ComputerEng/projects/PMU-PFE/Firmware/build/bootloader"
  "/home/yacine/ComputerEng/projects/PMU-PFE/Firmware/build/bootloader-prefix"
  "/home/yacine/ComputerEng/projects/PMU-PFE/Firmware/build/bootloader-prefix/tmp"
  "/home/yacine/ComputerEng/projects/PMU-PFE/Firmware/build/bootloader-prefix/src/bootloader-stamp"
  "/home/yacine/ComputerEng/projects/PMU-PFE/Firmware/build/bootloader-prefix/src"
  "/home/yacine/ComputerEng/projects/PMU-PFE/Firmware/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/yacine/ComputerEng/projects/PMU-PFE/Firmware/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/yacine/ComputerEng/projects/PMU-PFE/Firmware/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
