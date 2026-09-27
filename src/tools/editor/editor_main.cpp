// editor_main.cpp -- editor de CondemEngine (ImGui + Vulkan)
//
// La ventana y los eventos vienen de sys_vid.h (SDL2/SDL3); aquí solo se
// gestiona Vulkan (instancia, dispositivo, swapchain) y la interfaz ImGui.
// Basado en examples/example_sdl3_vulkan de Dear ImGui.

#include "tools/editor/editor.h"

#include "common/common.h"
#include "core/cmd.h"
#include "core/cvar.h"
#include "engine/console.h"
#include "sys/sys.h"
#include "sys/sys_vid.h"

#include "tools/editor/dock.h"
#include "tools/editor/menubar.h"
#include "tools/editor/style.h"
#include "tools/editor/window.h"
#include "tools/editor/wnd_console.h"
#include "tools/editor/wnd_info.h"

#include "imgui.h"
#include "imgui_impl_vulkan.h"

#include <cstring>
#include <vector>

/*
===============================================================================

                            VULKAN

===============================================================================
*/

static VkAllocationCallbacks*   vk_allocator = nullptr;
static VkInstance               vk_instance = VK_NULL_HANDLE;
static VkPhysicalDevice         vk_physical_device = VK_NULL_HANDLE;
static VkDevice                 vk_device = VK_NULL_HANDLE;
static uint32_t                 vk_queue_family = (uint32_t)-1;
static VkQueue                  vk_queue = VK_NULL_HANDLE;
static VkPipelineCache          vk_pipeline_cache = VK_NULL_HANDLE;
static VkPhysicalDeviceProperties vk_device_properties;

static ImGui_ImplVulkanH_Window vk_main_window;
static uint32_t                 vk_min_image_count = 2;
static bool                     vk_swapchain_rebuild = false;

static bool                     editor_quit = false;

static void VK_Check(VkResult err)
{
    if (err == VK_SUCCESS)
        return;
    Con_Printf("[vulkan] Error: VkResult = %d\n", (int)err);
    if (err < 0)
        Sys_Error("Vulkan: VkResult = %d", (int)err);
}

static bool VK_IsExtensionAvailable(const std::vector<VkExtensionProperties>& properties, const char* extension)
{
    for (const VkExtensionProperties& p : properties)
        if (!strcmp(p.extensionName, extension))
            return true;
    return false;
}

#ifndef NDEBUG
static bool VK_IsLayerAvailable(const char* layer)
{
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());

    for (const VkLayerProperties& l : layers)
        if (!strcmp(l.layerName, layer))
            return true;
    return false;
}
#endif

