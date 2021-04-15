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
#include "rxmodem.h"
#include "txmodem.h"
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

static char hostname[256];
static bool isGround = false;

#ifdef ENABLE_MODEM
bool show_chat_win = false;

#define RX_BUF_SIZE 8192
char rx_buf[RX_BUF_SIZE];
ssize_t rx_buf_sz = 1;
pthread_mutex_t rx_buf_access[1];

rxmodem rxdev[1];
void *rx_thread_fcn(void *tid)
{
    static int retval;
    if (rxmodem_init(rxdev, uio_get_id("rx_ipcore"), uio_get_id("rx_dma")) < 0)
    {
        memset(rx_buf, 0x0, RX_BUF_SIZE);
        rx_buf_sz = snprintf(rx_buf, RX_BUF_SIZE, "%s: Could not initialize RX Modem with uio devices %d and %d", __func__, uio_get_id("rx_ipcore"), uio_get_id("rx_dma"));
        rx_buf_sz++;
        retval = -1;
        goto err;
    }
    while (!done)
    {
        if (show_chat_win)
        {
            ssize_t rcv_sz = rxmodem_receive(rxdev);
            if (rcv_sz <= 0)
            {
                pthread_mutex_lock(rx_buf_access);
                memset(rx_buf, 0x0, RX_BUF_SIZE);
                snprintf(rx_buf, RX_BUF_SIZE, "Receive size invalid: %d", rcv_sz);
                pthread_mutex_unlock(rx_buf_access);
                continue;
            }
            char *buf = (char *)malloc(rcv_sz);
            memset(buf, 0x0, rcv_sz);
            ssize_t rd_sz = rxmodem_read(rxdev, (uint8_t *) buf, rcv_sz);
            pthread_mutex_lock(rx_buf_access);
            memset(rx_buf, 0x0, RX_BUF_SIZE);
            snprintf(rx_buf, rd_sz, "%s", buf);
            rx_buf_sz = rd_sz;
            if (rd_sz != rcv_sz)
            {
                rx_buf[rd_sz] = '\n';
                rx_buf_sz++;
                rx_buf_sz += snprintf(rx_buf + rd_sz + 1, RX_BUF_SIZE - rd_sz - 1, "Invalid read: %d out of %d", rd_sz, rcv_sz);
                rx_buf_sz++;
            }
            pthread_mutex_unlock(rx_buf_access);
            free(buf);
        }
        else
        {
            usleep(16000); // 60 Hz
        }
    }
err:
    return &retval;
}

