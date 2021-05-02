// Dear ImGui: standalone example application for GLFW + OpenGL2, using legacy fixed pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

// **DO NOT USE THIS CODE IF YOUR CODE/ENGINE IS USING MODERN OPENGL (SHADERS, VBO, VAO, etc.)**
// **Prefer using the code in the example_glfw_opengl2/ folder**
// See imgui_impl_glfw.cpp for details.

#include "imgui/imgui.h"
#include "backend/imgui_impl_glfw.h"
#include "backend/imgui_impl_opengl2.h"
#include <stdio.h>

#include "libuio.h"
#include <unistd.h>

#define MES_CLK_DEVNAME "measure_clk"
#define MES_CLK_REFCLK_HDL 100 * 1000 * 1000.0 // 100 MHz
enum MES_CLK_REGS
{
    MES_CLK_RESET = 0x0,
    MES_CLK_ENABLE = 0x4,
    MES_CLK_GUID = 0x8,
    
    MES_CLK_RST = 0x100,
    MES_CLK_EN = 0x200,

    MES_CLK_REFCYC = 0x104,

    MES_CLK_MESCYC = 0x108,

    MES_CLK_DBG_REFCYC = 0x300,
    MES_CLK_DBG_REFCLK = 0x304,
    MES_CLK_DBG_MESCLK = 0x308
};

#define eprintf(str, ...) \
    fprintf(stderr, "%s, %d: " str "\n", __func__, __LINE__, ##__VA_ARGS__); \
    fflush(stderr)

#include <stdlib.h>

#define MES_CLK_IPCORE_RST "991"
#define MES_CLK_IPCORE_RST_TOUT 1000 // 1 ms
static int mesclk_ipcore_rst()
{
    FILE *fp;
    ssize_t size;
    fp = fopen("/sys/class/gpio/export", "w");
    if (fp == NULL)
    {
        eprintf("Error opening");
        perror("gpioexport: ");
        goto exitfunc;
    }
    size = fprintf(fp, "%s", MES_CLK_IPCORE_RST);
    if (size <= 0)
    {
        eprintf("Error writing to ");
        perror("gpioexport: ");
        goto closefile;
    }
    fclose(fp);
    usleep(10000);
    fp = fopen("/sys/class/gpio/gpio" MES_CLK_IPCORE_RST "/direction", "w");
    if (fp == NULL)
    {
        eprintf("Error opening ");
        perror("gpiodirection: ");
        goto exitfunc;
    }
    size = fprintf(fp, "out");
    if (size <= 0)
    {
        eprintf("Error writing to ");
        perror("gpiodirection: ");
        goto closefile;
    }
    fclose(fp);
    usleep(10000);
    fp = fopen("/sys/class/gpio/gpio" MES_CLK_IPCORE_RST "/value", "w");
    if (fp == NULL)
    {
        eprintf("Error opening ");
        perror("gpiovalue: ");
        goto exitfunc;
    }
    size = fprintf(fp, "1");
    if (size <= 0)
    {
        eprintf("Error writing to ");
        perror("gpiovalue: ");
        goto closefile;
    }
    fclose(fp);
    usleep(MES_CLK_IPCORE_RST_TOUT);
    fp = fopen("/sys/class/gpio/gpio" MES_CLK_IPCORE_RST "/value", "w");
    if (fp == NULL)
    {
        eprintf("Error opening ");
        perror("gpiovalue: ");
        goto exitfunc;
    }
    size = fprintf(fp, "0");
    if (size <= 0)
    {
        eprintf("Error writing to ");
        perror("gpiovalue: ");
        goto closefile;
    }
    fclose(fp);
    return EXIT_SUCCESS;
closefile:
    fclose(fp);
exitfunc:
    return EXIT_FAILURE;
}

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif
#include <GLFW/glfw3.h>

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

int main(int, char**)
{
    // Setup UIO device
    uio_dev dev[1];
    if (uio_init(dev, uio_get_id(MES_CLK_DEVNAME)) < 0)
    {
        eprintf("Error initializing UIO device %s, ID: %d", MES_CLK_DEVNAME, uio_get_id(MES_CLK_DEVNAME));
        return 0;
    }
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Clock Measurement IP Controller", NULL, NULL);
    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Our state
    static bool enable = false;
    uio_write(dev, MES_CLK_RST, 1);
    uint32_t tstamp = 0x0;
    uio_read(dev, MES_CLK_GUID, &tstamp);
    char winName[128];
    snprintf(winName, 128, "Clock Measurement Tool (Rev %u)", tstamp);
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    static int refcycles = 100000;
    uio_write(dev, MES_CLK_REFCYC, refcycles);
    static int mescycles = 0;
    static int dbg_refcycles = 0;
    static int dbg_refclk = 0;
    static int dbg_mesclk = 0;
    static float clk_freq = 0;
    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        uio_read(dev, MES_CLK_REFCYC, (uint32_t *)&refcycles);
        uio_read(dev, MES_CLK_MESCYC, (uint32_t *)&mescycles);
        uio_read(dev, MES_CLK_DBG_REFCYC, (uint32_t *)&dbg_refcycles);
        uio_read(dev, MES_CLK_DBG_REFCLK, (uint32_t *)&dbg_refclk);
        uio_read(dev, MES_CLK_DBG_MESCLK, (uint32_t *)&dbg_mesclk);
        
        clk_freq = mescycles * MES_CLK_REFCLK_HDL / refcycles;

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {
            ImGui::Begin(winName);                          // Create a window called "Hello, world!" and append into it.
            ImGui::Text("Controls");
            if (ImGui::Checkbox("Enable", &enable))
            {
                uio_write(dev, MES_CLK_EN, (uint32_t) enable);
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset"))
            {
                uio_write(dev, MES_CLK_RST, 0x0);
                usleep(100);
                uio_write(dev, MES_CLK_RST, 0x1);
            }
            ImGui::SameLine();
            if (ImGui::Button("UIO Reset"))
            {
                uio_write(dev, MES_CLK_RESET, 0x1);
            }
            if (ImGui::Button("IPCore Reset"))
            {
                mesclk_ipcore_rst();
            }
            if (ImGui::InputInt("Reference Cycles", &refcycles, 0, 0, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
            {
                uio_write(dev, MES_CLK_REFCYC, (uint32_t) refcycles);
            }
            ImGui::Separator();
            ImGui::Text("Outputs:");
            ImGui::InputInt("Measured Cycles", &mescycles, 0, 0, ImGuiInputTextFlags_ReadOnly);
            ImGui::InputFloat("Clock Frequency", &clk_freq, 0, 0, "%.3f MHz", ImGuiInputTextFlags_ReadOnly);
            ImGui::Separator();
            ImGui::Text("Debug Outputs:");
            ImGui::InputInt("Debug Ref Cycles", &dbg_refcycles, 0, 0, ImGuiInputTextFlags_ReadOnly);
            ImGui::InputInt("Debug Reference Clock", &dbg_refclk, 0, 0, ImGuiInputTextFlags_ReadOnly);
            ImGui::InputInt("Debug Measured Clock", &dbg_mesclk, 0, 0, ImGuiInputTextFlags_ReadOnly);
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        // If you are using this code with non-legacy OpenGL header/contexts (which you should not, prefer using imgui_impl_opengl3.cpp!!),
        // you may need to backup/reset/restore current shader using the commented lines below.
        //GLint last_program;
        //glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
        //glUseProgram(0);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        //glUseProgram(last_program);

        glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    uio_write(dev, MES_CLK_EN, 0);
    uio_destroy(dev);
    return 0;
}
