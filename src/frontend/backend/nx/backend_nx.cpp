// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "ftpd_imgui/imgui_deko3d.h"
#include "ftpd_imgui/imgui_nx.h"
#include "../backend.hpp"
#include "../../system.hpp"
#include "audio/audio.hpp"
#include "fs.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <imgui.h>
#include <switch.h>
#include <cassert>

extern "C" {

#ifndef NDEBUG
    #include <unistd.h> // for close(socket);
    static int nxlink_socket = 0;
#endif

void userAppInit()
{
	appletLockExit(); // block exit until cleanup

	romfsInit(); // assets
	plInitialize(PlServiceType_User); // fonts

#ifndef NDEBUG
    socketInitializeDefault();
    nxlink_socket = nxlinkStdio();
#endif // NDEBUG
}

void userAppExit()
{
#ifndef NDEBUG
    close(nxlink_socket);
    socketExit();
#endif // NDEBUG

	plExit();
	romfsExit();
	appletUnlockExit();
}

} // extern "C"

namespace sys::backend {
namespace {

struct Texture
{
    auto init(int w, int h, int bpp, DkImageFormat f, int id, void* data = nullptr) -> void;
    auto init(const char* file, int id) -> void;

    auto quit() -> void;
    auto update(void* data) -> void;

    [[nodiscard]] auto get_image_id() { return image_id; }
    [[nodiscard]] auto get_sampler_id() { return sampler_id; }

private:
    dk::Image image;
    dk::UniqueMemBlock memBlock;

    // todo: make texture creation like a factory which returns an
    // id (index into texture array)
    int image_id;
    int sampler_id;

