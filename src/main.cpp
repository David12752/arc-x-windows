/*
 * ============================================================================
 * ARC System Audio Host - Main Entry Point
 * A professional Windows application for system-wide VST3 audio processing
 * ============================================================================
 *
 * Copyright (c) 2024 ARC Audio
 * All rights reserved.
 *
 * This application provides:
 * - System-wide audio capture via WASAPI loopback
 * - VST3 plugin hosting with full GUI support
 * - Real-time audio processing with low latency
 * - Professional dark-themed interface
 * - Plugin chain management with drag-and-drop reordering
 * - Preset saving and loading
 * - System tray integration
 *
 * ============================================================================
 */

#include "Application.h"

//=============================================================================
/**
 * Create the application instance
 * JUCE macro that sets up the application entry point
 */
START_JUCE_APPLICATION(ARC::Application)

