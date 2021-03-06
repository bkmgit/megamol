/*
 * ButtonParamUILayer.h
 *
 * Copyright (C) 2016 MegaMol Team
 * Alle Rechte vorbehalten. All rights reserved.
 */
#pragma once

#include "AbstractUILayer.h"
#include <chrono>
#include <map>
#include "vislib/String.h"
#include "vislib/sys/KeyCode.h"

namespace megamol {
namespace console {

    /**
     * This UI layer implements hot key for button parameter.
     */
    class ButtonParamUILayer : public AbstractUILayer {
    public:
        ButtonParamUILayer(gl::Window& wnd, void * coreHandle, void * viewHandle);
        virtual ~ButtonParamUILayer();

        inline void SetMaskingLayer(AbstractUILayer *layer) {
            maskingLayer = layer;
        }
        virtual bool Enabled();

        virtual bool onKey(Key key, int scancode, KeyAction action, Modifiers mods);
    private:
        void updateHotkeyList();

        void *hCore; // handle memory is owned by application
        void *hView; // handle memory is owned by Window

        std::chrono::system_clock::time_point last_update;
        std::map<vislib::sys::KeyCode, vislib::TString> hotkeys;
        AbstractUILayer *maskingLayer;
    };

}
}
