/*******************************************************************************************
*
*   rFXGen v2.0 - A simple and easy to use fx sounds generator (based on Tomas Petterson sfxr)
*
*   CONFIGURATION:
*
*   #define VERSION_ONE
*       Enable PRO features for the tool. Usually command-line and export options related.
*
*   #define RENDER_WAVE_TO_TEXTURE (defined by default)
*       Use RenderTexture2D to render wave on. If not defined, wave is diretly drawn using lines.
*
*   VERSIONS HISTORY:
*       2.0  (xx-Nov-2018) GUI redesigned, CLI improvements
*       1.8  (10-Oct-2018) Functions renaming, code reorganized, better consistency...
*       1.5  (23-Sep-2018) Support .wav export to code and sound playing on command line
*       1.4  (15-Sep-2018) Redesigned command line and comments
*       1.3  (15-May-2018) Reimplemented gui using rGuiLayout
*       1.2  (16-Mar-2018) Working on some code improvements and GUI review
*       1.1  (01-Oct-2017) Code review, simplified
*       1.0  (18-Mar-2017) First release
*       0.9x (XX-Jan-2017) Review complete file...
*       0.95 (14-Sep-2016) Reviewed comments and .rfx format
*       0.9  (12-Sep-2016) Defined WaveParams struct and command line functionality
*       0.8  (09-Sep-2016) Added open/save file dialogs using tinyfiledialogs library
*       0.7  (04-Sep-2016) Program variables renaming for consistency, code reorganized
*       0.6  (30-Aug-2016) Interface redesigned (reduced size) and new features added (wave drawing)
*       0.5  (27-Aug-2016) Completed port and adaptation from sfxr (only sound generation and playing)
*
*   DEPENDENCIES:
*       raylib 2.1-dev          - Windowing/input management and drawing.
*       raygui 2.0              - Immediate-mode GUI controls.
*       tinyfiledialogs 3.3.7   - Open/save file dialogs, it requires linkage with comdlg32 and ole32 libs.
*
*   COMPILATION (Windows - MinGW):
*       gcc -o rfxgen.exe rfxgen.c external/tinyfiledialogs.c -s rfxgen_icon -Iexternal /
*           -lraylib -lopengl32 -lgdi32 -lcomdlg32 -lole32 -std=c99 -Wl,--subsystem,windows
*
*   COMPILATION (Linux - GCC):
*       gcc -o rfxgen rfxgen.c external/tinyfiledialogs.c -s -Iexternal -no-pie -D_DEFAULT_SOURCE /
*           -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
*
*   DEVELOPERS:
*       Ramon Santamaria (@raysan5):   Developer, supervisor, updater and maintainer.
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2014-2018 raylib technologies (@raylibtech).
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

#include "raylib.h"

#define GUI_WINDOW_ABOUT_IMPLEMENTATION
#define RAYGUI_STYLE_SAVE_LOAD
#include "gui_window_about.h"

#define RAYGUI_IMPLEMENTATION
#define RAYGUI_STYLE_SAVE_LOAD
#include "raygui.h"                     // Required for: IMGUI controls

#include "external/tinyfiledialogs.h"   // Required for: Native open/save file dialogs

#include <math.h>                       // Required for: sinf(), pow()
#include <time.h>                       // Required for: clock()
#include <stdlib.h>                     // Required for: malloc(), free()
#include <string.h>                     // Required for: strcmp()
#include <stdio.h>                      // Required for: FILE, fopen(), fread(), fwrite(), ftell(), fseek() fclose()
                                        // NOTE: Used on functions: LoadSound(), SaveSound(), WriteWAV()

#if defined(_WIN32)
    #include <conio.h>                  // Required for: kbhit() [Windows only, no stardard library]
#else
    // Provide kbhit() function in non-Windows platforms
    #include <termios.h>
    #include <unistd.h>
    #include <fcntl.h>
#endif

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
//#define VERSION_ONE                   // Enable version ONE features
                                        // NOTE: It should be passed to compilation
//#define COMMAND_LINE_ONLY             // Compile tool oly for command line usage

#define TOOL_VERSION_TEXT    "2.0"      // Tool version string

// Float random number generation
#define frnd(range) ((float)GetRandomValue(0, 10000)/10000.0f*range)

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
bool __stdcall FreeConsole(void);       // Close console from code (kernel32.lib)
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// Wave parameters type (96 bytes)
typedef struct WaveParams {

    // Random seed used to generate the wave
    int randSeed;

    // Wave type (square, sawtooth, sine, noise)
    int waveTypeValue;

    // Wave envelope parameters
    float attackTimeValue;
    float sustainTimeValue;
    float sustainPunchValue;
    float decayTimeValue;

    // Frequency parameters
    float startFrequencyValue;
    float minFrequencyValue;
    float slideValue;
    float deltaSlideValue;
    float vibratoDepthValue;
    float vibratoSpeedValue;
    //float vibratoPhaseDelayValue;

    // Tone change parameters
    float changeAmountValue;
    float changeSpeedValue;

    // Square wave parameters
    float squareDutyValue;
    float dutySweepValue;

    // Repeat parameters
    float repeatSpeedValue;

    // Phaser parameters
    float phaserOffsetValue;
    float phaserSweepValue;

    // Filter parameters
    float lpfCutoffValue;
    float lpfCutoffSweepValue;
    float lpfResonanceValue;
    float hpfCutoffValue;
    float hpfCutoffSweepValue;

} WaveParams;

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------

// Volume parameters
static float volumeValue = 0.6f;    // Volume

// Export WAV variables
static int wavSampleSize = 16;      // Wave sample size in bits (bitrate)
static int wavSampleRate = 44100;   // Wave sample rate (frequency)

// Wave parameters
static WaveParams params;           // Stores wave parameters for generation
static bool regenerate = false;     // Wave regeneration required

#if defined(VERSION_ONE) && !defined(COMMAND_LINE_ONLY)
// raygui color palette: Light
static const int paletteStyleLight[14] = {
    0xf5f5f5ff,     // DEFAULT_BACKGROUND_COLOR
    0x90abb5ff,     // DEFAULT_LINES_COLOR
    0x838383ff,     // DEFAULT_BORDER_COLOR_NORMAL
    0xc9c9c9ff,     // DEFAULT_BASE_COLOR_NORMAL
    0x686868ff,     // DEFAULT_TEXT_COLOR_NORMAL
    0x5bb2d9ff,     // DEFAULT_BORDER_COLOR_FOCUSED
    0xc9effeff,     // DEFAULT_BASE_COLOR_FOCUSED
    0x6c9bbcff,     // DEFAULT_TEXT_COLOR_FOCUSED
    0x0492c7ff,     // DEFAULT_BORDER_COLOR_PRESSED
    0x97e8ffff,     // DEFAULT_BASE_COLOR_PRESSED
    0x368bafff,     // DEFAULT_TEXT_COLOR_PRESSED
    0xb5c1c2ff,     // DEFAULT_BORDER_COLOR_DISABLED
    0xe6e9e9ff,     // DEFAULT_BASE_COLOR_DISABLED
    0xaeb7b8ff,     // DEFAULT_TEXT_COLOR_DISABLED
};

// raygui color palette: Dark
static const int paletteStyleDark[14] = {
    0x2b3a3aff,     // DEFAULT_BACKGROUND_COLOR
    0x638465ff,     // DEFAULT_LINES_COLOR
    0x60827dff,     // DEFAULT_BORDER_COLOR_NORMAL
    0x2c3334ff,     // DEFAULT_BASE_COLOR_NORMAL
    0x82a29fff,     // DEFAULT_TEXT_COLOR_NORMAL
    0x5f9aa8ff,     // DEFAULT_BORDER_COLOR_FOCUSED
    0x334e57ff,     // DEFAULT_BASE_COLOR_FOCUSED
    0x6aa9b8ff,     // DEFAULT_TEXT_COLOR_FOCUSED
    0xa9cb8dff,     // DEFAULT_BORDER_COLOR_PRESSED
    0x3b6357ff,     // DEFAULT_BASE_COLOR_PRESSED
    0x97af81ff,     // DEFAULT_TEXT_COLOR_PRESSED
    0x5b6462ff,     // DEFAULT_BORDER_COLOR_DISABLED
    0x2c3334ff,     // DEFAULT_BASE_COLOR_DISABLED
    0x666b69ff,     // DEFAULT_TEXT_COLOR_DISABLED
};

// raygui color palette: Candy
static const int paletteStyleCandy[14] = {
    0xfff5e1ff,     // DEFAULT_BACKGROUND_COLOR
    0xd77575ff,     // DEFAULT_LINES_COLOR
    0xe58b68ff,     // DEFAULT_BORDER_COLOR_NORMAL
    0xfeda96ff,     // DEFAULT_BASE_COLOR_NORMAL
    0xe59b5fff,     // DEFAULT_TEXT_COLOR_NORMAL
    0xee813fff,     // DEFAULT_BORDER_COLOR_FOCUSED
    0xfcd85bff,     // DEFAULT_BASE_COLOR_FOCUSED
    0xf49641ff,     // DEFAULT_TEXT_COLOR_FOCUSED
    0xb34848ff,     // DEFAULT_BORDER_COLOR_PRESSED
    0xeb7272ff,     // DEFAULT_BASE_COLOR_PRESSED
    0xbd4a4aff,     // DEFAULT_TEXT_COLOR_PRESSED
    0x94795dff,     // DEFAULT_BORDER_COLOR_DISABLED
    0xc2a37aff,     // DEFAULT_BASE_COLOR_DISABLED
    0x9c8369ff      // DEFAULT_TEXT_COLOR_DISABLED
};
#endif

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
#if defined(VERSION_ONE) || defined(COMMAND_LINE_ONLY)
static void ShowCommandLineInfo(void);                      // Show command line usage info
static void ProcessCommandLine(int argc, char *argv[]);     // Process command line input
#endif

// Load/Save/Export data functions
static WaveParams LoadWaveParams(const char *fileName);                 // Load wave parameters from file
static void SaveWaveParams(WaveParams params, const char *fileName);    // Save wave parameters to file
static void ResetWaveParams(WaveParams *params);                        // Reset wave parameters
static Wave GenerateWave(WaveParams params);                            // Generate wave data from parameters

static void DialogLoadSound(void);          // Show dialog: load sound parameters file
static void DialogSaveSound(void);          // Show dialog: save sound parameters file
static void DialogExportWave(Wave wave);    // Show dialog: export current sound as .wav

// Sound generation functions
static void GenPickupCoin(void);            // Generate sound: Pickup/Coin
static void GenLaserShoot(void);            // Generate sound: Laser shoot
static void GenExplosion(void);             // Generate sound: Explosion
static void GenPowerup(void);               // Generate sound: Powerup
static void GenHitHurt(void);               // Generate sound: Hit/Hurt
static void GenJump(void);                  // Generate sound: Jump
static void GenBlipSelect(void);            // Generate sound: Blip/Select
static void GenRandomize(void);             // Generate random sound
static void GenMutate(void);                // Mutate current sound

#if !defined(COMMAND_LINE_ONLY)
// Auxiliar functions
static void DrawWave(Wave *wave, Rectangle bounds, Color color);    // Draw wave data using lines
#endif

#if defined(VERSION_ONE) || defined(COMMAND_LINE_ONLY)
static void WaitTime(int ms);               // Simple time wait in milliseconds
static void PlayWaveCLI(Wave wave);         // Play provided wave through CLI

#if !defined(_WIN32)
static int kbhit(void);                         // Check if a key has been pressed
static char getch(void) { return getchar(); }   // Get pressed character
#endif
#endif  // defined(COMMAND_LINE_ONLY)

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    char inFileName[256] = { 0 };       // Input file name (required in case of drag & drop over executable)

    // Command-line usage mode
    //--------------------------------------------------------------------------------------
    if (argc > 1)
    {
        if ((argc == 2) &&
            (strcmp(argv[1], "-h") != 0) &&
            (strcmp(argv[1], "--help") != 0))       // One argument (file dropped over executable?)
        {
            if (IsFileExtension(argv[1], ".rfx") ||
                IsFileExtension(argv[1], ".sfs"))
            {
                strcpy(inFileName, argv[1]);        // Read input filename to open with gui interface
            }
        }
#if defined(VERSION_ONE)
        else
        {
            ProcessCommandLine(argc, argv);
            return 0;
        }
#endif      // VERSION_ONE
    }

#if !defined(COMMAND_LINE_ONLY)
#if (defined(VERSION_ONE) && (defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)))
    // WARNING (Windows): If program is compiled as Window application (instead of console),
    // no console is available to show output info... solution is compiling a console application
    // and closing console (FreeConsole()) when changing to GUI interface
    FreeConsole();
#endif

    // GUI usage mode - Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 496;
    const int screenHeight = 500;

    SetTraceLog(0);                             // Disable trace log messsages
    //SetConfigFlags(FLAG_MSAA_4X_HINT);        // Window configuration flags
    InitWindow(screenWidth, screenHeight, FormatText("rFXGen v%s - A simple and easy-to-use fx sounds generator", TOOL_VERSION_TEXT));
    //SetExitKey(0);

    InitAudioDevice();

    Rectangle waveRec = { 10, 421, 475, 50 };       // Wave drawing rectangle box
    Vector2 paramsAnchor = { 115, 40 };             // Parameters box anchor point

    // GUI controls data
    //----------------------------------------------------------------------------------------
    bool playOnChangeValue = true;

    const char *comboxSampleRateText[2] = { "22050 Hz", "44100 Hz" };
    const char *comboxSampleSizeText[3] = { "8 bit", "16 bit", "32 bit" };
    int comboxSampleRateValue = 1;
    int comboxSampleSizeValue = 1;

    bool screenSizeToggle = false;

    const char *tgroupWaveTypeText[4] = { "Square", "Sawtooth", "Sinewave", "Noise" };

    GuiWindowAboutState windowAboutState = InitGuiWindowAbout();
    //----------------------------------------------------------------------------------------

    Wave wave;
    Sound sound;

    // Check if a wave parameters file has been provided on command line
    if (inFileName[0] != '\0')
    {
        params = LoadWaveParams(inFileName);    // Load wave parameters from .rfx
        wave = GenerateWave(params);            // Generate wave from parameters
        sound = LoadSoundFromWave(wave);        // Load sound from new wave
        PlaySound(sound);                       // Play generated sound
    }
    else
    {
        // Reset generation parameters
        // NOTE: Random seed for generation is set
        ResetWaveParams(&params);

        // Default wave values
        wave.sampleRate = 44100;
        wave.sampleSize = 32;       // 32 bit -> float
        wave.channels = 1;
        wave.sampleCount = 10*wave.sampleRate*wave.channels;    // Max sampleCount for 10 seconds
        wave.data = malloc(wave.sampleCount*wave.channels*wave.sampleSize/8);

        sound = LoadSoundFromWave(wave);
    }

    // Set default sound volume
    SetSoundVolume(sound, volumeValue);

#define RENDER_WAVE_TO_TEXTURE
#if defined(RENDER_WAVE_TO_TEXTURE)
    // To avoid enabling MSXAAx4, we will render wave to a texture x2
    RenderTexture2D waveTarget = LoadRenderTexture(waveRec.width*2, waveRec.height*2);
#endif

    // Render texture to draw full screen, enables screen scaling
    // NOTE: If screen is scaled, mouse input should be scaled proportionally
    RenderTexture2D screenTarget = LoadRenderTexture(512, 512);
    SetTextureFilter(screenTarget.texture, FILTER_POINT);

    SetTargetFPS(60);
    //----------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Dropped files logic
        //----------------------------------------------------------------------------------
        if (IsFileDropped())
        {
            int dropsCount = 0;
            char **droppedFiles = GetDroppedFiles(&dropsCount);

            // Support loading .rfx or .sfs files (wave parameters)
            if (IsFileExtension(droppedFiles[0], ".rfx") ||
                IsFileExtension(droppedFiles[0], ".sfs"))
            {
                params = LoadWaveParams(droppedFiles[0]);
                regenerate = true;

                //SetWindowTitle(FormatText("rFXGen v%s - %s", TOOL_VERSION_TEXT, GetFileName(droppedFiles[0])));
            }

            ClearDroppedFiles();
        }
        //----------------------------------------------------------------------------------

        // Keyboard shortcuts
        //------------------------------------------------------------------------------------
        if (IsKeyPressed(KEY_SPACE)) PlaySound(sound);                                  // Play current sound
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S)) DialogSaveSound();      // Show dialog: save sound (.rfx)
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_O)) DialogLoadSound();      // Show dialog: load sound (.rfx, .sfs)
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_E)) DialogExportWave(wave); // Show dialog: export wave (.wav)
#if defined(VERSION_ONE)
        if (IsKeyPressed(KEY_ONE)) GuiLoadStylePalette(paletteStyleLight);              // Load style color palette: light
        if (IsKeyPressed(KEY_TWO)) GuiLoadStylePalette(paletteStyleDark);               // Load style color palette: dark
        if (IsKeyPressed(KEY_THREE)) GuiLoadStylePalette(paletteStyleCandy);            // Load style color palette: candy
#endif
        if (IsKeyPressed(KEY_F1)) windowAboutState.active = !windowAboutState.active;
        //----------------------------------------------------------------------------------

        // Basic program flow logic
        //----------------------------------------------------------------------------------
        // Change window size to x2
        if (screenSizeToggle)
        {
            if (GetScreenWidth() < screenWidth*2)
            {
                SetWindowSize(screenWidth*2, screenHeight*2);
                SetMouseScale(0.5f);
            }
        }
        else
        {
            if (screenWidth*2 >= GetScreenWidth())
            {
                SetWindowSize(screenWidth, screenHeight);
                SetMouseScale(1.0f);
            }
        }

        // Consider two possible cases to regenerate wave and update sound:
        // CASE1: regenerate flag is true (set by sound buttons functions)
        // CASE2: Mouse is moving sliders and mouse is released (checks against all sliders box - a bit crappy solution...)
        if (regenerate || ((CheckCollisionPointRec(GetMousePosition(), (Rectangle){ 243, 48, 102, 362 })) && (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))))
        {
            UnloadWave(wave);
            wave = GenerateWave(params);        // Generate new wave from parameters

            UnloadSound(sound);
            sound = LoadSoundFromWave(wave);    // Reload sound from new wave
            //UpdateSound(sound, wave.data, wave.sampleCount);    // Update sound buffer with new data --> CRASHES RANDOMLY!

            if (regenerate || playOnChangeValue) PlaySound(sound);

            regenerate = false;
        }

        // Check gui combo box selected options
        if (comboxSampleRateValue == 0) wavSampleRate = 22050;
        else if (comboxSampleRateValue == 1) wavSampleRate = 44100;

        if (comboxSampleSizeValue == 0) wavSampleSize = 8;
        else if (comboxSampleSizeValue == 1) wavSampleSize = 16;
        else if (comboxSampleSizeValue == 2) wavSampleSize = 32;
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(GetColor(GuiGetStyleProperty(DEFAULT_BACKGROUND_COLOR)));

#if defined(RENDER_WAVE_TO_TEXTURE)
            BeginTextureMode(waveTarget);
                DrawWave(&wave, (Rectangle){ 0, 0, waveTarget.texture.width, waveTarget.texture.height }, GetColor(style[DEFAULT_TEXT_COLOR_PRESSED]));
            EndTextureMode();
#endif
            // Render all screen to a texture (for scaling)
            BeginTextureMode(screenTarget);

            DrawText("rFXGen", 29, 19, 20, GetColor(style[DEFAULT_TEXT_COLOR_PRESSED]));
            GuiLabel((Rectangle){ 89, 14, 10, 10 }, FormatText("v%s", TOOL_VERSION_TEXT));

            // Parameters group boxes
            GuiGroupBox((Rectangle){ paramsAnchor.x + 0, paramsAnchor.y + 0, 264, 71 }, "");
            GuiGroupBox((Rectangle){ paramsAnchor.x + 0, paramsAnchor.y + 70, 264, 96 }, "");
            GuiGroupBox((Rectangle){ paramsAnchor.x + 0, paramsAnchor.y + 165, 264, 36 }, "");
            GuiGroupBox((Rectangle){ paramsAnchor.x + 0, paramsAnchor.y + 200, 264, 36 }, "");
            GuiGroupBox((Rectangle){ paramsAnchor.x + 0, paramsAnchor.y + 235, 264, 21 }, "");
            GuiGroupBox((Rectangle){ paramsAnchor.x + 0, paramsAnchor.y + 255, 264, 36 }, "");
            GuiGroupBox((Rectangle){ paramsAnchor.x + 0, paramsAnchor.y + 290, 264, 85 }, "");

            // Parameters sliders
            //--------------------------------------------------------------------------------
            params.attackTimeValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 10, 100, 10 },  params.attackTimeValue, 0, 1, "ATTACK TIME", true);
            params.sustainTimeValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 25, 100, 10 },  params.sustainTimeValue, 0, 1, "SUSTAIN TIME", true);
            params.sustainPunchValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 40, 100, 10 },  params.sustainPunchValue, 0, 1, "SUSTAIN PUNCH", true);
            params.decayTimeValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 55, 100, 10 },  params.decayTimeValue, 0, 1, "DECAY TIME", true);
            params.startFrequencyValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 75, 100, 10 },  params.startFrequencyValue, 0, 1, "START FREQUENCY", true);
            params.minFrequencyValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 90, 100, 10 },  params.minFrequencyValue, 0, 1, "MIN FREQUENCY", true);
            params.slideValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 105, 100, 10 },  params.slideValue, -1, 1, "SLIDE", true);
            params.deltaSlideValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 120, 100, 10 },  params.deltaSlideValue, -1, 1, "DELTA SLIDE", true);
            params.vibratoDepthValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 135, 100, 10 },  params.vibratoDepthValue, 0, 1, "VIBRATO DEPTH", true);
            params.vibratoSpeedValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 150, 100, 10 },  params.vibratoSpeedValue, 0, 1, "VIBRATO SPEED", true);
            params.changeAmountValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 170, 100, 10 },  params.changeAmountValue, -1, 1, "CHANGE AMOUNT", true);
            params.changeSpeedValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 185, 100, 10 },  params.changeSpeedValue, 0, 1, "CHANGE SPEED", true);
            params.squareDutyValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 205, 100, 10 },  params.squareDutyValue, 0, 1, "SQUARE DUTY", true);
            params.dutySweepValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 220, 100, 10 },  params.dutySweepValue, -1, 1, "DUTY SWEEP", true);
            params.repeatSpeedValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 240, 100, 10 },  params.repeatSpeedValue, 0, 1, "REPEAT SPEED", true);
            params.phaserOffsetValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 260, 100, 10 },  params.phaserOffsetValue, -1, 1, "PHASER OFFSET", true);
            params.phaserSweepValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 275, 100, 10 },  params.phaserSweepValue, -1, 1, "PHASER SWEEP", true);
            params.lpfCutoffValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 295, 100, 10 },  params.lpfCutoffValue, 0, 1, "LPF CUTOFF", true);
            params.lpfCutoffSweepValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 310, 100, 10 },  params.lpfCutoffSweepValue, -1, 1, "LPF CUTOFF SWEEP", true);
            params.lpfResonanceValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 325, 100, 10 },  params.lpfResonanceValue, 0, 1, "LPF RESONANCE", true);
            params.hpfCutoffValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 340, 100, 10 },  params.hpfCutoffValue, 0, 1, "HPF CUTOFF", true);
            params.hpfCutoffSweepValue = GuiSliderBarEx((Rectangle){ paramsAnchor.x + 125, paramsAnchor.y + 355, 100, 10 },  params.hpfCutoffSweepValue, -1, 1, "HPF CUTOFF SWEEP", true);
            //--------------------------------------------------------------------------------

            // Buttons
            //--------------------------------------------------------------------------------
            if (GuiButton((Rectangle){ 10, 45, 95, 20 },  "Pickup/Coin")) GenPickupCoin();
            if (GuiButton((Rectangle){ 10, 70, 95, 20 },  "Laser/Shoot")) GenLaserShoot();
            if (GuiButton((Rectangle){ 10, 95, 95, 20 },  "Explosion")) GenExplosion();
            if (GuiButton((Rectangle){ 10, 120, 95, 20 }, "Powerup")) GenPowerup();
            if (GuiButton((Rectangle){ 10, 145, 95, 20 }, "Hit/Hurt")) GenHitHurt();
            if (GuiButton((Rectangle){ 10, 170, 95, 20 }, "Jump")) GenJump();
            if (GuiButton((Rectangle){ 10, 195, 95, 20 }, "Blip/Select")) GenBlipSelect();
            if (GuiButton((Rectangle){ 10, 364, 95, 20 }, "Mutate")) GenMutate();
            if (GuiButton((Rectangle){ 10, 389, 95, 20 }, "Randomize")) GenRandomize();
            if (GuiButton((Rectangle){ 390, 81, 95, 20 }, "Play Sound")) PlaySound(sound);
            if (GuiButton((Rectangle){ 390, 283, 95, 20 }, "Load Sound")) DialogLoadSound();
            if (GuiButton((Rectangle){ 390, 307, 95, 20 }, "Save Sound")) DialogSaveSound();
            if (GuiButton((Rectangle){ 390, 389, 95, 20 }, "Export .Wav")) DialogExportWave(wave);
            //--------------------------------------------------------------------------------

            // Right side controls
            //--------------------------------------------------------------------------------
            float previousVolumeValue = volumeValue;
            volumeValue = GuiSliderBar((Rectangle){ 390, 65, 95, 10 }, volumeValue, 0, 1);
            if (volumeValue != previousVolumeValue) SetSoundVolume(sound, volumeValue);
            if (volumeValue < 1.0f) GuiLabel((Rectangle){ 390, 49, 10, 10 }, FormatText("VOLUME:      %02i %%", (int)(volumeValue*100.0f)));
            else GuiLabel((Rectangle){ 390, 49, 10, 10 }, FormatText("VOLUME:     %02i %%", (int)(volumeValue*100.0f)));

            screenSizeToggle = GuiToggleButton((Rectangle){ 390, 15, 95, 20 }, "Screen Size x2", screenSizeToggle);
            playOnChangeValue = GuiCheckBoxEx((Rectangle){ 390, 115, 10, 10 }, playOnChangeValue, "Play on change");
            comboxSampleRateValue = GuiComboBox((Rectangle){ 390, 340, 95, 20 }, comboxSampleRateText, 2, comboxSampleRateValue);
            comboxSampleSizeValue = GuiComboBox((Rectangle){ 390, 364, 95, 20 }, comboxSampleSizeText, 3, comboxSampleSizeValue);

            int previousWaveTypeValue = params.waveTypeValue;
            params.waveTypeValue = GuiToggleGroup((Rectangle){ 115, 15, 260, 20 }, tgroupWaveTypeText, 4, params.waveTypeValue);
            if (params.waveTypeValue != previousWaveTypeValue) regenerate = true;

            // Draw status bar
            GuiStatusBar((Rectangle){ 0, screenHeight - 20, 206, 20 }, FormatText("SOUND INFO: Num samples: %i", wave.sampleCount), 14);
            GuiStatusBar((Rectangle){ 205, screenHeight - 20, 123, 20 }, FormatText("Duration: %i ms", wave.sampleCount*1000/(wave.sampleRate*wave.channels)), 10);
            GuiStatusBar((Rectangle){ 327, screenHeight - 20, screenWidth - 327, 20 }, FormatText("Wave size: %i bytes", wave.sampleCount*wavSampleSize/8), 10);
            //--------------------------------------------------------------------------------

            // Advertising (links, logos...) --> Moved to Window About
            //--------------------------------------------------------------------------------
            /*
            GuiLabel((Rectangle){ 16, 235, 10, 10 }, "based on sfxr by");
            GuiLabel((Rectangle){ 13, 248, 10, 10 }, "Tomas Pettersson");

            GuiLine((Rectangle){ 10, 268, 95, 1 }, 1);
            GuiLine((Rectangle){ 10, 225, 95, 1 }, 1);
            GuiLine((Rectangle){ 10, 358, 95, 1 }, 1);
            GuiLine((Rectangle){ 390, 108, 95, 1 }, 1);
            GuiLine((Rectangle){ 390, 277, 95, 1 }, 1);
            GuiLine((Rectangle){ 390, 334, 95, 1 }, 1);

            if (GuiLabelButton((Rectangle){ 18, 270, MeasureText("www.github.com/\nraysan5/raygui", 10)/2, 24 }, "www.github.com/\nraysan5/raygui")) OpenLinkURL("https://www.github.com/raysan5/raygui");
            if (GuiLabelButton((Rectangle){ 18, 315, MeasureText("www.github.com/\nraysan5/raylib", 10)/2, 24 }, "www.github.com/\nraysan5/raylib")) OpenLinkURL("https://www.github.com/raysan5/raylib");

            GuiLabel((Rectangle){ 390, 140, 10, 10 }, "powered by");
            DrawRectangle(390, 153, 92, 92, BLACK);
            DrawRectangle(396, 159, 80, 80, RAYWHITE);
            DrawText("raylib", 415, 214, 20, BLACK);

            if (GuiLabelButton((Rectangle){ 405, 250, MeasureText("www.raylib.com", 10), 10 }, "www.raylib.com")) OpenLinkURL("http://www.raylib.com");
            */
            //--------------------------------------------------------------------------------

            // Wave form
            //--------------------------------------------------------------------------------
        #if defined(RENDER_WAVE_TO_TEXTURE)
            DrawTextureEx(waveTarget.texture, (Vector2){ waveRec.x, waveRec.y }, 0.0f, 0.5f, WHITE);
        #else
            DrawWave(&wave, waveRec, GetColor(GuiGetStyleProperty(DEFAULT_LINES_COLOR)));
        #endif

            // TODO: Draw playing progress rectangle

            DrawRectangle(waveRec.x, waveRec.y + waveRec.height/2, waveRec.width, 1, Fade(GetColor(style[DEFAULT_TEXT_COLOR_FOCUSED]), 0.6f));
            DrawRectangleLines(waveRec.x, waveRec.y, waveRec.width, waveRec.height, GetColor(GuiGetStyleProperty(DEFAULT_LINES_COLOR)));
            //--------------------------------------------------------------------------------

            GuiWindowAbout(&windowAboutState);      // GUI About window

            EndTextureMode();

            if (screenSizeToggle) DrawTexturePro(screenTarget.texture, (Rectangle){ 0, 0, screenTarget.texture.width, -screenTarget.texture.height }, (Rectangle){ 0, 0, screenTarget.texture.width*2, screenTarget.texture.height*2 }, (Vector2){ 0, 0 }, 0.0f, WHITE);
            else DrawTextureRec(screenTarget.texture, (Rectangle){ 0, 0, screenTarget.texture.width, -screenTarget.texture.height }, (Vector2){ 0, 0 }, WHITE);

        EndDrawing();
        //------------------------------------------------------------------------------------
    }

    // De-Initialization
    //----------------------------------------------------------------------------------------
    UnloadSound(sound);
    UnloadWave(wave);

    UnloadRenderTexture(screenTarget);
