#pragma once
#include "mk_common.h"
#include "mk_command.h"
#include "mk_interface.h"

namespace mk
{

/**
 * Slider to MIDI Controllers to go crazy with!
 */
class MidiPot : public _MidiInterface
{
public:
    struct Config
    {
        // The pin this pot is read on
        int pin;

        // MIDI Command and Control for events
        uint8_t command;
        uint8_t control;

        // Should we invert this pots' values?
        bool invert = false;

        // bottom-out analog range
        uint16_t low = 0;

        // peak analog range
        uint16_t high = 1023;

        // minimum midi value
        uint8_t midi_low = 0;

        // max midi value
        uint8_t midi_high = 127;

        // analog change requirement to spawn
        // an event.
        uint16_t threshold = 5;
    };

    explicit MidiPot(
        const Config &config,
        MidiCommander *command)
        : _MidiInterface(POT_ID, command)
    {
        _config = config;
        pinMode(config.pin, INPUT);
    }

    void runtime() override
    {
        int current = analogRead(_config.pin);

        if (abs(current - _last) > _config.threshold)
        {
            // Let's call this a change!
            PotEvent event;
            event.type = POT_EVENT_ID;

            // TODO: Implement the address on a per-item basis where the
            // second byte is the offset of the item
            event.address = _command->address();
            event.control = _config.control;
            event.command = _config.command;

            event.value = calculate(current);
            if (event.value == _last_val)
                return; // We've already set to this value

            _last_val = event.value;
 
            queue_event(event);

            _last = current;
        }
    }

    virtual Parameters *parameters(size_t &size) const
    {
        size = sizeof(PotParameters);
        PotParameters *parms = new PotParameters();

        parms->control = _config.control;
        parms->high = _config.high;
        parms->low = _config.low;
        parms->midi_high = _config.midi_high;
        parms->midi_low = _config.midi_low;
        parms->threshold = _config.threshold;
        parms->invert = _config.invert;

        return parms;
    }

    virtual void setParameters(Parameters *parameters, size_t size)
    {
        if (size < sizeof(PotParameters))
        {
            Message("Invalid Param Size!");
            return;
        }

        PotParameters *parms = static_cast<PotParameters*>(parameters);

        _config.control = parms->control;
        _config.high = parms->high;
        _config.low = parms->low;
        _config.midi_high = parms->midi_high;
        _config.midi_low = parms->midi_low;
        _config.threshold = parms->threshold;
        _config.invert = parms->invert;

        Message("Pot Parms!");
    }

    uint8_t calculate(int value)
    {
        uint8_t lo = _config.invert
            ? _config.midi_high
            : _config.midi_low;

        uint8_t hi = _config.invert
            ? _config.midi_low
            : _config.midi_high;

        return (uint8_t) map(
            value,
            _config.low,
            _config.high,
            lo,
            hi
        );
    }

private:
    Config _config;
    int _last = 0; // analog
    uint8_t _last_val = 0; // MIDI
};

}
