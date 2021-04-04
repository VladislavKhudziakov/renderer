#pragma once

#include <vulkan/vulkan.h>

#if defined(__linux__)
    #define VK_USE_PLATFORM_XLIB_KHR 1
    #define VK_UTILS_SURFACE_CREATE_INFO_TYPE VkXlibSurfaceCreateInfoKHR
    #define VK_UTILS_SURFACE_CREATE_FUNCTION vkCreateXlibSurfaceKHR
    #include <X11/Xlib.h>
    #include <vulkan/vulkan_xlib.h>
#elif defined(_WIN32)
    #define  VK_USE_PLATFORM_WIN32_KHR 1
    #define VK_UTILS_SURFACE_CREATE_INFO_TYPE VkWin32SurfaceCreateInfoKHR
    #define VK_UTILS_SURFACE_CREATE_FUNCTION vkCreateWin32SurfaceKHR
    #include <windows.h>
    #include <vulkan/vulkan_win32.h>
#elif defined(__APPLE__)
    #define VK_USE_PLATFORM_METAL_EXT 1
    #define VK_UTILS_SURFACE_CREATE_INFO_TYPE VkMetalSurfaceCreateInfoEXT
    #define VK_UTILS_SURFACE_CREATE_FUNCTION vkCreateMetalSurfaceEXT
    #include <vulkan/vulkan_metal.h>
#endif