#if defined(RENDER_WAVE_TO_TEXTURE)
    UnloadRenderTexture(waveTarget);
#endif

    CloseAudioDevice();
    CloseWindow();          // Close window and OpenGL context
    //----------------------------------------------------------------------------------------

#endif  //!defined(COMMAND_LINE_ONLY)

    return 0;
}

//--------------------------------------------------------------------------------------------
// Module Functions Definitions (local)
//--------------------------------------------------------------------------------------------

#if defined(VERSION_ONE) || defined(COMMAND_LINE_ONLY)
// Show command line usage info
static void ShowCommandLineInfo(void)
{
    printf("\n//////////////////////////////////////////////////////////////////////////////////\n");
    printf("//                                                                              //\n");
    printf("// rFXGen v%s - A simple and easy-to-use fx sounds generator                   //\n", TOOL_VERSION_TEXT);
    printf("// powered by raylib v2.0 (www.raylib.com) and raygui v2.0                      //\n");
    printf("// more info and bugs-report: github.com/raysan5/rfxgen                         //\n");
    printf("//                                                                              //\n");
    printf("// Copyright (c) 2016-2018 raylib technologies (@raylibtech)                    //\n");
    printf("//                                                                              //\n");
    printf("//////////////////////////////////////////////////////////////////////////////////\n\n");

    printf("USAGE:\n\n");
    printf("    > rfxgen [--help] --input <filename.ext> [--output <filename.ext>]\n");
    printf("             [--format <sample_rate> <sample_size> <channels>] [--play <filename.ext>]\n");

    printf("\nOPTIONS:\n\n");
    printf("    -h, --help                      : Show tool version and command line usage help\n");
    printf("    -i, --input <filename.ext>      : Define input file.\n");
    printf("                                      Supported extensions: .rfx, .sfs, .wav\n");
    printf("    -o, --output <filename.ext>     : Define output file.\n");
    printf("                                      Supported extensions: .wav, .h\n");
    printf("                                      NOTE: If not specified, defaults to: output.wav\n\n");
    printf("    -f, --format <sample_rate>,<sample_size>,<channels>\n");
    printf("                                    : Define output wave format. Comma separated values.\n");
    printf("                                      Supported values:\n");
    printf("                                          Sample rate:      22050, 44100\n");
    printf("                                          Sample size:      8, 16, 32\n");
    printf("                                          Channels:         1 (mono), 2 (stereo)\n");
    printf("                                      NOTE: If not specified, defaults to: 44100, 16, 1\n\n");
    printf("    -p, --play <filename.ext>       : Play provided sound.\n");
    printf("                                      Supported extensions: .wav, .ogg, .flac, .mp3\n");

    printf("\nEXAMPLES:\n\n");
    printf("    > rfxgen --input sound.rfx --output jump.wav\n");
    printf("        Process <sound.rfx> to generate <sound.wav> at 44100 Hz, 32 bit, Mono\n\n");
    printf("    > rfxgen --input sound.rfx --output jump.wav --format 22050 16 2\n");
    printf("        Process <sound.rfx> to generate <jump.wav> at 22050 Hz, 16 bit, Stereo\n\n");
    printf("    > rfxgen --input sound.rfx --play output.wav\n");
    printf("        Process <sound.rfx> to generate <output.wav> and play <output.wav>\n\n");
    printf("    > rfxgen --input sound.wav --output jump.wav --format 22050,8,1 --play jump.wav\n");
    printf("        Process <sound.wav> to generate <jump.wav> at 22050 Hz, 8 bit, Stereo.\n");
    printf("        Plays generated sound <jump.wav>.\n");
}