/*
================
VK_Init

Crea la instancia, elige la GPU y crea el dispositivo lógico
================
*/
static void VK_Init(std::vector<const char*> instance_extensions)
{
    VkResult err;

// instancia
    {
        uint32_t count;
        vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> properties(count);
        err = vkEnumerateInstanceExtensionProperties(nullptr, &count, properties.data());
        VK_Check(err);

        if (VK_IsExtensionAvailable(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
            instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

        VkApplicationInfo app_info = {};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = ENGINE_NAME " Editor";
        app_info.pEngineName = ENGINE_NAME;
        app_info.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &app_info;
        create_info.enabledExtensionCount = (uint32_t)instance_extensions.size();
        create_info.ppEnabledExtensionNames = instance_extensions.data();

#ifndef NDEBUG
        // capa de validación del Vulkan SDK, si está instalada
        const char* validation = "VK_LAYER_KHRONOS_validation";
        if (VK_IsLayerAvailable(validation))
        {
            create_info.enabledLayerCount = 1;
            create_info.ppEnabledLayerNames = &validation;
            Con_Printf("Vulkan: capa de validación activada\n");
        }
#endif

        err = vkCreateInstance(&create_info, vk_allocator, &vk_instance);
        VK_Check(err);
    }

// GPU
    vk_physical_device = ImGui_ImplVulkanH_SelectPhysicalDevice(vk_instance);
    if (vk_physical_device == VK_NULL_HANDLE)
        Sys_Error("Vulkan: no se encontró ninguna GPU compatible");
    vkGetPhysicalDeviceProperties(vk_physical_device, &vk_device_properties);

// cola de gráficos
    vk_queue_family = ImGui_ImplVulkanH_SelectQueueFamilyIndex(vk_physical_device);
    if (vk_queue_family == (uint32_t)-1)
        Sys_Error("Vulkan: la GPU no tiene cola de gráficos");

// dispositivo lógico (con una cola)
    {
        std::vector<const char*> device_extensions;
        device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        uint32_t count;
        vkEnumerateDeviceExtensionProperties(vk_physical_device, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> properties(count);
        vkEnumerateDeviceExtensionProperties(vk_physical_device, nullptr, &count, properties.data());
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
        if (VK_IsExtensionAvailable(properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
            device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

        const float queue_priority[] = { 1.0f };
        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = vk_queue_family;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;

        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = 1;
        create_info.pQueueCreateInfos = queue_info;
        create_info.enabledExtensionCount = (uint32_t)device_extensions.size();
        create_info.ppEnabledExtensionNames = device_extensions.data();
        err = vkCreateDevice(vk_physical_device, &create_info, vk_allocator, &vk_device);
        VK_Check(err);
        vkGetDeviceQueue(vk_device, vk_queue_family, 0, &vk_queue);
    }

    Con_Printf("Vulkan: %s (API %u.%u.%u)\n", vk_device_properties.deviceName,
        VK_API_VERSION_MAJOR(vk_device_properties.apiVersion),
        VK_API_VERSION_MINOR(vk_device_properties.apiVersion),
        VK_API_VERSION_PATCH(vk_device_properties.apiVersion));
}

/*
================
VK_SetupWindow

Crea el swapchain y el render pass para la superficie de la ventana
================
*/
static void VK_SetupWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height)
{
    VkBool32 supported;
    vkGetPhysicalDeviceSurfaceSupportKHR(vk_physical_device, vk_queue_family, surface, &supported);
    if (supported != VK_TRUE)
        Sys_Error("Vulkan: la GPU no puede presentar en esta ventana");

    wd->Surface = surface;

    const VkFormat request_formats[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(vk_physical_device, wd->Surface,
        request_formats, (int)IM_ARRAYSIZE(request_formats), VK_COLORSPACE_SRGB_NONLINEAR_KHR);

    const VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };   // vsync
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(vk_physical_device, wd->Surface,
        present_modes, (int)IM_ARRAYSIZE(present_modes));

    ImGui_ImplVulkanH_CreateOrResizeWindow(vk_instance, vk_physical_device, vk_device, wd,
        vk_queue_family, vk_allocator, width, height, vk_min_image_count, 0);
}

/*
================
VK_Shutdown
================
*/
static void VK_Shutdown(void)
{
    ImGui_ImplVulkanH_DestroyWindow(vk_instance, vk_device, &vk_main_window, vk_allocator);
    if (vk_main_window.Surface)
        vkDestroySurfaceKHR(vk_instance, vk_main_window.Surface, vk_allocator);
    vk_main_window.Surface = VK_NULL_HANDLE;

    if (vk_device)
        vkDestroyDevice(vk_device, vk_allocator);
    if (vk_instance)
        vkDestroyInstance(vk_instance, vk_allocator);
    vk_device = VK_NULL_HANDLE;
    vk_instance = VK_NULL_HANDLE;
}

/*
================
VK_FrameRender

Devuelve false si no se pudo adquirir una imagen (swapchain caducado): no
hay nada que presentar
================
*/
static bool VK_FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data)
{
    // image_acquired rota por frame; render_complete va por imagen del
    // swapchain (FrameIndex, se sabe tras adquirirla): la presentación de esa
    // imagen es lo único que lo espera, así que cuando se vuelve a adquirir la
    // misma imagen ya está libre (VUID-vkQueueSubmit-pSignalSemaphores-00067)
    VkSemaphore image_acquired = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;

    VkResult err = vkAcquireNextImageKHR(vk_device, wd->Swapchain, UINT64_MAX, image_acquired, VK_NULL_HANDLE, &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        vk_swapchain_rebuild = true;
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
        return false;
    if (err != VK_SUBOPTIMAL_KHR)
        VK_Check(err);

    VkSemaphore render_complete = wd->FrameSemaphores[wd->FrameIndex].RenderCompleteSemaphore;
    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
    {
        err = vkWaitForFences(vk_device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
        VK_Check(err);
        err = vkResetFences(vk_device, 1, &fd->Fence);
        VK_Check(err);
    }
    {
        err = vkResetCommandPool(vk_device, fd->CommandPool, 0);
        VK_Check(err);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
        VK_Check(err);
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = wd->RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = (uint32_t)wd->Width;
        info.renderArea.extent.height = (uint32_t)wd->Height;
        info.clearValueCount = 1;
        info.pClearValues = &wd->ClearValue;
        vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

    vkCmdEndRenderPass(fd->CommandBuffer);
    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &image_acquired;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &fd->CommandBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &render_complete;

        err = vkEndCommandBuffer(fd->CommandBuffer);
        VK_Check(err);
        err = vkQueueSubmit(vk_queue, 1, &info, fd->Fence);
        VK_Check(err);
    }
    return true;
}

/*
================
VK_FramePresent

Presenta la imagen de VK_FrameRender. También con el swapchain subóptimo
(vk_swapchain_rebuild): la imagen adquirida se tiene que presentar para que
su semáforo quede libre antes de recrearlo
================
*/
static void VK_FramePresent(ImGui_ImplVulkanH_Window* wd)
{
    VkSemaphore render_complete = wd->FrameSemaphores[wd->FrameIndex].RenderCompleteSemaphore;   // ver VK_FrameRender
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete;
    info.swapchainCount = 1;
    info.pSwapchains = &wd->Swapchain;
    info.pImageIndices = &wd->FrameIndex;

    VkResult err = vkQueuePresentKHR(vk_queue, &info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        vk_swapchain_rebuild = true;
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
        return;
    if (err != VK_SUBOPTIMAL_KHR)
        VK_Check(err);

    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
}

/*
===============================================================================

                            INTERFAZ

===============================================================================
*/

/*
================
Editor_Quit_f
================
*/
static void Editor_Quit_f(void)
{
    editor_quit = true;
}

/*
===============================================================================

                            BUCLE PRINCIPAL

===============================================================================
*/

/*
================
Editor_Main
================
*/
int Editor_Main(void)
{
    Cmd_AddCommand("quit", Editor_Quit_f);

// ventana
    if (!VID_CreateVulkanWindow(ENGINE_NAME " Editor", 1280, 720))
        Sys_Error("No se pudo crear la ventana del editor");

// vulkan
    std::vector<const char*> extensions;
    if (!VID_GetVulkanInstanceExtensions(extensions))
        Sys_Error("Vulkan no está disponible en este sistema");
    VK_Init(extensions);

    VkSurfaceKHR surface;
    if (!VID_CreateVulkanSurface(vk_instance, &surface))
        Sys_Error("No se pudo crear la superficie Vulkan");

    int w, h;
    VID_GetDrawableSize(&w, &h);
    VK_SetupWindow(&vk_main_window, surface, w, h);

// imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = "editor.ini";

    // las ventanas flotantes pueden salir de la ventana principal: cada una
    // pasa a ser una ventana del sistema, hija de la del editor (siempre
    // delante de ella, se minimiza con ella) y sin icono en la barra de tareas
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigViewportsNoTaskBarIcon = true;
    io.ConfigViewportsNoDefaultParent = false;

    ed_style.apply(VID_GetDisplayScale());

    VID_ImGui_Init();

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VK_API_VERSION_1_0;
    init_info.Instance = vk_instance;
    init_info.PhysicalDevice = vk_physical_device;
    init_info.Device = vk_device;
    init_info.QueueFamily = vk_queue_family;
    init_info.Queue = vk_queue;
    init_info.PipelineCache = vk_pipeline_cache;
    init_info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE;
    init_info.MinImageCount = vk_min_image_count;
    init_info.ImageCount = vk_main_window.ImageCount;
    init_info.Allocator = vk_allocator;
    init_info.PipelineInfoMain.RenderPass = vk_main_window.RenderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = VK_Check;
    ImGui_ImplVulkan_Init(&init_info);

// ventanas del editor
    editor_window::init();
    editor_dock::init();
    info_window     info(vk_device_properties.deviceName);
    console_window  console;

// acoplamiento por defecto (editor.ini lo sobrescribe con el guardado):
// la consola abajo a todo el ancho, info a la izquierda del hueco central
    editor_dock     main_dock("main");
    main_dock.dock(&console, nullptr, DOCK_BOTTOM, 0.30f);
    main_dock.dock(&info, main_dock.central(), DOCK_LEFT, 0.22f);
    editor_dock::set_active(&main_dock);

    editor_menubar  menubar;

    Con_Printf("Editor iniciado. Escribe \"cmdlist\", \"cvarlist\" o \"windowlist\" en la consola.\n");

// bucle
    while (!editor_quit)
    {
        if (!VID_PumpEvents())
            break;

        Cbuf_Execute();

        // recrear el swapchain si cambió el tamaño de la ventana
        int fb_width, fb_height;
        VID_GetDrawableSize(&fb_width, &fb_height);
        if (fb_width > 0 && fb_height > 0 && (vk_swapchain_rebuild || vk_main_window.Width != fb_width || vk_main_window.Height != fb_height))
        {
            ImGui_ImplVulkan_SetMinImageCount(vk_min_image_count);
            ImGui_ImplVulkanH_CreateOrResizeWindow(vk_instance, vk_physical_device, vk_device, &vk_main_window,
                vk_queue_family, vk_allocator, fb_width, fb_height, vk_min_image_count, 0);
            vk_main_window.FrameIndex = 0;
            vk_main_window.SemaphoreIndex = 0;     // puede haber menos imágenes que antes
            vk_swapchain_rebuild = false;
        }
        if (VID_IsMinimized())
        {
            Sys_Sleep(10);
            continue;
        }

        ImGui_ImplVulkan_NewFrame();
        VID_ImGui_NewFrame();
        ImGui::NewFrame();

        menubar.draw();         // antes: reduce el área de trabajo
        editor_window::draw_all();

        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        bool rendered = false;
        if (!minimized)
        {
            vk_main_window.ClearValue.color.float32[0] = ed_style.background.x;
            vk_main_window.ClearValue.color.float32[1] = ed_style.background.y;
            vk_main_window.ClearValue.color.float32[2] = ed_style.background.z;
            vk_main_window.ClearValue.color.float32[3] = ed_style.background.w;
            rendered = VK_FrameRender(&vk_main_window, draw_data);
        }

        // ventanas del sistema de las ventanas que están fuera de la principal
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        if (rendered)
            VK_FramePresent(&vk_main_window);
    }

// limpieza
    VK_Check(vkDeviceWaitIdle(vk_device));
    ImGui_ImplVulkan_Shutdown();
    VID_ImGui_Shutdown();
    ImGui::DestroyContext();

    VK_Shutdown();
    VID_DestroyWindow();
    return 0;
}