static txmodem txdev[1];
#define TX_BUF_SIZE 4096
static char tx_buf[TX_BUF_SIZE];
static char tmptxbuf[4000];
void ChatWin(bool *active)
{
    static bool firstRun = true;
    static char chatwindowname[256];
    static bool transmitted = false;
    time_t rawtime;
    static struct tm *timeinfo;
    static int mtu = 0;
    static int fr_loop_idx = 40;
    if (firstRun)
    {
        snprintf(tmptxbuf, 4000, "Testing...");
        snprintf(chatwindowname, 256, "Chat @%s", hostname);
        firstRun = false;
    }
    ImGui::Begin(chatwindowname, active);
    pthread_mutex_lock(rx_buf_access);
    ImGui::TextWrapped("Received: %s", rx_buf);
    pthread_mutex_unlock(rx_buf_access);
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() * 0.4);
    if (ImGui::InputInt("FR Loop BW", &fr_loop_idx, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
    {
        if (fr_loop_idx < 0 || fr_loop_idx > 127)
            fr_loop_idx = 40;
        rxdev->conf->fr_loop_bw = fr_loop_idx;
        rxmodem_reset(rxdev, rxdev->conf);
        rxmodem_start(rxdev);
    }
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() * 0.5);
    ImGui::InputTextMultiline("To Send", tmptxbuf, 4000, ImVec2(ImGui::GetWindowWidth() - 100, ImGui::GetWindowHeight() * 0.4 - 20), ImGuiInputTextFlags_AutoSelectAll);
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() * 0.9);
    if (ImGui::InputInt("MTU", &mtu, 0, 0, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
    {
        if (mtu < 0)
            mtu = 0;
        txdev->mtu = mtu;
    }
    ImGui::SameLine();
    if (ImGui::Button("Transmit"))
    {
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        ssize_t sz = snprintf(tx_buf, TX_BUF_SIZE, "%s (%04d-%02d-%02d %02d:%02d:%02d) > %s", hostname, timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, tmptxbuf);
        sz++;
        txmodem_write(txdev, (uint8_t *)tx_buf, sz);
        if (strlen(tmptxbuf) < 2)
        {
            snprintf(tmptxbuf, 4000, "Testing...");
        }
        transmitted = true;
    }
    if (transmitted)
    {
        ImGui::Text("Last transmitted at: %04d-%02d-%02d %02d:%02d:%02d", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    }
    ImGui::End();
}
#endif // ENABLE_MODEM

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
    static char *phymodestr[] = {"Sleep", "FDD", "TDD"};
    static char ftr_fname[128];
    static char ensmmode[256];
    enum ensm_mode phymode;
    char curgainmode[32];
    adradio_get_ensm_mode(phy, ensmmode, 256);
    if (strncmp(ensmmode, "fdd", 20) == 0)
        phymode = FDD;
    else if (strncmp(ensmmode, "sleep", 20) == 0)
        phymode = SLEEP;
    else if (strncmp(ensmmode, "tdd", 20) == 0)
        phymode = TDD;
    adradio_get_rx_hardwaregainmode(phy, curgainmode, IM_ARRAYSIZE(curgainmode));
    adradio_get_temp(phy, &temp);
    adradio_get_rssi(phy, &rssi);
    ImGui::Columns(4, "phy_sensors", true);
    ImGui::Text("System Mode: %s", ensmmode);
    ImGui::NextColumn();
    ImGui::Text("Temperature: %.3f °C", temp * 0.001);
    ImGui::NextColumn();
    ImGui::Text("RSSI: %.2lf dB", rssi);
    ImGui::NextColumn();
    ImGui::Text("Gain Control: %s", curgainmode);

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
    adradio_get_samp(phy, &samp);
    adradio_get_tx_hardwaregain(phy, &gain);
    if (firstrun)
    {
        _lo_tx = lo * 1e-6;
        _bw_tx = bw * 1e-6;
        _samp_tx = samp * 1e-6;
        gain_tx = gain;
        snprintf(ftr_fname, 128, "Enter Filter File Name");
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
    adradio_get_samp(phy, &samp);
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
    if (ImGui::InputFloat("Sample Rate (MHz)", &_samp_tx, 0, 0, "%.3f", ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
    {
        adradio_set_samp(phy, MHZ(_samp_tx));
    }
    ImGui::Separator();
    if (ImGui::InputFloat("TX LO (MHz)", &_lo_tx, 0, 0, "%.3f", ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
    {
        adradio_set_tx_lo(phy, MHZ(_lo_tx));
    }
    if (ImGui::InputFloat("TX BW (MHz)", &_bw_tx, 0, 0, "%.3f", ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
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
    if (ImGui::InputFloat("RX BW (MHz)", &_bw_rx, 0, 0, "%.3f", ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
    {
        adradio_set_rx_bw(phy, MHZ(_bw_rx));
    }
    if (ImGui::Combo("RX Gain Control Mode", (int *)&gainmode, gainmodestr, IM_ARRAYSIZE(gainmodestr)))
    {
        adradio_set_rx_hardwaregainmode(phy, (enum gain_mode)(gainmode + 1));
    }
    ImGui::Separator();
    static bool ftr_en = true;
    adradio_check_fir(phy, &ftr_en);
    if (ImGui::Checkbox("Enable FIR Filter", &ftr_en))
    {
        adradio_enable_fir(phy, ftr_en);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload DDS Config"))
    {
        adradio_reconfigure_dds(phy);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Default"))
    {
        adradio_enable_fir(phy, false);
        adradio_set_samp(phy, 10000000);
        adradio_set_rx_bw(phy, 10000000);
        adradio_set_tx_bw(phy, 10000000);
        adradio_set_tx_lo(phy, isGround ? 2400000000 : 2500000000);
        adradio_set_rx_lo(phy, isGround ? 2500000000 : 2400000000);
        adradio_reconfigure_dds(phy);
    }
    ImGui::InputText("Filter File", ftr_fname, IM_ARRAYSIZE(ftr_fname), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if (ImGui::Button("Load Filter"))
    {
        char buf[256];
        snprintf(buf, 256, "/home/sunip/%s.ftr", ftr_fname);
        if (adradio_load_fir(phy, buf) == EXIT_FAILURE)
        {
            snprintf(ftr_fname, IM_ARRAYSIZE(ftr_fname), "Could not load %s", buf);
        }
    }
    ImGui::Separator();
    if (ImGui::Combo("AD9361 Mode", (int *)&phymode, phymodestr, IM_ARRAYSIZE(phymodestr)))
    {
        adradio_set_ensm_mode(phy, phymode);
    }
#ifdef ENABLE_MODEM
    ImGui::Checkbox("Chat Window", &show_chat_win);
    ImGui::SameLine();
#endif
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
}

int main(int, char **)
{
    // set up phy configuration tool
    if (adradio_init(phy) != EXIT_SUCCESS)
        return 1;
    // set hostname
    char progname[256];
    if (!gethostname(hostname, 256))
        snprintf(progname, 256, "AD9361 Configuration Utility");
    else
        snprintf(progname, 256, "AD9361 Configuration Utility @ %s", hostname);
    if (strncasecmp("adrv9361", hostname, strlen("adrv9361")) == 0)
        isGround = true;
#ifdef ENABLE_MODEM
    // Set up TX modem
    if (txmodem_init(txdev, uio_get_id("tx_ipcore"), uio_get_id("tx_dma")) < 0)
    {
        eprintf("Could not initialize TX modem\n");
        adradio_destroy(phy);
        return 2;
    }
    // Set up RX modem
    pthread_t rxthread;
    if (pthread_create(&rxthread, NULL, &rx_thread_fcn, NULL) != 0)
    {
        eprintf("Could not initialize RX thread\n");
        txmodem_destroy(txdev);
        adradio_destroy(phy);
        return 3;
    }
#endif
    // register signal handler
    signal(SIGINT, &sighandler);
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;
    ImVec2 mainwinsize = {712, 400};
#ifdef ENABLE_MODEM
    GLFWwindow *window = glfwCreateWindow((int)mainwinsize.x, (int)mainwinsize.y * 2, progname, NULL, NULL);
#else
    GLFWwindow *window = glfwCreateWindow((int)mainwinsize.x, (int)mainwinsize.y, progname, NULL, NULL);
#endif
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
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(mainwinsize);
        PhyWin(&show_phy_win);
#ifdef ENABLE_MODEM
        if (show_chat_win)
        {
            ImGui::SetNextWindowPos(ImVec2(0, 400));
            ImGui::SetNextWindowSize(mainwinsize);
            ChatWin(&show_chat_win);
        }
#endif
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
#ifdef ENABLE_MODEM
    show_chat_win = false;
    done = 1;
    pthread_cancel(rxthread);
    txmodem_destroy(txdev);
    rxmodem_destroy(rxdev);
#endif
    adradio_destroy(phy);
    return 0;
}