// Process command line input
static void ProcessCommandLine(int argc, char *argv[])
{
    // CLI required variables
    bool showUsageInfo = false;     // Toggle command line usage info

    char inFileName[256] = { 0 };   // Input file name
    char outFileName[256] = { 0 };  // Output file name
    char playFileName[256] = { 0 }; // Play file name

    int sampleRate = 44100;         // Default conversion sample rate
    int sampleSize = 16;            // Default conversion sample size
    int channels = 1;               // Default conversion channels number

    // Process command line arguments
    for (int i = 1; i < argc; i++)
    {
        if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0))
        {
            showUsageInfo = true;
        }
        else if ((strcmp(argv[i], "-i") == 0) || (strcmp(argv[i], "--input") == 0))
        {
            // Check for valid argumment and valid file extension
            if (((i + 1) < argc) && (argv[i + 1][0] != '-') &&
                (IsFileExtension(argv[i + 1], ".rfx") ||
                 IsFileExtension(argv[i + 1], ".sfs") ||
                 IsFileExtension(argv[i + 1], ".wav")))
            {
                strcpy(inFileName, argv[i + 1]);    // Read input filename
                i++;
            }
            else printf("WARNING: Input file extension not recognized\n");
        }
        else if ((strcmp(argv[i], "-o") == 0) || (strcmp(argv[i], "--output") == 0))
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-') &&
                (IsFileExtension(argv[i + 1], ".wav") ||
                 IsFileExtension(argv[i + 1], ".h")))
            {
                strcpy(outFileName, argv[i + 1]);   // Read output filename
                i++;
            }
            else printf("WARNING: Output file extension not recognized\n");
        }
        else if ((strcmp(argv[i], "-f") == 0) || (strcmp(argv[i], "--format") == 0))
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                int numValues = 0;
                char **values = SplitText(argv[i + 1], ',', &numValues);

                if (numValues != 3) printf("WARNING: Incorrect number of format values\n");
                else
                {
                    // Read values text and convert to integer values
                    sampleRate = atoi(values[0]);
                    sampleSize = atoi(values[1]);
                    channels = atoi(values[2]);

                    // Verify retrieved values are valid
                    if ((sampleRate != 44100) && (sampleRate != 22050))
                    {
                        printf("WARNING: Sample rate not supported. Default: 44100 Hz\n");
                        sampleRate = 44100;
                    }

                    if ((sampleSize != 8) && (sampleSize != 16) && (sampleSize != 32))
                    {
                        printf("WARNING: Sample size not supported. Default: 16 bit\n");
                        sampleSize = 16;
                    }

                    if ((channels != 1) && (channels != 2))
                    {
                        printf("WARNING: Channels number not supported. Default: 1 (mono)\n");
                        channels = 1;
                    }
                }

                for (int i = 0; i < numValues; i++) free(values[i]);
                if (values != NULL) free(values);
            }
            else printf("WARNING: Format parameters provided not valid\n");
        }
        else if ((strcmp(argv[i], "-p") == 0) || (strcmp(argv[i], "--play") == 0))
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-') &&
                (IsFileExtension(argv[i + 1], ".wav") ||
                 IsFileExtension(argv[i + 1], ".ogg") ||
                 IsFileExtension(argv[i + 1], ".flac") ||
                 IsFileExtension(argv[i + 1], ".mp3")))
            {
                strcpy(playFileName, argv[i + 1]);   // Read filename to play
                i++;
            }
            else printf("WARNING: Play file extension not supported\n");
        }
    }

    // Process input file if provided
    if (inFileName[0] != '\0')
    {
        if (outFileName[0] == '\0') strcpy(outFileName, "output.wav");  // Set a default name for output in case not provided

        printf("\nInput file:       %s", inFileName);
        printf("\nOutput file:      %s", outFileName);
        printf("\nOutput format:    %i Hz, %i bits, %s\n\n", sampleRate, sampleSize, (channels == 1) ? "Mono" : "Stereo");

        Wave wave = { 0 };

        if (IsFileExtension(inFileName, ".rfx") || IsFileExtension(inFileName, ".sfs"))
        {
            params = LoadWaveParams(inFileName);
            wave = GenerateWave(params);
        }
        else if (IsFileExtension(inFileName, ".wav")) wave = LoadWave(inFileName);

        // Format wave data to desired sampleRate, sampleSize and channels
        WaveFormat(&wave, sampleRate, sampleSize, channels);

        // Export wave data as audio file (.wav) or code file (.h)
        if (IsFileExtension(outFileName, ".wav")) ExportWave(wave, outFileName);
        else if (IsFileExtension(outFileName, ".h")) ExportWaveAsCode(wave, outFileName);

        UnloadWave(wave);
    }

    // Play audio file if provided
    if (playFileName[0] != '\0')
    {
        Wave wave = LoadWave(playFileName);
        PlayWaveCLI(wave);
        UnloadWave(wave);
    }

    if (showUsageInfo) ShowCommandLineInfo();
}
#endif      // VERSION_ONE