    // thse should be set in init()
    DkImageFormat format;
    int width;
    int height;
    int size;
};

constexpr auto MAX_SAMPLERS = 2;
constexpr auto MAX_IMAGES = std::to_underlying(TextureID::max) + 2;
constexpr auto FB_NUM = 2u;
constexpr auto CMDBUF_SIZE = 1024 * 1024;

unsigned s_width = 1920;
unsigned s_height = 1080;

dk::UniqueDevice s_device;
dk::UniqueMemBlock s_depthMemBlock;
dk::Image s_depthBuffer;
dk::UniqueMemBlock s_fbMemBlock;
dk::Image s_frameBuffers[FB_NUM];
dk::UniqueMemBlock s_cmdMemBlock[FB_NUM];
dk::UniqueCmdBuf s_cmdBuf[FB_NUM];
dk::UniqueMemBlock s_imageMemBlock;
dk::UniqueMemBlock s_descriptorMemBlock;
dk::SamplerDescriptor *s_samplerDescriptors = nullptr;
dk::ImageDescriptor *s_imageDescriptors   = nullptr;

dk::UniqueQueue s_queue;
dk::UniqueSwapchain s_swapchain;

Texture textures[std::to_underlying(TextureID::max)];
PadState pad;

AppletHookCookie appletHookCookie;

bool show_fs_browser{false};

auto applet_show_error_message(const char* message, const char* long_message)
{
    ErrorApplicationConfig cfg;
    errorApplicationCreate(&cfg, message, long_message);
    errorApplicationShow(&cfg);
}

auto on_applet_focus_state()
{
    switch(appletGetFocusState())
    {
        case AppletFocusState_InFocus:
            std::printf("[APPLET] AppletFocusState_InFocus\n");
            break;

        case AppletFocusState_OutOfFocus:
            std::printf("[APPLET] AppletFocusState_OutOfFocus\n");
            break;

        case AppletFocusState_Background:
            std::printf("[APPLET] AppletFocusState_Background\n");
            break;
    }
}

auto on_applet_operation_mode()
{
    switch (appletGetOperationMode())
    {
        case AppletOperationMode_Handheld:
            std::printf("[APPLET] AppletOperationMode_Handheld\n");
            break;

        case AppletOperationMode_Console:
            std::printf("[APPLET] AppletOperationMode_Console\n");
            break;
    }
}

auto applet_on_performance_mode()
{
    switch (appletGetPerformanceMode())
    {
        case ApmPerformanceMode_Invalid:
            std::printf("[APPLET] ApmPerformanceMode_Invalid\n");
            break;

        case ApmPerformanceMode_Normal:
            std::printf("[APPLET] ApmPerformanceMode_Normal\n");
            break;

        case ApmPerformanceMode_Boost:
            std::printf("[APPLET] ApmPerformanceMode_Boost\n");
            break;
    }
}

void appplet_hook_calback(AppletHookType type, void *param)
{
    switch (type)
    {
        case AppletHookType_OnFocusState:
            on_applet_focus_state();
            break;

        case AppletHookType_OnOperationMode:
            on_applet_operation_mode();
            break;

        case AppletHookType_OnPerformanceMode:
            applet_on_performance_mode();
            break;

        case AppletHookType_OnExitRequest:
            break;

        case AppletHookType_OnResume:
            break;

        case AppletHookType_OnCaptureButtonShortPressed:
            break;

        case AppletHookType_OnAlbumScreenShotTaken:
            break;

        case AppletHookType_RequestToDisplay:
            break;

        case AppletHookType_Max:
            assert(!"AppletHookType_Max hit");
            break;
    }
}

void RebuildSwapchain(unsigned const width_, unsigned const height_) {
    // destroy old swapchain
    s_swapchain = nullptr;

    // create new depth buffer image layout
    dk::ImageLayout depthLayout;
    dk::ImageLayoutMaker{s_device}
        .setFlags(DkImageFlags_UsageRender | DkImageFlags_HwCompression)
        .setFormat(DkImageFormat_Z24S8)
        .setDimensions(width_, height_)
        .initialize(depthLayout);

    auto const depthAlign = depthLayout.getAlignment();
    auto const depthSize  = depthLayout.getSize();

    // create depth buffer memblock
    if (!s_depthMemBlock) {
        s_depthMemBlock = dk::MemBlockMaker{s_device,
        imgui::deko3d::align(depthSize, std::max<unsigned> (depthAlign, DK_MEMBLOCK_ALIGNMENT))}
            .setFlags(DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image)
            .create();
    }

    s_depthBuffer.initialize(depthLayout, s_depthMemBlock, 0);

    // create framebuffer image layout
    dk::ImageLayout fbLayout;
    dk::ImageLayoutMaker{s_device}
        .setFlags(DkImageFlags_UsageRender | DkImageFlags_UsagePresent | DkImageFlags_HwCompression)
        .setFormat(DkImageFormat_RGBA8_Unorm)
        .setDimensions(width_, height_)
        .initialize(fbLayout);

    auto const fbAlign = fbLayout.getAlignment();
    auto const fbSize  = fbLayout.getSize();

    // create framebuffer memblock
    if (!s_fbMemBlock) {
        s_fbMemBlock = dk::MemBlockMaker{s_device, imgui::deko3d::align(FB_NUM * fbSize, std::max<unsigned> (fbAlign, DK_MEMBLOCK_ALIGNMENT))}
            .setFlags(DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image)
            .create();
    }

    // initialize swapchain images
    std::array<DkImage const *, FB_NUM> swapchainImages;
    for (unsigned i = 0; i < FB_NUM; ++i) {
        swapchainImages[i] = &s_frameBuffers[i];
        s_frameBuffers[i].initialize(fbLayout, s_fbMemBlock, i * fbSize);
    }

    // create swapchain
    s_swapchain = dk::SwapchainMaker{s_device, nwindowGetDefault(), swapchainImages}.create();
}

// void* userData, const char* context, DkResult result, const char* message
auto deko3d_error_cb(void* userData, const char* context, DkResult result, const char* message)
{
    switch (result)
    {
        case DkResult_Success:
            return;

        case DkResult_Fail:
            std::printf("[DkResult_Fail] %s\n", message);
            return;

        case DkResult_Timeout:
            std::printf("[DkResult_Timeout] %s\n", message);
            return;

        case DkResult_OutOfMemory:
            std::printf("[DkResult_OutOfMemory] %s\n", message);
            return;

        case DkResult_NotImplemented:
            std::printf("[DkResult_NotImplemented] %s\n", message);
            return;

        case DkResult_MisalignedSize:
            std::printf("[DkResult_MisalignedSize] %s\n", message);
            return;

        case DkResult_MisalignedData:
            std::printf("[DkResult_MisalignedData] %s\n", message);
            return;

        case DkResult_BadInput:
            std::printf("[DkResult_BadInput] %s\n", message);
            return;

        case DkResult_BadFlags:
            std::printf("[DkResult_BadFlags] %s\n", message);
            return;

        case DkResult_BadState:
            std::printf("[DkResult_BadState] %s\n", message);
            return;
    }
}

void deko3d_init(void) {
    // create deko3d device
    s_device = dk::DeviceMaker{}
        .setCbDebug(deko3d_error_cb)
        .create();

    // initialize swapchain with maximum resolution
    RebuildSwapchain(s_width, s_height);

    // create memblocks for each image slot
    for (std::size_t i = 0; i < FB_NUM; ++i) {
        // create command buffer memblock
        s_cmdMemBlock[i] = dk::MemBlockMaker{s_device, imgui::deko3d::align(CMDBUF_SIZE, DK_MEMBLOCK_ALIGNMENT)}
            .setFlags(DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached)
            .create();

        // create command buffer
        s_cmdBuf[i] = dk::CmdBufMaker{s_device}.create();
        s_cmdBuf[i].addMemory(s_cmdMemBlock[i], 0, s_cmdMemBlock[i].getSize());
    }

    // create image/sampler memblock
    static_assert(sizeof(dk::ImageDescriptor)   == DK_IMAGE_DESCRIPTOR_ALIGNMENT);
    static_assert(sizeof(dk::SamplerDescriptor) == DK_SAMPLER_DESCRIPTOR_ALIGNMENT);
    static_assert(DK_IMAGE_DESCRIPTOR_ALIGNMENT == DK_SAMPLER_DESCRIPTOR_ALIGNMENT);
    s_descriptorMemBlock = dk::MemBlockMaker{s_device, imgui::deko3d::align((MAX_SAMPLERS + MAX_IMAGES) * sizeof(dk::ImageDescriptor), DK_MEMBLOCK_ALIGNMENT)}
        .setFlags(DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached)
        .create();

    // get cpu address for descriptors
    s_samplerDescriptors = static_cast<dk::SamplerDescriptor *> (s_descriptorMemBlock.getCpuAddr());
    s_imageDescriptors = reinterpret_cast<dk::ImageDescriptor *> (&s_samplerDescriptors[MAX_SAMPLERS]);

    // create queue
    s_queue = dk::QueueMaker{s_device}.setFlags(DkQueueFlags_Graphics).create();
    dk::UniqueCmdBuf &cmdBuf = s_cmdBuf[0];

    // bind image/sampler descriptors
    cmdBuf.bindSamplerDescriptorSet(s_descriptorMemBlock.getGpuAddr(), MAX_SAMPLERS);
    cmdBuf.bindImageDescriptorSet(s_descriptorMemBlock.getGpuAddr() + MAX_SAMPLERS * sizeof(dk::SamplerDescriptor), MAX_IMAGES);
    s_queue.submitCommands(cmdBuf.finishList());
    s_queue.waitIdle();
    cmdBuf.clear();
}

void exit_deko3d(void) {
    // clean up all of the deko3d objects
    s_imageMemBlock = nullptr;
    s_descriptorMemBlock = nullptr;

    for (unsigned i = 0; i < FB_NUM; ++i) {
        s_cmdBuf[i] = nullptr;
        s_cmdMemBlock[i] = nullptr;
    }

    s_queue = nullptr;
    s_swapchain = nullptr;
    s_fbMemBlock = nullptr;
    s_depthMemBlock = nullptr;
    s_device = nullptr;
}

// SOURCE: https://github.com/joel16/NX-Shell/blob/5a5067afeb6b18c0d2bb4d7b16f71899a768012a/source/textures.cpp#L150
auto Texture::init(int w, int h, int bpp, DkImageFormat f, int id, void* data) -> void
{
    width = w;
    height = h;
    format = f;
    image_id = 1 + id;
    sampler_id = 1;
    size = width * height * bpp;

    s_queue.waitIdle();

    dk::ImageLayout layout;
    dk::ImageLayoutMaker{s_device}
        .setFlags(0)
        .setFormat(format)
        .setDimensions(width, height)
        .initialize(layout);

    memBlock = dk::MemBlockMaker{s_device, imgui::deko3d::align(size, DK_MEMBLOCK_ALIGNMENT)}
        .setFlags(DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached)
        .create();

    s_imageMemBlock = dk::MemBlockMaker{s_device, imgui::deko3d::align(layout.getSize(), DK_MEMBLOCK_ALIGNMENT)}
        .setFlags(DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image)
        .create();

    if (data != nullptr)
    {
        std::memcpy(memBlock.getCpuAddr(), data, size);
    }
    else
    {
        std::memset(memBlock.getCpuAddr(), 0, size);
    }

    image.initialize(layout, s_imageMemBlock, 0);
    s_imageDescriptors[image_id].initialize(image);

    dk::ImageView imageView(image);

    s_cmdBuf[0].copyBufferToImage({memBlock.getGpuAddr()}, imageView,
        {0, 0, 0, static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1});

    s_queue.submitCommands(s_cmdBuf[0].finishList());

    // this is a hack because i cba to write good code
    static bool sample_already_init = false;
    if (!sample_already_init)
    {
        s_samplerDescriptors[sampler_id].initialize(dk::Sampler{}
            .setFilter(DkFilter_Nearest, DkFilter_Nearest)
            .setWrapMode(DkWrapMode_ClampToEdge, DkWrapMode_ClampToEdge, DkWrapMode_ClampToEdge));
    }

    s_queue.waitIdle();
}

auto Texture::init(const char* file, int id) -> void
{
    // Load from disk into a raw RGBA buffer
    int image_width = 0;
    int image_height = 0;
    int bpp = 0;
    unsigned char* image_data = stbi_load(file, &image_width, &image_height, NULL, 4);
    if (image_data == NULL)
    {
        std::printf("failed to load image: %s\n", file);
        return;
    }

    init(image_width, image_height, 4, DkImageFormat_RGBA8_Unorm, id, image_data);
    stbi_image_free(image_data);

    std::printf("loaded file\n");
}

auto Texture::update(void* data) -> void
{
    s_queue.waitIdle(); // is this needed?

    std::memcpy(memBlock.getCpuAddr(), data, size);

    dk::ImageView imageView(image);

    s_cmdBuf[0].copyBufferToImage({memBlock.getGpuAddr()}, imageView,
        {0, 0, 0, static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1});

    s_queue.submitCommands(s_cmdBuf[0].finishList());

    s_queue.waitIdle();
}

auto Texture::quit() -> void
{
    this->memBlock = nullptr;
}

} // namespace

[[nodiscard]] auto init() -> bool
{
    // only application applet supported
    if (appletGetAppletType() != AppletType_Application)
    {
        applet_show_error_message("Unsupported Launch!", "Please launch as application!");
        return false;
    }

    if (!imgui::nx::init())
    {
        applet_show_error_message("Failed to init imgui!", "");
        return false;
    }

    // init deko3d
    deko3d_init();

    // init deko3d for imgui
    imgui::deko3d::init(s_device, s_queue, s_cmdBuf[0], s_samplerDescriptors[0], s_imageDescriptors[0], dkMakeTextureHandle(0, 0), FB_NUM);

    textures[std::to_underlying(TextureID::emu)].init(240, 160, sizeof(u16), DkImageFormat_RGB5_Unorm, std::to_underlying(TextureID::emu));
    textures[std::to_underlying(TextureID::layer0)].init(240, 160, sizeof(u16), DkImageFormat_RGB5_Unorm, std::to_underlying(TextureID::layer0));
    textures[std::to_underlying(TextureID::layer1)].init(240, 160, sizeof(u16), DkImageFormat_RGB5_Unorm, std::to_underlying(TextureID::layer1));
    textures[std::to_underlying(TextureID::layer2)].init(240, 160, sizeof(u16), DkImageFormat_RGB5_Unorm, std::to_underlying(TextureID::layer2));
    textures[std::to_underlying(TextureID::layer3)].init(240, 160, sizeof(u16), DkImageFormat_RGB5_Unorm, std::to_underlying(TextureID::layer3));

    textures[std::to_underlying(TextureID::folder_icon)].init("romfs:/icons/icons8-mac-folder-64.png", std::to_underlying(TextureID::folder_icon));
    textures[std::to_underlying(TextureID::file_icon)].init("romfs:/icons/icons8-visual-game-boy-48.png", std::to_underlying(TextureID::file_icon));

    // setup callback for applet events
    appletHook(&appletHookCookie, appplet_hook_calback, nullptr);

    // setup controller
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    if (!nx::audio::init())
    {
        applet_show_error_message("failed to open audio!", "");
        return false;
    }

    return true;
}

auto quit() -> void
{
    nx::audio::quit();

    imgui::nx::exit();

    // wait for queue to be idle
    s_queue.waitIdle();

    for (auto& texture : textures)
    {
        texture.quit();
    }

    // close deko3d for imgui
    imgui::deko3d::exit();

    // close deko3d
    exit_deko3d();

    // unhook the applet callback
    appletUnhook(&appletHookCookie);
}


auto poll_events() -> void
{
    if (!appletMainLoop())
    {
        System::running = false;
        return;
    }

    padUpdate(&pad);
    const auto buttons = padGetButtons(&pad);
    const auto down = padGetButtonsDown(&pad);

    System::emu_set_button(gba::A, !!(buttons & HidNpadButton_A));
    System::emu_set_button(gba::B, !!(buttons & HidNpadButton_B));
    System::emu_set_button(gba::L, !!(buttons & HidNpadButton_L));
    System::emu_set_button(gba::R, !!(buttons & HidNpadButton_R));
    System::emu_set_button(gba::START, !!(buttons & HidNpadButton_Plus));
    System::emu_set_button(gba::SELECT, !!(buttons & HidNpadButton_Minus));
    System::emu_set_button(gba::UP, !!(buttons & HidNpadButton_AnyUp));
    System::emu_set_button(gba::DOWN, !!(buttons & HidNpadButton_AnyDown));
    System::emu_set_button(gba::LEFT, !!(buttons & HidNpadButton_AnyLeft));
    System::emu_set_button(gba::RIGHT, !!(buttons & HidNpadButton_AnyRight));

    if (!!(down & HidNpadButton_ZR))
    {
        System::running = false;
    }

    if (!!(down & HidNpadButton_ZL))
    {
        System::loadstate(System::rom_path);
    }

    if (!!(down & HidNpadButton_Y))
    {
        show_fs_browser ^= 1;
        if (System::has_rom)
        {
            // if showing browser, stop running, else run
            System::emu_run = !show_fs_browser;
        }
    }

    // this only update inputs and screen size
    // so it should be called in poll events
    imgui::nx::newFrame(&pad);
}

void show_debug_monitor()
{
    static int corner = -1;
    ImGuiIO& io = ImGui::GetIO();
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (corner != -1)
    {
        const float PAD = 10.0f;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
        ImVec2 work_size = viewport->WorkSize;
        ImVec2 window_pos, window_pos_pivot;
        window_pos.x = (corner & 1) ? (work_pos.x + work_size.x - PAD) : (work_pos.x + PAD);
        window_pos.y = (corner & 2) ? (work_pos.y + work_size.y - PAD) : (work_pos.y + PAD);
        window_pos_pivot.x = (corner & 1) ? 1.0f : 0.0f;
        window_pos_pivot.y = (corner & 2) ? 1.0f : 0.0f;
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        window_flags |= ImGuiWindowFlags_NoMove;
    }

    ImGui::SetNextWindowBgAlpha(0.50f); // Transparent background
    if (ImGui::Begin("NX overlay", nullptr, window_flags))
    {
        ImGui::Text("BEEG Debug Monitor\n");
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Memory"))
        {
            // SOURCE: https://switchbrew.org/wiki/SVC#SystemInfoType
            constexpr u64 TotalPhysicalMemorySize_Application = 0;
            constexpr u64 TotalPhysicalMemorySize_Applet = 1;
            constexpr u64 TotalPhysicalMemorySize_System = 2;
            constexpr u64 TotalPhysicalMemorySize_SystemUnsafe = 3;

            constexpr u64 UsedPhysicalMemorySize_Application = 0;
            constexpr u64 UsedPhysicalMemorySize_Applet = 1;
            constexpr u64 UsedPhysicalMemorySize_System = 2;
            constexpr u64 UsedPhysicalMemorySize_SystemUnsafe = 3;

            constexpr u8 svcGetSystemInfo_id = 0x6F;

            // check if we have priv to use the sys call!
            if (envIsSyscallHinted(svcGetSystemInfo_id))
            {
                u64 total_application = 0;
                u64 total_applet = 0;
                u64 total_system = 0;
                u64 total_unsafe = 0;

                u64 used_application = 0;
                u64 used_applet = 0;
                u64 used_system = 0;
                u64 used_unsafe = 0;

                svcGetSystemInfo(&total_application, 0, INVALID_HANDLE, TotalPhysicalMemorySize_Application);
                svcGetSystemInfo(&total_applet, 0, INVALID_HANDLE, TotalPhysicalMemorySize_Applet);
                svcGetSystemInfo(&total_system, 0, INVALID_HANDLE, TotalPhysicalMemorySize_System);
                svcGetSystemInfo(&total_unsafe, 0, INVALID_HANDLE, TotalPhysicalMemorySize_SystemUnsafe);

                svcGetSystemInfo(&used_application, 1, INVALID_HANDLE, UsedPhysicalMemorySize_Application);
                svcGetSystemInfo(&used_applet, 1, INVALID_HANDLE, UsedPhysicalMemorySize_Applet);
                svcGetSystemInfo(&used_system, 1, INVALID_HANDLE, UsedPhysicalMemorySize_System);
                svcGetSystemInfo(&used_unsafe, 1, INVALID_HANDLE, UsedPhysicalMemorySize_SystemUnsafe);

                ImGui::Text("[Application]  %.2f MB\t%.2f MB\n", (double)used_application / 1024.0 / 1024.0, (double)total_application / 1024.0 / 1024.0);
                ImGui::Text("[Applet]       %.2f MB\t%.2f MB\n", (double)used_applet / 1024.0 / 1024.0, (double)total_applet / 1024.0 / 1024.0);
                ImGui::Text("[System]       %.2f MB\t%.2f MB\n", (double)used_system / 1024.0 / 1024.0, (double)total_system / 1024.0 / 1024.0);
                ImGui::Text("[SystemUnsafe] %.2f MB\t%.2f MB\n", (double)used_unsafe / 1024.0 / 1024.0, (double)total_unsafe / 1024.0 / 1024.0);
            }
        }

        if (ImGui::CollapsingHeader("Audio"))
        {

        }

        if (ImGui::CollapsingHeader("Display"))
        {

        }

        if (ImGui::CollapsingHeader("Misc"))
        {

        }
    }
    ImGui::End();
}

auto render_begin() -> void
{
}

auto render() -> void
{
    show_debug_monitor();

    if (!sys::System::has_rom || show_fs_browser)
    {
        // fs returns true when a rom has been loaded
        show_fs_browser = !nx::fs::render();
    }
}

auto render_end() -> void
{
    ImGuiIO &io = ImGui::GetIO();
    if (s_width != io.DisplaySize.x || s_height != io.DisplaySize.y)
    {
        s_width  = io.DisplaySize.x;
        s_height = io.DisplaySize.y;
        RebuildSwapchain(s_width, s_height);
    }

    // get image from queue
    const int slot = s_queue.acquireImage(s_swapchain);
    dk::UniqueCmdBuf &cmdBuf = s_cmdBuf[slot];
    cmdBuf.clear();

    // bind frame/depth buffers and clear them
    dk::ImageView colorTarget{s_frameBuffers[slot]};
    dk::ImageView depthTarget{s_depthBuffer};
    cmdBuf.bindRenderTargets(&colorTarget, &depthTarget);
    cmdBuf.setScissors(0, DkScissor{0, 0, s_width, s_height});
    cmdBuf.clearColor(0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f, 1.0f);
    cmdBuf.clearDepthStencil(true, 1.0f, 0xFF, 0);
    s_queue.submitCommands(cmdBuf.finishList());

    imgui::deko3d::render(s_device, s_queue, cmdBuf, slot);

    // wait for fragments to be completed before discarding depth/stencil buffer
    cmdBuf.barrier(DkBarrier_Fragments, 0);
    cmdBuf.discardDepthStencil();

    // present image
    s_queue.presentImage(s_swapchain, slot);
}

auto get_texture(TextureID id) -> void*
{
    auto& texture = textures[std::to_underlying(id)];
    return imgui::deko3d::makeTextureID(dkMakeTextureHandle(texture.get_image_id(), texture.get_sampler_id()));
}

auto update_texture(TextureID id, std::uint16_t pixels[160][240]) -> void
{
    textures[std::to_underlying(id)].update(pixels);
}

[[nodiscard]] auto get_window_size() -> std::pair<int, int>
{
    // todo: have callback for size changes
    // return {s_width, s_height};
    return {1280, 720};
}

auto set_window_size(std::pair<int, int> new_size) -> void
{
}

[[nodiscard]] auto is_fullscreen() -> bool
{
    return true;
}

auto toggle_fullscreen() -> void
{
}

auto open_url(const char* url) -> void
{
    WebCommonConfig config{};

    auto rc = webPageCreate(&config, url);

    if (R_SUCCEEDED(rc))
    {
        rc = webConfigSetWhitelist(&config, "^http*");
        if (R_SUCCEEDED(rc))
        {
            rc = webConfigShow(&config, nullptr);
            if (R_SUCCEEDED(rc))
            {
            }
        }
    }
}

} // sys::backend
