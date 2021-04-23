

#include "context.hpp"

#include <logger/log.hpp>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace
{
    template <typename... LogArgs>
    void log(const char* fmt, LogArgs&& ...args)
    {
        constexpr const char* ignore[] {
            "vmaFlushAllocation"
        };

        for (const auto* ignored : ignore) {
            if (strstr(fmt, ignored) != nullptr) {
                return;
            }
        }

        printf(fmt, std::forward<LogArgs>(args)...);
    }
}
#ifndef VMA_IMPLEMENTATION
    #define VMA_IMPLEMENTATION
    #ifndef NDEBUG
//            #define VMA_DEBUG_LOG(format, ...) log(LOGGER_COLOR_MODIFIER_FG_BLUE "[DEBUG] [VMA] " format LOGGER_COLOR_MODIFIER_FG_DEFAULT "\n" __VA_OPT__(, ) __VA_ARGS__);
//            printf(LOGGER_COLOR_MODIFIER_FG_BLUE "[DEBUG] [VMA] " format LOGGER_COLOR_MODIFIER_FG_DEFAULT "\n" __VA_OPT__(, ) __VA_ARGS__);
    #endif
    #include <VulkanMemoryAllocator/src/vk_mem_alloc.h>
#endif

#include <vector>
#include <cstring>
#include <optional>

namespace
{
    std::optional<vk_utils::context> ctx;

    void merge_extensions_list(const std::vector<VkExtensionProperties>& props, const char** ext_list, uint32_t ext_count, std::vector<const char*>& out_extensions)
    {
        for (uint32_t i = 0; i < ext_count; ++i) {
            const auto required_ext_found = std::find_if(props.begin(), props.end(), [i, ext_list](const VkExtensionProperties& props) {
                return strcmp(props.extensionName, ext_list[i]) == 0;
            });

            if (required_ext_found == props.end()) {
                continue;
            }

            const auto extension_was_added = std::find_if(
                out_extensions.begin(),
                out_extensions.end(),
                [i, &ext_list](const char* e) {
                    return strcmp(e, ext_list[i]) == 0;
                });

            if (extension_was_added == out_extensions.end()) {
                out_extensions.emplace_back(ext_list[i]);
            }
        }
    }


    void merge_layers_list(const std::vector<VkLayerProperties>& props, const char** l_list, uint32_t l_count, std::vector<const char*>& out_layers)
    {
        for (uint32_t i = 0; i < l_count; ++i) {
            const auto required_ext_found = std::find_if(props.begin(), props.end(), [i, l_list](const VkLayerProperties& props) {
                return strcmp(props.layerName, l_list[i]) == 0;
            });

            if (required_ext_found == props.end()) {
                continue;
            }

            const auto layer_was_added = std::find_if(
                out_layers.begin(),
                out_layers.end(),
                [i, &l_list](const char* e) {
                    return strcmp(e, l_list[i]) == 0;
                });

            if (layer_was_added == out_layers.end()) {
                out_layers.emplace_back(l_list[i]);
            }
        }
    }

    bool check_device_extensions(VkPhysicalDevice device, const char** ext_names, uint32_t ext_count)
    {
        uint32_t curr_ext_count;

        vkEnumerateDeviceExtensionProperties(device, nullptr, &curr_ext_count, nullptr);
        std::vector<VkExtensionProperties> props{curr_ext_count};
        vkEnumerateDeviceExtensionProperties(device, nullptr, &curr_ext_count, props.data());

        for (int i = 0; i < ext_count; ++i) {
            const auto required_ext_found = std::find_if(props.begin(), props.end(), [i, ext_names](const VkExtensionProperties& props) {
                return strcmp(props.extensionName, ext_names[i]) == 0;
            });
            if (required_ext_found == props.end()) {
                return false;
            }
        }

        return true;
    };