//--------------------------------------------------------------------------------------------
// Load/Save/Export functions
//--------------------------------------------------------------------------------------------

// Reset wave parameters
static void ResetWaveParams(WaveParams *params)
{
    // NOTE: Random seed is set to a random value
    params->randSeed = GetRandomValue(0x1, 0xFFFE);
    srand(params->randSeed);

    // Wave type
    params->waveTypeValue = 0;

    // Wave envelope params
    params->attackTimeValue = 0.0f;
    params->sustainTimeValue = 0.3f;
    params->sustainPunchValue = 0.0f;
    params->decayTimeValue = 0.4f;

    // Frequency params
    params->startFrequencyValue = 0.3f;
    params->minFrequencyValue = 0.0f;
    params->slideValue = 0.0f;
    params->deltaSlideValue = 0.0f;
    params->vibratoDepthValue = 0.0f;
    params->vibratoSpeedValue = 0.0f;
    //params->vibratoPhaseDelay = 0.0f;

    // Tone change params
    params->changeAmountValue = 0.0f;
    params->changeSpeedValue = 0.0f;

    // Square wave params
    params->squareDutyValue = 0.0f;
    params->dutySweepValue = 0.0f;

    // Repeat params
    params->repeatSpeedValue = 0.0f;

    // Phaser params
    params->phaserOffsetValue = 0.0f;
    params->phaserSweepValue = 0.0f;

    // Filter params
    params->lpfCutoffValue = 1.0f;
    params->lpfCutoffSweepValue = 0.0f;
    params->lpfResonanceValue = 0.0f;
    params->hpfCutoffValue = 0.0f;
    params->hpfCutoffSweepValue = 0.0f;
}

