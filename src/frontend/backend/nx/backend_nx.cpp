// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "ftpd_imgui/imgui_deko3d.h"
#include "ftpd_imgui/imgui_nx.h"
#include "../backend.hpp"
#include "../../system.hpp"
#include <cstdint>
#include <switch/applets/error.h>
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
    auto init() -> void;
    auto quit() -> void;
    auto update(std::uint16_t data[160][240]) -> void;

    [[nodiscard]] auto get_image_id() { return image_id; }
    [[nodiscard]] auto get_sampler_id() { return sampler_id; }

private:
    dk::Image image;
    dk::UniqueMemBlock memBlock;

    // todo: make texture creation like a factory which returns an
    // id (index into texture array)
    static inline const auto image_id = 2;
    static inline const auto sampler_id = 1;

    // thse should be set in init()
    static inline const auto format = DkImageFormat_RGB5_Unorm;
    static inline const auto width = 240;
    static inline const auto height = 160;
    static inline const auto size = width * height * sizeof(uint16_t);
};

constexpr auto MAX_SAMPLERS = 2;
constexpr auto MAX_IMAGES = 8;
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

Texture textures[1];
PadState pad;

AppletHookCookie appletHookCookie;

auto applet_show_error_message(const char* message, const char* long_message)
{
    ErrorApplicationConfig cfg;
    errorApplicationCreate(&cfg, "Unsupported Launch!", "Please launch as application!");
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
auto Texture::init() -> void
{
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

    std::memset(memBlock.getCpuAddr(), 0, size);

    image.initialize(layout, s_imageMemBlock, 0);
    s_imageDescriptors[image_id].initialize(image);

    dk::ImageView imageView(image);

    s_cmdBuf[0].copyBufferToImage({memBlock.getGpuAddr()}, imageView,
        {0, 0, 0, static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1});

    s_queue.submitCommands(s_cmdBuf[0].finishList());

    s_samplerDescriptors[sampler_id].initialize(dk::Sampler{}
        .setFilter(DkFilter_Nearest, DkFilter_Nearest)
        .setWrapMode(DkWrapMode_ClampToEdge, DkWrapMode_ClampToEdge, DkWrapMode_ClampToEdge));

    s_queue.waitIdle();
}

auto Texture::update(std::uint16_t data[160][240]) -> void
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
        applet_show_error_message("Unsupported Launch!", "Please launch as application!");
        return false;
    }

    // init deko3d
    deko3d_init();

    // init deko3d for imgui
    imgui::deko3d::init(s_device, s_queue, s_cmdBuf[0], s_samplerDescriptors[0], s_imageDescriptors[0], dkMakeTextureHandle(0, 0), FB_NUM);

    // init all textures being used
    for (auto& texture : textures)
    {
        texture.init();
    }

    // setup callback for applet events
    appletHook(&appletHookCookie, appplet_hook_calback, nullptr);

    // setup controller
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    return true;
}

auto quit() -> void
{
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
    const auto down = padGetButtons(&pad);

    System::emu_set_button(gba::A, !!(down & HidNpadButton_A));
    System::emu_set_button(gba::B, !!(down & HidNpadButton_B));
    System::emu_set_button(gba::L, !!(down & HidNpadButton_L));
    System::emu_set_button(gba::R, !!(down & HidNpadButton_R));
    System::emu_set_button(gba::START, !!(down & HidNpadButton_Plus));
    System::emu_set_button(gba::SELECT, !!(down & HidNpadButton_Minus));
    System::emu_set_button(gba::UP, !!(down & HidNpadButton_AnyUp));
    System::emu_set_button(gba::DOWN, !!(down & HidNpadButton_AnyDown));
    System::emu_set_button(gba::LEFT, !!(down & HidNpadButton_AnyLeft));
    System::emu_set_button(gba::RIGHT, !!(down & HidNpadButton_AnyRight));

    if (!!(down & HidNpadButton_ZR))
    {
        System::running = false;
    }

    // this only update inputs and screen size
    // so it should be called in poll events
    imgui::nx::newFrame(&pad);
}

auto render_begin() -> void
{
    // imgui::nx::newFrame(&pad);
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
    assert(id == TextureID::emu && "only emu texture is impl!");
    auto& texture = textures[std::to_underlying(id)];
    return imgui::deko3d::makeTextureID(dkMakeTextureHandle(texture.get_image_id(), texture.get_sampler_id()));
}

auto update_texture(TextureID id, std::uint16_t pixels[160][240]) -> void
{
    assert(id == TextureID::emu && "only emu texture is impl!");
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
}

} // sys::backend
