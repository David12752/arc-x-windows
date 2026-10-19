/*
 * ============================================================================
 * ARC System Audio Host - Headless Engine Entry Point
 *
 * This file defines the JUCE application class for the arc-engine.exe process.
 * It is a console app (no GUI) — JUCE_HEADLESS_PLUGIN_CLIENT is set in
 * CMakeLists.txt and no juce_gui_basics module is linked.
 *
 * Process lifecycle
 * -----------------
 *   1. OS spawns arc-engine.exe (typically by the Electron main process)
 *   2. initialise() creates EngineCore and calls initialize()
 *   3. Engine runs indefinitely — JUCE MessageManager pumps messages
 *   4. Shutdown is triggered by:
 *        a) UI sends ShutdownCmd over the named pipe
 *        b) OS sends SIGTERM / WM_QUIT
 *        c) Parent Electron process exits (engine detects pipe disconnect)
 *   5. systemRequestedQuit() → quit() → shutdown() → exit
 *
 * Exit codes
 * ----------
 *   0  clean shutdown
 *   1  initialization failure (error written to stderr + JUCE logger)
 * ============================================================================
 */

#include "JuceHeader.h"
#include "Engine/EngineCore.h"

namespace ARC
{

//=============================================================================
class ARCEngineApp final : public juce::JUCEApplication
{
public:
    //=========================================================================
    ARCEngineApp() = default;

    //=========================================================================
    const juce::String getApplicationName()    override { return "ARC Engine"; }
    const juce::String getApplicationVersion() override { return "2.0.0"; }
    bool moreThanOneInstanceAllowed()           override { return false; }

    //=========================================================================
    void initialise(const juce::String& /*commandLine*/) override
    {
        // Log to JUCE default logger (stdout / debug output on Windows)
        juce::Logger::writeToLog("ARC Engine v2.0.0 starting...");

        engineCore_ = std::make_unique<EngineCore>();
        const juce::String error = engineCore_->initialize();

        if (error.isNotEmpty())
        {
            juce::Logger::writeToLog("ARC Engine FATAL: " + error);
            std::cerr << "ARC Engine FATAL: " << error.toStdString() << std::endl;
            setApplicationReturnValue(1);
            quit();
            return;
        }

        juce::Logger::writeToLog("ARC Engine running — listening on "
                                  "\\\\.\\pipe\\arc-command");
    }

    //=========================================================================
    void shutdown() override
    {
        juce::Logger::writeToLog("ARC Engine shutting down...");
        if (engineCore_)
        {
            engineCore_->shutdown();
            engineCore_.reset();
        }
        juce::Logger::writeToLog("ARC Engine exit");
    }

    //=========================================================================
    void systemRequestedQuit() override
    {
        // Called when OS sends quit signal or ShutdownCmd is received.
        // Let JUCE tear down cleanly (calls shutdown() above).
        quit();
    }

    //=========================================================================
    void anotherInstanceStarted(const juce::String& /*commandLine*/) override
    {
        // moreThanOneInstanceAllowed() returns false so this should never fire.
        // If it does, ignore.
    }

private:
    //=========================================================================
    std::unique_ptr<EngineCore> engineCore_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ARCEngineApp)
};

} // namespace ARC

//=============================================================================
// JUCE entry point macro — generates main() for the console app
START_JUCE_APPLICATION(ARC::ARCEngineApp)