// Generates new wave from wave parameters
// NOTE: By default wave is generated as 44100Hz, 32bit float, mono
static Wave GenerateWave(WaveParams params)
{
    #define MAX_WAVE_LENGTH_SECONDS  10     // Max length for wave: 10 seconds
    #define WAVE_SAMPLE_RATE      44100     // Default sample rate

    // NOTE: GetRandomValue() is provided by raylib and seed is initialized at InitWindow()
    #define GetRandomFloat(range) ((float)GetRandomValue(0, 10000)/10000.0f*range)

    if (params.randSeed != 0) srand(params.randSeed);   // Initialize seed if required

    // Configuration parameters for generation
    // NOTE: Those parameters are calculated from selected values
    int phase = 0;
    double fperiod = 0.0;
    double fmaxperiod = 0.0;
    double fslide = 0.0;
    double fdslide = 0.0;
    int period = 0;
    float squareDuty = 0.0f;
    float squareSlide = 0.0f;
    int envelopeStage = 0;
    int envelopeTime = 0;
    int envelopeLength[3] = { 0 };
    float envelopeVolume = 0.0f;
    float fphase = 0.0f;
    float fdphase = 0.0f;
    int iphase = 0;
    float phaserBuffer[1024] = { 0.0f };
    int ipp = 0;
    float noiseBuffer[32] = { 0.0f };       // Required for noise wave, depends on random seed!
    float fltp = 0.0f;
    float fltdp = 0.0f;
    float fltw = 0.0f;
    float fltwd = 0.0f;
    float fltdmp = 0.0f;
    float fltphp = 0.0f;
    float flthp = 0.0f;
    float flthpd = 0.0f;
    float vibratoPhase = 0.0f;
    float vibratoSpeed = 0.0f;
    float vibratoAmplitude = 0.0f;
    int repeatTime = 0;
    int repeatLimit = 0;
    int arpeggioTime = 0;
    int arpeggioLimit = 0;
    double arpeggioModulation = 0.0;

    // HACK: Security check to avoid crash (why?)
    if (params.minFrequencyValue > params.startFrequencyValue) params.minFrequencyValue = params.startFrequencyValue;
    if (params.slideValue < params.deltaSlideValue) params.slideValue = params.deltaSlideValue;

    // Reset sample parameters
    //----------------------------------------------------------------------------------------
    fperiod = 100.0/(params.startFrequencyValue*params.startFrequencyValue + 0.001);
    period = (int)fperiod;
    fmaxperiod = 100.0/(params.minFrequencyValue*params.minFrequencyValue + 0.001);
    fslide = 1.0 - pow((double)params.slideValue, 3.0)*0.01;
    fdslide = -pow((double)params.deltaSlideValue, 3.0)*0.000001;
    squareDuty = 0.5f - params.squareDutyValue*0.5f;
    squareSlide = -params.dutySweepValue*0.00005f;

    if (params.changeAmountValue >= 0.0f) arpeggioModulation = 1.0 - pow((double)params.changeAmountValue, 2.0)*0.9;
    else arpeggioModulation = 1.0 + pow((double)params.changeAmountValue, 2.0)*10.0;

    arpeggioLimit = (int)(pow(1.0f - params.changeSpeedValue, 2.0f)*20000 + 32);

    if (params.changeSpeedValue == 1.0f) arpeggioLimit = 0;     // WATCH OUT: float comparison

    // Reset filter parameters
    fltw = pow(params.lpfCutoffValue, 3.0f)*0.1f;
    fltwd = 1.0f + params.lpfCutoffSweepValue*0.0001f;
    fltdmp = 5.0f/(1.0f + pow(params.lpfResonanceValue, 2.0f)*20.0f)*(0.01f + fltw);
    if (fltdmp > 0.8f) fltdmp = 0.8f;
    flthp = pow(params.hpfCutoffValue, 2.0f)*0.1f;
    flthpd = 1.0 + params.hpfCutoffSweepValue*0.0003f;

    // Reset vibrato
    vibratoSpeed = pow(params.vibratoSpeedValue, 2.0f)*0.01f;
    vibratoAmplitude = params.vibratoDepthValue*0.5f;

    // Reset envelope
    envelopeLength[0] = (int)(params.attackTimeValue*params.attackTimeValue*100000.0f);
    envelopeLength[1] = (int)(params.sustainTimeValue*params.sustainTimeValue*100000.0f);
    envelopeLength[2] = (int)(params.decayTimeValue*params.decayTimeValue*100000.0f);

    fphase = pow(params.phaserOffsetValue, 2.0f)*1020.0f;
    if (params.phaserOffsetValue < 0.0f) fphase = -fphase;

    fdphase = pow(params.phaserSweepValue, 2.0f)*1.0f;
    if (params.phaserSweepValue < 0.0f) fdphase = -fdphase;

    iphase = abs((int)fphase);

    for (int i = 0; i < 32; i++) noiseBuffer[i] = GetRandomFloat(2.0f) - 1.0f;      // WATCH OUT: GetRandomFloat()

    repeatLimit = (int)(pow(1.0f - params.repeatSpeedValue, 2.0f)*20000 + 32);

    if (params.repeatSpeedValue == 0.0f) repeatLimit = 0;
    //----------------------------------------------------------------------------------------

    // NOTE: We reserve enough space for up to 10 seconds of wave audio at given sample rate
    // By default we use float size samples, they are converted to desired sample size at the end
    float *buffer = (float *)calloc(MAX_WAVE_LENGTH_SECONDS*WAVE_SAMPLE_RATE, sizeof(float));
    bool generatingSample = true;
    int sampleCount = 0;

    for (int i = 0; i < MAX_WAVE_LENGTH_SECONDS*WAVE_SAMPLE_RATE; i++)
    {
        if (!generatingSample)
        {
            sampleCount = i;
            break;
        }

        // Generate sample using selected parameters
        //------------------------------------------------------------------------------------
        repeatTime++;

        if ((repeatLimit != 0) && (repeatTime >= repeatLimit))
        {
            // Reset sample parameters (only some of them)
            repeatTime = 0;

            fperiod = 100.0/(params.startFrequencyValue*params.startFrequencyValue + 0.001);
            period = (int)fperiod;
            fmaxperiod = 100.0/(params.minFrequencyValue*params.minFrequencyValue + 0.001);
            fslide = 1.0 - pow((double)params.slideValue, 3.0)*0.01;
            fdslide = -pow((double)params.deltaSlideValue, 3.0)*0.000001;
            squareDuty = 0.5f - params.squareDutyValue*0.5f;
            squareSlide = -params.dutySweepValue*0.00005f;

            if (params.changeAmountValue >= 0.0f) arpeggioModulation = 1.0 - pow((double)params.changeAmountValue, 2.0)*0.9;
            else arpeggioModulation = 1.0 + pow((double)params.changeAmountValue, 2.0)*10.0;

            arpeggioTime = 0;
            arpeggioLimit = (int)(pow(1.0f - params.changeSpeedValue, 2.0f)*20000 + 32);

            if (params.changeSpeedValue == 1.0f) arpeggioLimit = 0;     // WATCH OUT: float comparison
        }

        // Frequency envelopes/arpeggios
        arpeggioTime++;

        if ((arpeggioLimit != 0) && (arpeggioTime >= arpeggioLimit))
        {
            arpeggioLimit = 0;
            fperiod *= arpeggioModulation;
        }

        fslide += fdslide;
        fperiod *= fslide;

        if (fperiod > fmaxperiod)
        {
            fperiod = fmaxperiod;

            if (params.minFrequencyValue > 0.0f) generatingSample = false;
        }

        float rfperiod = fperiod;

        if (vibratoAmplitude > 0.0f)
        {
            vibratoPhase += vibratoSpeed;
            rfperiod = fperiod*(1.0 + sinf(vibratoPhase)*vibratoAmplitude);
        }

        period = (int)rfperiod;

        if (period < 8) period=8;

        squareDuty += squareSlide;

        if (squareDuty < 0.0f) squareDuty = 0.0f;
        if (squareDuty > 0.5f) squareDuty = 0.5f;

        // Volume envelope
        envelopeTime++;

        if (envelopeTime > envelopeLength[envelopeStage])
        {
            envelopeTime = 0;
            envelopeStage++;

            if (envelopeStage == 3) generatingSample = false;
        }

        if (envelopeStage == 0) envelopeVolume = (float)envelopeTime/envelopeLength[0];
        if (envelopeStage == 1) envelopeVolume = 1.0f + pow(1.0f - (float)envelopeTime/envelopeLength[1], 1.0f)*2.0f*params.sustainPunchValue;
        if (envelopeStage == 2) envelopeVolume = 1.0f - (float)envelopeTime/envelopeLength[2];

        // Phaser step
        fphase += fdphase;
        iphase = abs((int)fphase);

        if (iphase > 1023) iphase = 1023;

        if (flthpd != 0.0f)     // WATCH OUT!
        {
            flthp *= flthpd;
            if (flthp < 0.00001f) flthp = 0.00001f;
            if (flthp > 0.1f) flthp = 0.1f;
        }

        float ssample = 0.0f;

        #define MAX_SUPERSAMPLING   8

        // Supersampling x8
        for (int si = 0; si < MAX_SUPERSAMPLING; si++)
        {
            float sample = 0.0f;
            phase++;

            if (phase >= period)
            {
                //phase = 0;
                phase %= period;

                if (params.waveTypeValue == 3)
                {
                    for (int i = 0;i < 32; i++) noiseBuffer[i] = GetRandomFloat(2.0f) - 1.0f;   // WATCH OUT: GetRandomFloat()
                }
            }

            // base waveform
            float fp = (float)phase/period;

            switch (params.waveTypeValue)
            {
                case 0: // Square wave
                {
                    if (fp < squareDuty) sample = 0.5f;
                    else sample = -0.5f;

                } break;
                case 1: sample = 1.0f - fp*2; break;    // Sawtooth wave
                case 2: sample = sinf(fp*2*PI); break;  // Sine wave
                case 3: sample = noiseBuffer[phase*32/period]; break; // Noise wave
                default: break;
            }

            // LP filter
            float pp = fltp;
            fltw *= fltwd;

            if (fltw < 0.0f) fltw = 0.0f;
            if (fltw > 0.1f) fltw = 0.1f;

            if (params.lpfCutoffValue != 1.0f)  // WATCH OUT!
            {
                fltdp += (sample-fltp)*fltw;
                fltdp -= fltdp*fltdmp;
            }
            else
            {
                fltp = sample;
                fltdp = 0.0f;
            }

            fltp += fltdp;

            // HP filter
            fltphp += fltp - pp;
            fltphp -= fltphp*flthp;
            sample = fltphp;

            // Phaser
            phaserBuffer[ipp & 1023] = sample;
            sample += phaserBuffer[(ipp - iphase + 1024) & 1023];
            ipp = (ipp + 1) & 1023;

            // Final accumulation and envelope application
            ssample += sample*envelopeVolume;
        }

        #define SAMPLE_SCALE_COEFICIENT 0.2f    // NOTE: Used to scale sample value to [-1..1]

        ssample = (ssample/MAX_SUPERSAMPLING)*SAMPLE_SCALE_COEFICIENT;
        //------------------------------------------------------------------------------------

        // Accumulate samples in the buffer
        if (ssample > 1.0f) ssample = 1.0f;
        if (ssample < -1.0f) ssample = -1.0f;

        buffer[i] = ssample;
    }

    Wave genWave;
    genWave.sampleCount = sampleCount;
    genWave.sampleRate = WAVE_SAMPLE_RATE; // By default 44100 Hz
    genWave.sampleSize = 32;               // By default 32 bit float samples
    genWave.channels = 1;                  // By default 1 channel (mono)

    // NOTE: Wave can be converted to desired format after generation

    genWave.data = calloc(genWave.sampleCount*genWave.channels, genWave.sampleSize/8);
    memcpy(genWave.data, buffer, genWave.sampleCount*genWave.channels*genWave.sampleSize/8);

    free(buffer);

    return genWave;
}

