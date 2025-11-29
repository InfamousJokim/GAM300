// Runtime/src/main.cpp
// Standalone game runtime (no editor, no ImGui)

#include <cstdint>
#include <memory>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <Windows.h>

#include "BoomEngine.h"
#include "Core.h"  // For BOOM_INFO, BOOM_ERROR logging macros

int32_t main()
{
    // Open log file for debugging
    std::ofstream logFile("Game_log.txt");
    std::streambuf* coutBackup = std::cout.rdbuf();
    std::streambuf* cerrBackup = std::cerr.rdbuf();

    // Redirect cout and cerr to both console and file
    std::cout.rdbuf(logFile.rdbuf());
    std::cerr.rdbuf(logFile.rdbuf());

    try
    {
        std::cout << "========================================" << std::endl;
        std::cout << "Game Runtime Starting..." << std::endl;
        std::cout << "========================================" << std::endl;

        // IMPORTANT: Set working directory to exe location
        // This ensures Resources/, Scenes/, and Scripts/ folders are found correctly
        namespace fs = std::filesystem;

        std::cout << "Getting exe path..." << std::endl;
        std::cout.flush();

        fs::path exePath = fs::current_path();

        // Try to find Game.exe or Runtime.exe location
        char buffer[MAX_PATH];
        GetModuleFileNameA(NULL, buffer, MAX_PATH);
        exePath = fs::path(buffer).parent_path();

        std::cout << "Exe path: " << exePath.string() << std::endl;
        std::cout.flush();

        // Set working directory to where the exe is
        fs::current_path(exePath);

        std::cout << "Runtime working directory: " << exePath.string() << std::endl;
        std::cout << "Checking for required folders..." << std::endl;

        // Check for required folders
        std::cout << "  Resources/ exists: " << (fs::exists("Resources") ? "YES" : "NO") << std::endl;
        std::cout << "  Scenes/ exists: " << (fs::exists("Scenes") ? "YES" : "NO") << std::endl;
        std::cout << "  Scripts/ exists: " << (fs::exists("Scripts") ? "YES" : "NO") << std::endl;

        std::cout << "Creating engine instance..." << std::endl;
        std::cout.flush();

        // Boot the engine
        MyEngineClass engine;

        std::cout << "Engine instance created successfully" << std::endl;
        std::cout.flush();

        engine.whatup();

        std::cout << "Engine whatup() called" << std::endl;
        std::cout.flush();

        std::cout << "Game Runtime Started" << std::endl;
        std::cout.flush();

        // Init audio (FMOD)
        std::cout << "Initializing FMOD..." << std::endl;
        std::cout.flush();

        if (!SoundEngine::Instance().Init())
        {
            std::cerr << "FMOD init failed" << std::endl;
            return -1;
        }

        std::cout << "FMOD initialized successfully" << std::endl;
        std::cout.flush();

        // Create the engine Application (owns the GLFW window + run loop)
        std::cout << "Creating application..." << std::endl;
        std::cout.flush();

        auto app = engine.CreateApp();

        std::cout << "Application created" << std::endl;
        std::cout.flush();

        app->PostEvent<WindowTitleRenameEvent>("Game");

        std::cout << "Starting game loop..." << std::endl;
        std::cout.flush();

        // Run the game (no editor, no ImGui frame rendering needed)
        app->RunContext(false);

        std::cout << "Game loop ended" << std::endl;
        std::cout.flush();

        // Cleanup
        SoundEngine::Instance().Shutdown();
        std::cout << "Game Runtime Shutdown" << std::endl;

        // Flush and close log
        std::cout.flush();
        logFile.close();
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n========================================" << std::endl;
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        std::cerr << "========================================" << std::endl;

        BOOM_ERROR("Application failed: {}", e.what());

        // Flush log file
        std::cerr.flush();
        logFile.flush();
        logFile.close();

        // Restore console output
        std::cout.rdbuf(coutBackup);
        std::cerr.rdbuf(cerrBackup);

        // Show message box with error
        std::string errorMsg = "Game crashed!\n\nError: ";
        errorMsg += e.what();
        errorMsg += "\n\nCheck Game_log.txt for details.";
        MessageBoxA(NULL, errorMsg.c_str(), "Game Error", MB_OK | MB_ICONERROR);

        return -1;
    }
    catch (...)
    {
        std::cerr << "\n========================================" << std::endl;
        std::cerr << "FATAL ERROR: Unknown exception" << std::endl;
        std::cerr << "========================================" << std::endl;

        // Flush log file
        logFile.flush();
        logFile.close();

        // Restore console output
        std::cout.rdbuf(coutBackup);
        std::cerr.rdbuf(cerrBackup);

        MessageBoxA(NULL, "Game crashed with unknown error!\n\nCheck Game_log.txt for details.", "Game Error", MB_OK | MB_ICONERROR);

        return -1;
    }

    return 0;
}
