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

static void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

#include "libiio.h"
// #include "rxmodem.h"
// #include "txmodem.h"
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#define eprintf(...)              \
    fprintf(stderr, __VA_ARGS__); \
    fflush(stderr)

// #include <openssl/md5.h>

volatile sig_atomic_t done = 0;
void sighandler(int sig)
{
    done = 1;
}

// static char md5digest[MD5_DIGEST_LENGTH];

// void md5sum(const char *fname)
// {
//     if (fname == NULL)
//         return;
//     MD5_CTX c;
//     char buf[512];
//     ssize_t bytes;
//     memset(md5digest, 0x0, MD5_DIGEST_LENGTH);
//     MD5_Init(&c);
//     FILE *fp = fopen(fname, "rb");
//     do
//     {
//         bytes = fread(buf, 0x1, 512, fp);
//         if (bytes > 0)
//             MD5_Update(&c, buf, bytes);
//     } while (bytes == 512);
//     fclose(fp);
//     MD5_Final(md5digest, &c);
// }

// typedef struct
// {
//     ssize_t len;
//     char *buf;
//     pthread_mutex_t lock[1];
// } txt_buf;

// pthread_t rxthread;
// pthread_mutex_t rxthread_wake_m[1];
// pthread_cond_t rxthread_wake[1];
// bool rxthread_rcv_file = false;
// char rxthread_rcv_fname[256];
// pthread_mutex_t rcv_file[1];
// txt_buf rcv_txt[1];

// pthread_t txthread;
// pthread_mutex_t txthread_wake_m[1];
// pthread_cond_t txthread_wake[1];
// bool txthread_send_file = false;
// char txthread_send_fname[256];
// txt_buf send_txt[1];

// void *rxthread_fcn(void *tid)
// {
//     rxmodem dev[1];
//     if (rxmodem_init(dev, 0, 2) < 0)
//     {
//         printf("error initializing RX device\n");
//         return NULL;
//     }
//     memset(rcv_txt, 0x0, sizeof(rcv_txt));
//     pthread_mutex_init(rcv_txt->lock, NULL);
//     rxmodem_reset(dev, dev->conf);
//     while (!done)
//     {
//         pthread_cond_wait(rxthread_wake, rxthread_wake_m);
//         ssize_t rcv_sz = rxmodem_receive(dev);
//         if (rcv_sz < 0)
//         {
//             eprintf("%s: Receive size = %d\n", __func__, rcv_sz);
//             continue;
//         }
//         printf("%s: Received data size: %d\n", __func__, rcv_sz);
//         fflush(stdout);
//         if (!rxthread_rcv_file)
//         {
//             pthread_mutex_lock(rcv_txt->lock);
//             if (rcv_txt->buf != NULL)
//             {
//                 free(rcv_txt->buf);
//                 rcv_txt->len = 0;
//             }
//             rcv_txt->buf = (char *)malloc(rcv_sz);
//             rcv_txt->len = rcv_sz;
//             ssize_t rd_sz = rxmodem_read(dev, (uint8_t *)(rcv_txt->buf), rcv_txt->len);
//             pthread_mutex_unlock(rcv_txt->lock);
//             if (rcv_sz != rd_sz)
//             {
//                 eprintf("%s: Read size = %d out of %d\n", __func__, rd_sz, rcv_sz);
//             }
//         }
//         else
//         {

//         }
//     }
// }

adradio_t phy[1];