// Load .rfx (rFXGen) or .sfs (sfxr) sound parameters file
static WaveParams LoadWaveParams(const char *fileName)
{
    WaveParams params = { 0 };

    if (IsFileExtension(GetExtension(fileName), ".sfs"))
    {
        FILE *sfsFile = fopen(fileName, "rb");

        if (sfsFile == NULL) return params;

        // Load .sfs sound parameters
        int version = 0;
        fread(&version, 1, sizeof(int), sfsFile);

        if ((version == 100) || (version == 101) || (version == 102))
        {
            fread(&params.waveTypeValue, 1, sizeof(int), sfsFile);

            volumeValue = 0.5f;

            if (version == 102) fread(&volumeValue, 1, sizeof(float), sfsFile);

            fread(&params.startFrequencyValue, 1, sizeof(float), sfsFile);
            fread(&params.minFrequencyValue, 1, sizeof(float), sfsFile);
            fread(&params.slideValue, 1, sizeof(float), sfsFile);

            if (version >= 101) fread(&params.deltaSlideValue, 1, sizeof(float), sfsFile);

            fread(&params.squareDutyValue, 1, sizeof(float), sfsFile);
            fread(&params.dutySweepValue, 1, sizeof(float), sfsFile);

            fread(&params.vibratoDepthValue, 1, sizeof(float), sfsFile);
            fread(&params.vibratoSpeedValue, 1, sizeof(float), sfsFile);

            float vibratoPhaseDelay = 0.0f;
            fread(&vibratoPhaseDelay, 1, sizeof(float), sfsFile); // Not used

            fread(&params.attackTimeValue, 1, sizeof(float), sfsFile);
            fread(&params.sustainTimeValue, 1, sizeof(float), sfsFile);
            fread(&params.decayTimeValue, 1, sizeof(float), sfsFile);
            fread(&params.sustainPunchValue, 1, sizeof(float), sfsFile);

            bool filterOn = false;
            fread(&filterOn, 1, sizeof(bool), sfsFile); // Not used

            fread(&params.lpfResonanceValue, 1, sizeof(float), sfsFile);
            fread(&params.lpfCutoffValue, 1, sizeof(float), sfsFile);
            fread(&params.lpfCutoffSweepValue, 1, sizeof(float), sfsFile);
            fread(&params.hpfCutoffValue, 1, sizeof(float), sfsFile);
            fread(&params.hpfCutoffSweepValue, 1, sizeof(float), sfsFile);

            fread(&params.phaserOffsetValue, 1, sizeof(float), sfsFile);
            fread(&params.phaserSweepValue, 1, sizeof(float), sfsFile);
            fread(&params.repeatSpeedValue, 1, sizeof(float), sfsFile);

            if (version >= 101)
            {
                fread(&params.changeSpeedValue, 1, sizeof(float), sfsFile);
                fread(&params.changeAmountValue, 1, sizeof(float), sfsFile);
            }
        }
        else printf("[%s] SFS file version not supported\n", fileName);

        fclose(sfsFile);
    }
    else if (IsFileExtension(GetExtension(fileName), ".rfx"))
    {
        FILE *rfxFile = fopen(fileName, "rb");

        if (rfxFile != NULL)
        {
            // Read .rfx file header
            unsigned char signature[5];
            fread(signature, 4, sizeof(unsigned char), rfxFile);

            // Check for valid .rfx file (FormatCC)
            if ((signature[0] == 'r') &&
                (signature[1] == 'F') &&
                (signature[2] == 'X') &&
                (signature[3] == ' '))
            {
                int version;
                fread(&version, 1, sizeof(int), rfxFile);

                if (version == 100) printf("[%s] Wrong rFX file version (%i)\n", fileName, version);
                else if (version == 120) 
                {
                    // Load wave generation parameters
                    fread(&params, 1, sizeof(WaveParams), rfxFile);
                }
            }
            else printf("[%s] rFX file does not seem to be valid\n", fileName);

            fclose(rfxFile);
        }
    }

    return params;
}

// Save .rfx sound parameters file
static void SaveWaveParams(WaveParams params, const char *fileName)
{
    /*
    if (IsFileExtension(GetExtension(fileName), ".sfs"))
    {
        FILE *sfsFile = fopen(fileName, "wb");

        if (sfsFile == NULL) return;

        // Save .sfs sound parameters
        int version = 102;
        fwrite(&version, 1, sizeof(int), sfsFile);

        fwrite(&params.waveTypeValue, 1, sizeof(int), sfsFile);

        fwrite(&volumeValue, 1, sizeof(float), sfsFile);

        fwrite(&params.startFrequencyValue, 1, sizeof(float), sfsFile);
        fwrite(&params.minFrequencyValue, 1, sizeof(float), sfsFile);
        fwrite(&params.slideValue, 1, sizeof(float), sfsFile);
        fwrite(&params.deltaSlideValue, 1, sizeof(float), sfsFile);
        fwrite(&params.squareDutyValue, 1, sizeof(float), sfsFile);
        fwrite(&params.dutySweepValue, 1, sizeof(float), sfsFile);

        fwrite(&params.vibratoDepthValue, 1, sizeof(float), sfsFile);
        fwrite(&params.vibratoSpeedValue, 1, sizeof(float), sfsFile);

        float vibratoPhaseDelay = 0.0f;
        fwrite(&vibratoPhaseDelay, 1, sizeof(float), sfsFile); // Not used

        fwrite(&params.attackTimeValue, 1, sizeof(float), sfsFile);
        fwrite(&params.sustainTimeValue, 1, sizeof(float), sfsFile);
        fwrite(&params.decayTimeValue, 1, sizeof(float), sfsFile);
        fwrite(&params.sustainPunchValue, 1, sizeof(float), sfsFile);

        bool filterOn = false;
        fwrite(&filterOn, 1, sizeof(bool), sfsFile); // Not used

        fwrite(&params.lpfResonanceValue, 1, sizeof(float), sfsFile);
        fwrite(&params.lpfCutoffValue, 1, sizeof(float), sfsFile);
        fwrite(&params.lpfCutoffSweepValue, 1, sizeof(float), sfsFile);
        fwrite(&params.hpfCutoffValue, 1, sizeof(float), sfsFile);
        fwrite(&params.hpfCutoffSweepValue, 1, sizeof(float), sfsFile);

        fwrite(&params.phaserOffsetValue, 1, sizeof(float), sfsFile);
        fwrite(&params.phaserSweepValue, 1, sizeof(float), sfsFile);

        fwrite(&params.repeatSpeedValue, 1, sizeof(float), sfsFile);

        fwrite(&params.changeSpeedValue, 1, sizeof(float), sfsFile);
        fwrite(&params.changeAmountValue, 1, sizeof(float), sfsFile);

        fclose(sfsFile);
    }
    else 
    */
    if (IsFileExtension(GetExtension(fileName), ".rfx"))
    {
        #define TOOL_VERSION_TEXT_BINARY     120
        
        // Fx Sound File Structure (.rfx)
        // ------------------------------------------------------
        // Offset | Size  | Type       | Description
        // ------------------------------------------------------
        // 0      | 4     | char       | Signature: "rFX " // ID
        // 4      | 2     | short      | Version: 200
        // 6      | 2     | short      | Data length: 96 bytes
        // 8      | 96    | WaveParams | Wave parameters
        // ------------------------------------------------------

        FILE *rfxFile = fopen(fileName, "wb");

        if (rfxFile != NULL)
        {
            unsigned char signature[5] = "rFX ";
            unsigned int version = TOOL_VERSION_TEXT_BINARY;
            
            // Write .rfx file header
            fwrite(signature, 4, sizeof(unsigned char), rfxFile);
            fwrite(&version, 1, sizeof(unsigned int), rfxFile);

            // Write wave generation parameters
            fwrite(&params, 1, sizeof(WaveParams), rfxFile);

            fclose(rfxFile);
        }
    }
}

