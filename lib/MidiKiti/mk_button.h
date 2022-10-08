#pragma once
#include "mk_common.h"
#include "mk_interface.h"

#include "lutil.h"
#include "lu_process/button.h"

namespace mk
{

/**
 * Simple button within the midi interface. Can be set up as a
 * toggle or a simepl fire and forget.
 */
class MidiButton
    : public _AbstractMidiInterface
    , public lutil::Button
{
protected:
    struct Config
    {
        int pin;

        uint8_t control;

        // True if this is a toggled-button
        bool toggle = false;

        // If this button also comes with a light, we
        // can set that up here.
        int ledPin = -1;
    };

    MidiButton(const Config &config, MidiCommander *command);

    // Implement _AbstractMidiInterface
    uint8_t interface_id() const override;
    Parameters *parameters(size_t &size) const override;
    void setParameters(Parameters *parameters, size_t size) override;

    // Implement lutil::Button options
    void pressed() override;
    void released() override;

private:
    // Implement the midi interface requirements
    MidiCommander *_command = nullptr;

    uint8_t _control;
    bool _toggle = false;

    int _ledPin;
    bool _on = false; // If toggle is true, this will change
};

}