bool show_phy_win = true;
void PhyWin(bool *active)
{
    ImGui::Begin("Configure AD9361", active);
    static bool firstrun = true;
    static long long lo, bw, samp, temp;
    static float _lo_rx, _bw_rx, _samp_rx;
    static float _lo_tx, _bw_tx, _samp_tx;
    static double rssi, gain;
    static float gain_tx;
    static int gainmode;
    static char *gainmodestr[] = {"slow_attack", "fast_attack"};
    static char ftr_fname[256];
    char curgainmode[32];
    adradio_get_rx_hardwaregainmode(phy, curgainmode, IM_ARRAYSIZE(curgainmode));
    adradio_get_temp(phy, &temp);
    adradio_get_rssi(phy, &rssi);
    ImGui::Columns(3, "phy_sensors", true);
    ImGui::Text("Temperature: %.3f °C", temp * 0.001);
    ImGui::NextColumn();
    ImGui::Text("RSSI: %.2lf dB", rssi);
    ImGui::NextColumn();
    ImGui::Text("Gain Control Mode: %s", curgainmode);

    ImGui::Columns(5, "phy_outputs", true);
    ImGui::Text(" ");
    ImGui::NextColumn();
    ImGui::Text("LO (Hz)");
    ImGui::NextColumn();
    ImGui::Text("Samp (Hz)");
    ImGui::NextColumn();
    ImGui::Text("BW (Hz)");
    ImGui::NextColumn();
    ImGui::Text("Power/Gain (dBm/dB)");
    ImGui::NextColumn();

    adradio_get_tx_lo(phy, &lo);
    adradio_get_tx_bw(phy, &bw);
    adradio_get_tx_samp(phy, &samp);
    adradio_get_tx_hardwaregain(phy, &gain);
    if (firstrun)
    {
        _lo_tx = lo * 1e-6;
        _bw_tx = bw * 1e-6;
        _samp_tx = samp * 1e-6;
        gain_tx = gain;
        snprintf(ftr_fname, 256, "Enter Filter File Name");
    }
    ImGui::Text("TX:");
    ImGui::NextColumn();
    ImGui::Text("%lld", lo);
    ImGui::NextColumn();
    ImGui::Text("%lld", samp);
    ImGui::NextColumn();
    ImGui::Text("%lld", bw);
    ImGui::NextColumn();
    ImGui::Text("%lf dBm", gain);
    ImGui::NextColumn();

    adradio_get_rx_lo(phy, &lo);
    adradio_get_rx_bw(phy, &bw);
    adradio_get_rx_samp(phy, &samp);
    adradio_get_rx_hardwaregain(phy, &gain);
    if (firstrun)
    {
        _lo_rx = lo * 1e-6;
        _bw_rx = bw * 1e-6;
        _samp_rx = samp * 1e-6;
        if (strncmp(curgainmode, gainmodestr[SLOW_ATTACK - 1], strlen(gainmodestr[SLOW_ATTACK - 1])) == 0)
            gainmode = SLOW_ATTACK - 1;
        else if (strncmp(curgainmode, gainmodestr[FAST_ATTACK - 1], strlen(gainmodestr[FAST_ATTACK - 1])) == 0)
            gainmode = FAST_ATTACK - 1;
        else
            gainmode = SLOW_ATTACK - 1;
        firstrun = false;
    }
    ImGui::Text("RX:");
    ImGui::NextColumn();
    ImGui::Text("%lld", lo);
    ImGui::NextColumn();
    ImGui::Text("%lld", samp);
    ImGui::NextColumn();
    ImGui::Text("%lld", bw);
    ImGui::NextColumn();
    ImGui::Text("%lf dB", gain);

    ImGui::Columns(1);
    ImGui::Text("Set Outputs: ");
    if (ImGui::InputFloat("TX LO (MHz)", &_lo_tx, 0, 0, "%.3f", ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
    {
        adradio_set_tx_lo(phy, MHZ(_lo_tx));
    }
    if (ImGui::InputFloat("TX Samp (MHz)", &_samp_tx, 0, 0, "%.3f", ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
    {
        adradio_set_tx_samp(phy, MHZ(_samp_tx));
    }
    if (ImGui::InputFloat("TX BW", &_bw_tx, 0, 0, "%.3f", ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
    {
        adradio_set_tx_bw(phy, MHZ(_bw_tx));
    }
    if (ImGui::InputFloat("TX Power (dBm)", &gain_tx, 0, 0, "%.1f", ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
    {
        adradio_set_tx_hardwaregain(phy, gain_tx);
    }
    ImGui::Separator();
    if (ImGui::InputFloat("RX LO (MHz)", &_lo_rx, 0, 0, "%.3f", ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
    {
        adradio_set_rx_lo(phy, MHZ(_lo_rx));
    }
    if (ImGui::InputFloat("RX Samp (MHz)", &_samp_rx, 0, 0, "%.3f", ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
    {
        adradio_set_rx_samp(phy, MHZ(_samp_rx));
    }
    if (ImGui::InputFloat("RX BW (MHz)", &_bw_rx, 0, 0, "%.3f", ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
    {
        adradio_set_rx_bw(phy, MHZ(_bw_rx));
    }
    if (ImGui::Combo("RX Gain Control Mode", (int *)&gainmode, gainmodestr, IM_ARRAYSIZE(gainmodestr)))
    {
        adradio_set_rx_hardwaregainmode(phy, (enum gain_mode)(gainmode + 1));
    }
    ImGui::Separator();
    static bool tx_ftr_en = true, rx_ftr_en = true;
    adradio_check_fir(phy, TX, &tx_ftr_en);
    adradio_check_fir(phy, RX, &rx_ftr_en);
    if (ImGui::Checkbox("Enable TX FIR Filter", &tx_ftr_en))
    {
        adradio_enable_fir(phy, TX, tx_ftr_en);
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Enable RX FIR Filter", &rx_ftr_en))
    {
        adradio_enable_fir(phy, RX, rx_ftr_en);
    }
    if (ImGui::InputText("Filter File", ftr_fname, IM_ARRAYSIZE(ftr_fname), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
    {
        char buf[256];
        memcpy(buf, ftr_fname, 255);
        buf[255] = '\0';
        if (access(ftr_fname, F_OK | R_OK))
        {
            snprintf(ftr_fname, IM_ARRAYSIZE(ftr_fname), "Invalid file %s", buf);
        }
        else if (adradio_load_fir(phy, ftr_fname) == EXIT_FAILURE)
        {
            snprintf(ftr_fname, IM_ARRAYSIZE(ftr_fname), "Could not load %s", buf);
        }
    }
    ImGui::Separator();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
}

int main(int, char **)
{
    if (adradio_init(phy) != EXIT_SUCCESS)
        return 0;
    char hostname[256];
    char progname[256];
    if (!gethostname(hostname, 256))
        snprintf(progname, 256, "AD9361 Configuration Utility");
    else
        snprintf(progname, 256, "AD9361 Configuration Utility @%s", hostname);
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;
    GLFWwindow *window = glfwCreateWindow(1280, 720, progname, NULL, NULL);
    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
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
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

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

        show_phy_win = true;
        PhyWin(&show_phy_win);

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

    adradio_destroy(phy);
    return 0;
}