// Show dialog: load sound parameters file
static void DialogLoadSound(void)
{
    // Open file dialog
    const char *filters[] = { "*.rfx", "*.sfs" };
    const char *fileName = tinyfd_openFileDialog("Load sound parameters file", "", 2, filters, "Sound Param Files (*.rfx, *.sfs)", 0);

    if (fileName != NULL)
    {
        params = LoadWaveParams(fileName);
        regenerate = true;

        //SetWindowTitle(FormatText("rFXGen v%s - %s", TOOL_VERSION_TEXT, GetFileName(fileName)));
    }
}

// Show dialog: save sound parameters file
static void DialogSaveSound(void)
{
    // Save file dialog
    const char *filters[] = { "*.rfx" };
    const char *fileName = tinyfd_saveFileDialog("Save sound parameters file", "sound.rfx", 1, filters, "Sound Param Files (*.rfx)");

    if (fileName != NULL)
    {
        char outFileName[128] = { 0 };
        strcpy(outFileName, fileName);

        // Check for valid extension and make sure it is
        if ((GetExtension(outFileName) == NULL) || !IsFileExtension(outFileName, ".rfx")) strcat(outFileName, ".rfx\0");

        // Save wave parameters
        SaveWaveParams(params, outFileName);
    }
}

// Show dialog: export current sound as .wav
static void DialogExportWave(Wave wave)
{
    // Save file dialog
    const char *filters[] = { "*.wav" };
    const char *fileName = tinyfd_saveFileDialog("Export wave file", "sound.wav", 1, filters, "Wave File (*.wav)");

    if (fileName != NULL)
    {
        char outFileName[128] = { 0 };
        strcpy(outFileName, fileName);

        // Check for valid extension and make sure it is
        if ((GetExtension(outFileName) == NULL) || !IsFileExtension(outFileName, ".wav")) strcat(outFileName, ".wav\0");

        // Export wave data
        Wave cwave = WaveCopy(wave);
        WaveFormat(&cwave, wavSampleRate, wavSampleSize, 1);    // Before exporting wave data, we format it as desired
        ExportWave(cwave, outFileName);                         // Export wave data to file
        UnloadWave(cwave);
    }
}

//--------------------------------------------------------------------------------------------
// Sound generation functions
//--------------------------------------------------------------------------------------------

// Generate sound: Pickup/Coin
static void GenPickupCoin(void)
{
    ResetWaveParams(&params);

    params.startFrequencyValue = 0.4f + frnd(0.5f);
    params.attackTimeValue = 0.0f;
    params.sustainTimeValue = frnd(0.1f);
    params.decayTimeValue = 0.1f + frnd(0.4f);
    params.sustainPunchValue = 0.3f + frnd(0.3f);

    if (GetRandomValue(0, 1))
    {
        params.changeSpeedValue = 0.5f + frnd(0.2f);
        params.changeAmountValue = 0.2f + frnd(0.4f);
    }

    regenerate = true;
}

// Generate sound: Laser shoot
static void GenLaserShoot(void)
{
    ResetWaveParams(&params);

    params.waveTypeValue = GetRandomValue(0, 2);

    if ((params.waveTypeValue == 2) && GetRandomValue(0, 1)) params.waveTypeValue = GetRandomValue(0, 1);

    params.startFrequencyValue = 0.5f + frnd(0.5f);
    params.minFrequencyValue = params.startFrequencyValue - 0.2f - frnd(0.6f);

    if (params.minFrequencyValue < 0.2f) params.minFrequencyValue = 0.2f;

    params.slideValue = -0.15f - frnd(0.2f);

    if (GetRandomValue(0, 2) == 0)
    {
        params.startFrequencyValue = 0.3f + frnd(0.6f);
        params.minFrequencyValue = frnd(0.1f);
        params.slideValue = -0.35f - frnd(0.3f);
    }

    if (GetRandomValue(0, 1))
    {
        params.squareDutyValue = frnd(0.5f);
        params.dutySweepValue = frnd(0.2f);
    }
    else
    {
        params.squareDutyValue = 0.4f + frnd(0.5f);
        params.dutySweepValue = -frnd(0.7f);
    }

    params.attackTimeValue = 0.0f;
    params.sustainTimeValue = 0.1f + frnd(0.2f);
    params.decayTimeValue = frnd(0.4f);

    if (GetRandomValue(0, 1)) params.sustainPunchValue = frnd(0.3f);

    if (GetRandomValue(0, 2) == 0)
    {
        params.phaserOffsetValue = frnd(0.2f);
        params.phaserSweepValue = -frnd(0.2f);
    }

    if (GetRandomValue(0, 1)) params.hpfCutoffValue = frnd(0.3f);

    regenerate = true;
}

// Generate sound: Explosion
static void GenExplosion(void)
{
    ResetWaveParams(&params);

    params.waveTypeValue = 3;

    if (GetRandomValue(0, 1))
    {
        params.startFrequencyValue = 0.1f + frnd(0.4f);
        params.slideValue = -0.1f + frnd(0.4f);
    }
    else
    {
        params.startFrequencyValue = 0.2f + frnd(0.7f);
        params.slideValue = -0.2f - frnd(0.2f);
    }

    params.startFrequencyValue *= params.startFrequencyValue;

    if (GetRandomValue(0, 4) == 0) params.slideValue = 0.0f;
    if (GetRandomValue(0, 2) == 0) params.repeatSpeedValue = 0.3f + frnd(0.5f);

    params.attackTimeValue = 0.0f;
    params.sustainTimeValue = 0.1f + frnd(0.3f);
    params.decayTimeValue = frnd(0.5f);

    if (GetRandomValue(0, 1) == 0)
    {
        params.phaserOffsetValue = -0.3f + frnd(0.9f);
        params.phaserSweepValue = -frnd(0.3f);
    }

    params.sustainPunchValue = 0.2f + frnd(0.6f);

    if (GetRandomValue(0, 1))
    {
        params.vibratoDepthValue = frnd(0.7f);
        params.vibratoSpeedValue = frnd(0.6f);
    }

    if (GetRandomValue(0, 2) == 0)
    {
        params.changeSpeedValue = 0.6f + frnd(0.3f);
        params.changeAmountValue = 0.8f - frnd(1.6f);
    }

    regenerate = true;
}

// Generate sound: Powerup
static void GenPowerup(void)
{
    ResetWaveParams(&params);

    if (GetRandomValue(0, 1)) params.waveTypeValue = 1;
    else params.squareDutyValue = frnd(0.6f);

    if (GetRandomValue(0, 1))
    {
        params.startFrequencyValue = 0.2f + frnd(0.3f);
        params.slideValue = 0.1f + frnd(0.4f);
        params.repeatSpeedValue = 0.4f + frnd(0.4f);
    }
    else
    {
        params.startFrequencyValue = 0.2f + frnd(0.3f);
        params.slideValue = 0.05f + frnd(0.2f);

        if (GetRandomValue(0, 1))
        {
            params.vibratoDepthValue = frnd(0.7f);
            params.vibratoSpeedValue = frnd(0.6f);
        }
    }

    params.attackTimeValue = 0.0f;
    params.sustainTimeValue = frnd(0.4f);
    params.decayTimeValue = 0.1f + frnd(0.4f);

    regenerate = true;
}

// Generate sound: Hit/Hurt
static void GenHitHurt(void)
{
    ResetWaveParams(&params);

    params.waveTypeValue = GetRandomValue(0, 2);
    if (params.waveTypeValue == 2) params.waveTypeValue = 3;
    if (params.waveTypeValue == 0) params.squareDutyValue = frnd(0.6f);

    params.startFrequencyValue = 0.2f + frnd(0.6f);
    params.slideValue = -0.3f - frnd(0.4f);
    params.attackTimeValue = 0.0f;
    params.sustainTimeValue = frnd(0.1f);
    params.decayTimeValue = 0.1f + frnd(0.2f);

    if (GetRandomValue(0, 1)) params.hpfCutoffValue = frnd(0.3f);

    regenerate = true;
}

// Generate sound: Jump
static void GenJump(void)
{
    ResetWaveParams(&params);

    params.waveTypeValue = 0;
    params.squareDutyValue = frnd(0.6f);
    params.startFrequencyValue = 0.3f + frnd(0.3f);
    params.slideValue = 0.1f + frnd(0.2f);
    params.attackTimeValue = 0.0f;
    params.sustainTimeValue = 0.1f + frnd(0.3f);
    params.decayTimeValue = 0.1f + frnd(0.2f);

    if (GetRandomValue(0, 1)) params.hpfCutoffValue = frnd(0.3f);
    if (GetRandomValue(0, 1)) params.lpfCutoffValue = 1.0f - frnd(0.6f);

    regenerate = true;
}

// Generate sound: Blip/Select
static void GenBlipSelect(void)
{
    ResetWaveParams(&params);

    params.waveTypeValue = GetRandomValue(0, 1);
    if (params.waveTypeValue == 0) params.squareDutyValue = frnd(0.6f);
    params.startFrequencyValue = 0.2f + frnd(0.4f);
    params.attackTimeValue = 0.0f;
    params.sustainTimeValue = 0.1f + frnd(0.1f);
    params.decayTimeValue = frnd(0.2f);
    params.hpfCutoffValue = 0.1f;

    regenerate = true;
}