    bool check_device_layers(VkPhysicalDevice device, const char** layers_names, uint32_t curr_layers_count)
    {
        uint32_t layers_count;

        vkEnumerateDeviceLayerProperties(device, &layers_count, nullptr);
        std::vector<VkLayerProperties> props{layers_count};
        vkEnumerateDeviceLayerProperties(device, &layers_count, props.data());

        for (int i = 0; i < curr_layers_count; ++i) {
            const auto required_ext_found = std::find_if(props.begin(), props.end(), [i, layers_names](const VkLayerProperties& props) {
                return strcmp(props.layerName, layers_names[i]) == 0;
            });
            if (required_ext_found == props.end()) {
                return false;
            }
        }

        return true;
    };


    bool check_device_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface, vk_utils::context::queue_family_data* queue_families_indices)
    {
        uint32_t queue_families_count;

        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_families_count, nullptr);
        std::vector<VkQueueFamilyProperties> props{queue_families_count};
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_families_count, props.data());

        for (int i = 0; i < queue_families_count; ++i) {
            auto& p = props[i];

            VkBool32 surface_supported{};

            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &surface_supported);

            if (surface_supported && queue_families_indices[vk_utils::context::QUEUE_TYPE_PRESENT].index < 0) {
                queue_families_indices[vk_utils::context::QUEUE_TYPE_PRESENT].index = i;
                queue_families_indices[vk_utils::context::QUEUE_TYPE_PRESENT].max_queue_count = 1;
            }

            if (p.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                if (queue_families_indices[vk_utils::context::QUEUE_TYPE_GRAPHICS].index >= 0) {
                    if (props[i].queueCount > props[queue_families_indices[vk_utils::context::QUEUE_TYPE_GRAPHICS].index].queueCount) {
                        queue_families_indices[vk_utils::context::QUEUE_TYPE_GRAPHICS].index = i;
                        queue_families_indices[vk_utils::context::QUEUE_TYPE_GRAPHICS].max_queue_count = props[i].queueCount;
                    }
                } else {
                    queue_families_indices[vk_utils::context::QUEUE_TYPE_GRAPHICS].index = i;
                    queue_families_indices[vk_utils::context::QUEUE_TYPE_GRAPHICS].max_queue_count = props[i].queueCount;
                }
            }

            if (p.queueFlags & VK_QUEUE_COMPUTE_BIT) {
                if (queue_families_indices[vk_utils::context::QUEUE_TYPE_COMPUTE].index >= 0) {
                    if (props[i].queueCount > props[queue_families_indices[vk_utils::context::QUEUE_TYPE_COMPUTE].index].queueCount) {
                        queue_families_indices[vk_utils::context::QUEUE_TYPE_COMPUTE].index = i;
                        queue_families_indices[vk_utils::context::QUEUE_TYPE_COMPUTE].max_queue_count = props[i].queueCount;
                    }
                } else {
                    queue_families_indices[vk_utils::context::QUEUE_TYPE_COMPUTE].index = i;
                    queue_families_indices[vk_utils::context::QUEUE_TYPE_COMPUTE].max_queue_count = props[i].queueCount;
                }
            }

            if (p.queueFlags & VK_QUEUE_TRANSFER_BIT) {
                if (queue_families_indices[vk_utils::context::QUEUE_TYPE_TRANSFER].index >= 0) {
                    if (props[i].queueCount > props[queue_families_indices[vk_utils::context::QUEUE_TYPE_TRANSFER].index].queueCount) {
                        queue_families_indices[vk_utils::context::QUEUE_TYPE_TRANSFER].index = i;
                        queue_families_indices[vk_utils::context::QUEUE_TYPE_TRANSFER].max_queue_count = props[i].queueCount;
                    }
                } else {
                    queue_families_indices[vk_utils::context::QUEUE_TYPE_TRANSFER].index = i;
                    queue_families_indices[vk_utils::context::QUEUE_TYPE_TRANSFER].max_queue_count = props[i].queueCount;
                }
            }
        }

        for (int i = 0; i < vk_utils::context::QUEUE_TYPE_SIZE; ++i) {
            if (queue_families_indices[i].index < 0 && queue_families_indices[i].max_queue_count > 0) {
                return false;
            }
        }

        return true;
    };

    const char* implicit_required_instance_layers[] = {
        "VK_LAYER_KHRONOS_validation"};

    const char* implicit_required_instance_extensions[] = {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

    const char* implicit_required_device_extensions[]{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    const char* implicit_required_device_layers[] = {
        "VK_LAYER_KHRONOS_validation"};

} // namespace


VkDevice vk_utils::context::device() const
{
    return m_device;
}


const char* vk_utils::context::app_name() const
{
    return m_app_name;
}


const vk_utils::context& vk_utils::context::get()
{
    return *ctx;
}


VkInstance vk_utils::context::instance() const
{
    return m_instance;
}


ERROR_TYPE vk_utils::context::init(
    const char* app_name,
    const context_init_info& context_init_info)
{
    ctx.emplace();
    ctx->m_app_name = app_name;

    PASS_ERROR(init_instance(app_name, context_init_info));
    PASS_ERROR(init_debug_messenger(context_init_info));
    VkSurfaceKHR sf{nullptr};
    if (const auto err_code = context_init_info.surface_create_callback(ctx->m_instance, &sf); err_code != VK_SUCCESS) {
        RAISE_ERROR_FATAL(err_code, "Cannot init surface.");
    }
    ctx->m_surface.reset(ctx->m_instance, sf);
    PASS_ERROR(select_physical_device(context_init_info));
    PASS_ERROR(init_device(context_init_info));
    PASS_ERROR(init_memory_allocator());
    PASS_ERROR(request_queues());

    RAISE_ERROR_OK();
}


ERROR_TYPE vk_utils::context::init_instance(const char* app_name, const vk_utils::context::context_init_info& context_init_info)
{
    uint32_t api_version;
    vkEnumerateInstanceVersion(&api_version);

    auto messenger_create_info = get_debug_messenger_create_info();

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = nullptr;
    app_info.pApplicationName = app_name;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "no Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instance_info{};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pNext = nullptr;
#ifndef NDEBUG
    instance_info.pNext = &messenger_create_info;
#else
    instance_info.pNext = nullptr;
#endif
    uint32_t i_ext_props_size{0};
    vkEnumerateInstanceExtensionProperties(nullptr, &i_ext_props_size, nullptr);
    std::vector<VkExtensionProperties> instance_extensions_props{i_ext_props_size};
    vkEnumerateInstanceExtensionProperties(nullptr, &i_ext_props_size, instance_extensions_props.data());

    std::vector<const char*> instance_extensions_list{};

    for (int i = 0; i < context_init_info.required_instance_extensions.count; ++i) {
        const auto required_ext_found = std::find_if(instance_extensions_props.begin(), instance_extensions_props.end(), [i, &context_init_info](const VkExtensionProperties& props) {
            return strcmp(props.extensionName, context_init_info.required_instance_extensions.names[i]) == 0;
        });

        if (required_ext_found == instance_extensions_props.end()) {
            RAISE_ERROR_FATAL(-1, std::string("cannot find required instance extension ") + context_init_info.required_instance_extensions.names[i]);
        }
    }

    merge_extensions_list(
        instance_extensions_props,
        context_init_info.required_instance_extensions.names,
        context_init_info.required_instance_extensions.count,
        instance_extensions_list);

    merge_extensions_list(
        instance_extensions_props,
        context_init_info.additional_instance_extensions.names,
        context_init_info.additional_instance_extensions.count,
        instance_extensions_list);

    merge_extensions_list(
        instance_extensions_props,
        implicit_required_instance_extensions,
#ifndef NDEBUG
        std::size(implicit_required_instance_extensions),
#else
        0,
#endif
        instance_extensions_list);

    uint32_t i_layers_props_size{0};
    vkEnumerateInstanceLayerProperties(&i_layers_props_size, nullptr);
    std::vector<VkLayerProperties> instance_layer_props{i_layers_props_size};
    vkEnumerateInstanceLayerProperties(&i_ext_props_size, instance_layer_props.data());

    std::vector<const char*> instance_layers_list{};

    for (int i = 0; i < context_init_info.required_instance_layers.count; ++i) {
        const auto required_layer_found = std::find_if(instance_layer_props.begin(), instance_layer_props.end(), [i, &context_init_info](const VkLayerProperties& props) {
            return strcmp(props.layerName, context_init_info.required_instance_layers.names[i]) == 0;
        });

        if (required_layer_found == instance_layer_props.end()) {
            RAISE_ERROR_FATAL(-1, std::string("cannot find required instance layer ") + context_init_info.required_instance_layers.names[i]);
        }
    }

    merge_layers_list(
        instance_layer_props,
        context_init_info.required_instance_layers.names,
        context_init_info.required_instance_layers.count,
        instance_layers_list);

    merge_layers_list(
        instance_layer_props,
        context_init_info.additional_instance_layers.names,
        context_init_info.additional_instance_layers.count,
        instance_layers_list);

    merge_layers_list(
        instance_layer_props,
        implicit_required_instance_layers,
#ifndef NDEBUG
        std::size(implicit_required_instance_layers),
#else
        0,
#endif
        instance_layers_list);

    instance_info.ppEnabledLayerNames = instance_layers_list.data();
    instance_info.enabledLayerCount = instance_layers_list.size();

    instance_info.ppEnabledExtensionNames = instance_extensions_list.data();
    instance_info.enabledExtensionCount = instance_extensions_list.size();

    if (auto err_code = ctx->m_instance.init(&instance_info); err_code != VK_SUCCESS) {
        RAISE_ERROR_FATAL(err_code, "failed to initialize VK instance.");
    }
    RAISE_ERROR_OK();
}


ERROR_TYPE vk_utils::context::init_debug_messenger(const vk_utils::context::context_init_info& info)
{
#ifdef NDEBUG
    return errors::OK;
#else
    auto messenger_create_info = get_debug_messenger_create_info();

    if (auto err_code = ctx->m_debug_messenger.init(context::get().instance(), &messenger_create_info); err_code != VK_SUCCESS) {
        RAISE_ERROR_FATAL(err_code, "cannot init debug messenger.");
    }
    RAISE_ERROR_OK();
#endif
}


ERROR_TYPE vk_utils::context::select_physical_device(const vk_utils::context::context_init_info& info)
{
    uint32_t phys_devices_count;
    vkEnumeratePhysicalDevices(ctx->m_instance, &phys_devices_count, nullptr);
    std::vector<VkPhysicalDevice> physical_devices{phys_devices_count};
    vkEnumeratePhysicalDevices(ctx->m_instance, &phys_devices_count, physical_devices.data());

    auto try_select_device = [&physical_devices, &info](std::function<bool(const VkPhysicalDeviceProperties&)> cond) {
        for (const auto device : physical_devices) {
            VkPhysicalDeviceFeatures features{};
            VkPhysicalDeviceProperties props{};

            vkGetPhysicalDeviceFeatures(device, &features);
            vkGetPhysicalDeviceProperties(device, &props);

            if (cond(props)) {
                if (check_device_extensions(device, info.required_device_extensions.names, info.required_device_extensions.count)
                    && check_device_layers(device, info.required_device_layers.names, info.required_device_layers.count)
                    && check_device_queue_families(device, ctx->m_surface, ctx->m_queue_families_indices)) {
                    ctx->m_physical_device = device;
                    LOG_INFO(std::string(props.deviceName) + " device selected.");
                    return true;
                }
            }
        }

        return false;
    };

    if (info.device_name != nullptr) {
        memset(ctx->m_queue_families_indices, char(-1), sizeof(m_queue_families_indices));
        if (try_select_device([](const VkPhysicalDeviceProperties&) { return true; })) {
            RAISE_ERROR_OK();
        }
    }

    memset(ctx->m_queue_families_indices, char(-1), sizeof(m_queue_families_indices));

    if (try_select_device([](const VkPhysicalDeviceProperties& props) { return props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU; })) {
        RAISE_ERROR_OK();
    }

    memset(ctx->m_queue_families_indices, char(-1), sizeof(m_queue_families_indices));

    if (try_select_device([](const VkPhysicalDeviceProperties& props) { return true; })) {
        RAISE_ERROR_OK();
    }

    RAISE_ERROR_FATAL(-1, "Cannot find suitable device.");
}


ERROR_TYPE vk_utils::context::init_device(const context_init_info& context_init_info)
{
    uint32_t ext_props_size{0};
    vkEnumerateDeviceExtensionProperties(context::get().gpu(), nullptr, &ext_props_size, nullptr);
    std::vector<VkExtensionProperties> device_extensions_props{ext_props_size};
    vkEnumerateDeviceExtensionProperties(context::get().gpu(), nullptr, &ext_props_size, device_extensions_props.data());

    uint32_t layers_props_size{0};
    vkEnumerateDeviceLayerProperties(context::get().gpu(), &layers_props_size, nullptr);
    std::vector<VkLayerProperties> device_layers_props{layers_props_size};
    vkEnumerateDeviceLayerProperties(context::get().gpu(), &layers_props_size, device_layers_props.data());

    std::vector<const char*> device_extensions_list;
    std::vector<const char*> device_layers_list;

    merge_extensions_list(
        device_extensions_props,
        context_init_info.required_instance_extensions.names,
        context_init_info.required_instance_extensions.count,
        device_extensions_list);

    merge_extensions_list(
        device_extensions_props,
        context_init_info.additional_device_extensions.names,
        context_init_info.additional_device_extensions.count,
        device_extensions_list);

    merge_extensions_list(
        device_extensions_props,
        implicit_required_device_extensions,
        std::size(implicit_required_device_extensions),
        device_extensions_list);

    merge_layers_list(
        device_layers_props,
        context_init_info.required_device_layers.names,
        context_init_info.required_device_layers.count,
        device_layers_list);

    merge_layers_list(
        device_layers_props,
        context_init_info.additional_device_layers.names,
        context_init_info.additional_device_layers.count,
        device_layers_list);

    merge_layers_list(
        device_layers_props,
        implicit_required_device_layers,
#ifndef NDEBUG
        std::size(implicit_required_device_layers),
#else
        0,
#endif
        device_layers_list);

    std::vector<VkDeviceQueueCreateInfo> out_infos{};
    out_infos.reserve(QUEUE_TYPE_SIZE);
    std::vector<float> priorities(QUEUE_TYPE_SIZE, 1.0f);

    for (int i = 0; i < QUEUE_TYPE_SIZE; ++i) {
        auto it = std::find_if(out_infos.begin(), out_infos.end(), [i](const VkDeviceQueueCreateInfo& info) {
            return info.queueFamilyIndex == ctx->m_queue_families_indices[i].index;
        });
        if (it != out_infos.end()) {
            it->queueCount = std::min(it->queueCount +1, ctx->m_queue_families_indices[i].max_queue_count);
            ctx->m_queue_families_indices[i].queue_index = it->queueCount - 1;
        } else {
            out_infos.push_back(VkDeviceQueueCreateInfo {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .queueFamilyIndex = static_cast<uint32_t>(ctx->m_queue_families_indices[i].index),
                .pQueuePriorities = priorities.data(),
            });
            ctx->m_queue_families_indices[i].queue_index = 0;
        }
    }

    VkDeviceCreateInfo device_info{};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext = nullptr;
    device_info.ppEnabledExtensionNames = device_extensions_list.data();
    device_info.enabledExtensionCount = device_extensions_list.size();
    device_info.ppEnabledLayerNames = device_layers_list.data();
    device_info.enabledLayerCount = device_layers_list.size();
    device_info.pQueueCreateInfos = out_infos.data();
    device_info.queueCreateInfoCount = out_infos.size();

    if (auto err = ctx->m_device.init(context::get().gpu(), &device_info); err != VK_SUCCESS) {
        RAISE_ERROR_FATAL(err, "cannot initialize logical device");
    }
    RAISE_ERROR_OK();
}


ERROR_TYPE vk_utils::context::init_memory_allocator()
{
    VmaAllocatorCreateInfo allocator_create_info{};
    allocator_create_info.device = ctx->device();
    allocator_create_info.instance = ctx->instance();
    allocator_create_info.physicalDevice = ctx->gpu();
    allocator_create_info.vulkanApiVersion = VK_API_VERSION_1_0;

    if (auto err = ctx->m_allocator.init(&allocator_create_info); err != VK_SUCCESS) {
        RAISE_ERROR_FATAL(err, "cannot initialize allocator.");
    }
    RAISE_ERROR_OK();
}


ERROR_TYPE vk_utils::context::request_queues()
{
    uint32_t graphics_queue_index{0};

    for (int i = 0; i < std::size(ctx->m_queue_families_indices); ++i) {
        if (ctx->m_queue_families_indices[i].index >= 0) {
            vkGetDeviceQueue(ctx->m_device, ctx->m_queue_families_indices[i].index, ctx->m_queue_families_indices[i].queue_index, &ctx->m_queues[i]);
        }
    }

    RAISE_ERROR_OK();
}


vk_utils::context::~context()
{
    m_allocator.destroy();
    m_device.destroy();
    m_surface.destroy();
    m_debug_messenger.destroy();
    m_instance.destroy();
}


int32_t vk_utils::context::queue_family_index(vk_utils::context::queue_type t) const
{
    return m_queue_families_indices[t].index;
}


VkPhysicalDevice vk_utils::context::gpu() const
{
    return m_physical_device;
}


VkQueue vk_utils::context::queue(vk_utils::context::queue_type type) const
{
    return m_queues[type];
}


VkSurfaceKHR vk_utils::context::surface() const
{
    return m_surface;
}


vk_utils::context::memory_alloc_info vk_utils::context::get_memory_alloc_info(VkBuffer buffer, VkMemoryPropertyFlags props_flags) const
{
    VkPhysicalDeviceMemoryProperties properties;
    vkGetPhysicalDeviceMemoryProperties(m_physical_device, &properties);

    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(m_device, buffer, &requirements);

    memory_alloc_info res{};
    res.allocation_size = requirements.size;
    res.allocation_alignment = requirements.alignment;


    for (int i = 0; i < properties.memoryTypeCount; ++i) {
        if (requirements.memoryTypeBits & (1 << i) && (props_flags & properties.memoryTypes[i].propertyFlags)) {
            res.memory_type_index = i;
        }
    }

    return res;
}
VkDebugUtilsMessengerCreateInfoEXT vk_utils::context::get_debug_messenger_create_info()
{
    static auto cb =
        [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
           VkDebugUtilsMessageTypeFlagsEXT messageTypes,
           const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
           void* pUserData) -> uint32_t {
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            LOG_ERROR(pCallbackData->pMessage);
        } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            LOG_WARN(pCallbackData->pMessage);
        } else {
            LOG_INFO(pCallbackData->pMessage);
        }

        return 0;
    };

    VkDebugUtilsMessengerCreateInfoEXT messenger_create_info{};
    messenger_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    messenger_create_info.pNext = nullptr;
    messenger_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    messenger_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    messenger_create_info.pfnUserCallback = cb;

    return messenger_create_info;
}


VmaAllocator vk_utils::context::allocator() const
{
    return m_allocator;
}