// Generate random sound
static void GenRandomize(void)
{
    params.randSeed = GetRandomValue(0, 0xFFFE);

    params.startFrequencyValue = pow(frnd(2.0f) - 1.0f, 2.0f);

    if (GetRandomValue(0, 1)) params.startFrequencyValue = pow(frnd(2.0f) - 1.0f, 3.0f)+0.5f;

    params.minFrequencyValue = 0.0f;
    params.slideValue = pow(frnd(2.0f) - 1.0f, 5.0f);

    if ((params.startFrequencyValue > 0.7f) && (params.slideValue > 0.2f)) params.slideValue = -params.slideValue;
    if ((params.startFrequencyValue < 0.2f) && (params.slideValue < -0.05f)) params.slideValue = -params.slideValue;

    params.deltaSlideValue = pow(frnd(2.0f) - 1.0f, 3.0f);
    params.squareDutyValue = frnd(2.0f) - 1.0f;
    params.dutySweepValue = pow(frnd(2.0f) - 1.0f, 3.0f);
    params.vibratoDepthValue = pow(frnd(2.0f) - 1.0f, 3.0f);
    params.vibratoSpeedValue = frnd(2.0f) - 1.0f;
    //params.vibratoPhaseDelay = frnd(2.0f) - 1.0f;
    params.attackTimeValue = pow(frnd(2.0f) - 1.0f, 3.0f);
    params.sustainTimeValue = pow(frnd(2.0f) - 1.0f, 2.0f);
    params.decayTimeValue = frnd(2.0f)-1.0f;
    params.sustainPunchValue = pow(frnd(0.8f), 2.0f);

    if (params.attackTimeValue + params.sustainTimeValue + params.decayTimeValue < 0.2f)
    {
        params.sustainTimeValue += 0.2f + frnd(0.3f);
        params.decayTimeValue += 0.2f + frnd(0.3f);
    }

    params.lpfResonanceValue = frnd(2.0f) - 1.0f;
    params.lpfCutoffValue = 1.0f - pow(frnd(1.0f), 3.0f);
    params.lpfCutoffSweepValue = pow(frnd(2.0f) - 1.0f, 3.0f);

    if (params.lpfCutoffValue < 0.1f && params.lpfCutoffSweepValue < -0.05f) params.lpfCutoffSweepValue = -params.lpfCutoffSweepValue;

    params.hpfCutoffValue = pow(frnd(1.0f), 5.0f);
    params.hpfCutoffSweepValue = pow(frnd(2.0f) - 1.0f, 5.0f);
    params.phaserOffsetValue = pow(frnd(2.0f) - 1.0f, 3.0f);
    params.phaserSweepValue = pow(frnd(2.0f) - 1.0f, 3.0f);
    params.repeatSpeedValue = frnd(2.0f) - 1.0f;
    params.changeSpeedValue = frnd(2.0f) - 1.0f;
    params.changeAmountValue = frnd(2.0f) - 1.0f;

    regenerate = true;
}

// Mutate current sound
static void GenMutate(void)
{
    if (GetRandomValue(0, 1)) params.startFrequencyValue += frnd(0.1f) - 0.05f;
    //if (GetRandomValue(0, 1)) params.minFrequencyValue += frnd(0.1f) - 0.05f;
    if (GetRandomValue(0, 1)) params.slideValue += frnd(0.1f) - 0.05f;
    if (GetRandomValue(0, 1)) params.deltaSlideValue += frnd(0.1f) - 0.05f;
    if (GetRandomValue(0, 1)) params.squareDutyValue += frnd(0.1f) - 0.05f;
    if (GetRandomValue(0, 1)) params.dutySweepValue += frnd(0.1f) - 0.05f;
    if (GetRandomValue(0, 1)) params.vibratoDepthValue += frnd(0.1f) - 0.05f;
    if (GetRandomValue(0, 1)) params.vibratoSpeedValue += frnd(0.1f) - 0.05f;
    //if (GetRandomValue(0, 1)) params.vibratoPhaseDelay += frnd(0.1f) - 0.05f;
    if (GetRandomValue(0, 1)) params.attackTimeValue += frnd(0.1f) - 0.05f;
    if (GetRandomValue(0, 1)) params.sustainTimeValue += frnd(0.1f) - 0.05f;
    if (GetRandomValue(0, 1)) params.decayTimeValue += frnd(0.1f) - 0.05f;
    if (GetRandomValue(0, 1)) params.sustainPunchValue += frnd(0.1f) - 0.05f;
    if (GetRandomValue(0, 1)) params.lpfResonanceValue += frnd(0.1f) - 0.05f;
    if (GetRandomValue(0, 1)) params.lpfCutoffValue += frnd(0.1f) - 0.05f;
    if (GetRandomValue(0, 1)) params.lpfCutoffSweepValue += frnd(0.1f) - 0.05f;
    if (GetRandomValue(0, 1)) params.hpfCutoffValue += frnd(0.1f) - 0.05f;
    if (GetRandomValue(0, 1)) params.hpfCutoffSweepValue += frnd(0.1f) - 0.05f;
    if (GetRandomValue(0, 1)) params.phaserOffsetValue += frnd(0.1f) - 0.05f;
    if (GetRandomValue(0, 1)) params.phaserSweepValue += frnd(0.1f) - 0.05f;
    if (GetRandomValue(0, 1)) params.repeatSpeedValue += frnd(0.1f) - 0.05f;
    if (GetRandomValue(0, 1)) params.changeSpeedValue += frnd(0.1f) - 0.05f;
    if (GetRandomValue(0, 1)) params.changeAmountValue += frnd(0.1f) - 0.05f;

    regenerate = true;
}

//--------------------------------------------------------------------------------------------
// Auxiliar functions
//--------------------------------------------------------------------------------------------

#if !defined(COMMAND_LINE_ONLY)
// Draw wave data
// NOTE: For proper visualization, MSAA x4 is recommended, alternatively
// it should be rendered to a bigger texture and then scaled down with
// bilinear/trilinear texture filtering
static void DrawWave(Wave *wave, Rectangle bounds, Color color)
{
    float sample, sampleNext;
    float currentSample = 0.0f;
    float sampleIncrement = (float)wave->sampleCount/(float)(bounds.width*2);
    float sampleScale = (float)bounds.height;

    for (int i = 1; i < bounds.width*2 - 1; i++)
    {
        sample = ((float *)wave->data)[(int)currentSample]*sampleScale;
        sampleNext = ((float *)wave->data)[(int)(currentSample + sampleIncrement)]*sampleScale;

        if (sample > bounds.height/2) sample = bounds.height/2;
        else if (sample < -bounds.height/2) sample = -bounds.height/2;

        if (sampleNext > bounds.height/2) sampleNext = bounds.height/2;
        else if (sampleNext < -bounds.height/2) sampleNext = -bounds.height/2;

        DrawLineV((Vector2){ (float)bounds.x + (float)i/2.0f, (float)(bounds.y + bounds.height/2) + sample },
                  (Vector2){ (float)bounds.x + (float)i/2.0f, (float)(bounds.y  + bounds.height/2) + sampleNext }, color);

        currentSample += sampleIncrement;
    }
}
#endif // COMMAND_LINE_ONLY

#if defined(VERSION_ONE) || defined(COMMAND_LINE_ONLY)
// Simple time wait in milliseconds
static void WaitTime(int ms)
{
    if (ms > 0)
    {
        int currentTime = clock()*1000/CLOCKS_PER_SEC;  // Current time in milliseconds
        int totalTime = currentTime + ms;               // Total required time in ms to return from this timeout

        int percent = 0;
        int prevPercent = percent;

        // Wait until current ms time matches total ms time
        while (currentTime <= totalTime)
        {
            // Check for key pressed to stop playing
            if (kbhit())
            {
                int key = getch();
                if ((key == 13) || (key == 27)) break;    // KEY_ENTER || KEY_ESCAPE
            }

            currentTime = clock()*1000/CLOCKS_PER_SEC;

            // Print console time bar
            percent = (int)(((float)currentTime/totalTime)*100.0f);

            if (percent != prevPercent)
            {
                printf("\r[");
                for (int j = 0; j < 50; j++)
                {
                    if (j < percent/2) printf("=");
                    else printf(" ");
                }
                printf("] [%02i%%]", percent);

                prevPercent = percent;
            }
        }

        printf("\n\n");
    }
}

// Play provided wave through CLI
static void PlayWaveCLI(Wave wave)
{
    float waveTimeMs = (float)wave.sampleCount*1000.0/(wave.sampleRate*wave.channels);

    InitAudioDevice();                  // Init audio device
    Sound fx = LoadSoundFromWave(wave); // Load WAV audio file

    printf("\n//////////////////////////////////////////////////////////////////////////////////\n");
    printf("//                                                                              //\n");
    printf("// rFXGen v%s - CLI audio player                                               //\n", TOOL_VERSION_TEXT);
    printf("//                                                                              //\n");
    printf("// more info and bugs-report: github.com/raysan5/rfxgen                         //\n");
    printf("//                                                                              //\n");
    printf("// Copyright (c) 2018 raylib technologies (@raylibtech)                         //\n");
    printf("//                                                                              //\n");
    printf("//////////////////////////////////////////////////////////////////////////////////\n\n");

    printf("Playing sound [%.2f sec.]. Press ENTER to finish.\n", waveTimeMs/1000.0f);

    PlaySound(fx);                      // Play sound
    WaitTime(waveTimeMs);               // Wait while audio is playing
    UnloadSound(fx);                    // Unload sound data
    CloseAudioDevice();                 // Close audio device
}

#if !defined(__WIN32)
// Check if a key has been pressed
static int kbhit(void)
{
	struct termios oldt, newt;
	int ch;
	int oldf;

	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

	ch = getchar();

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	fcntl(STDIN_FILENO, F_SETFL, oldf);

	if (ch != EOF)
	{
		ungetc(ch, stdin);
		return 1;
	}

	return 0;
}
#endif
#endif      // VERSION_ONE
