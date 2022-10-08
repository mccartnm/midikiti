#pragma once
#include "mk_common.h"
#include "mk_command.h"
#include "mk_shift.h"
#include "mk_interface.h"

#define kMaxPressTime 800000 // .8s (preference? SD Card? idk...)

namespace mk
{

class MidiKey
{
public:
    explicit MidiKey(
        uint8_t key,
        uint32_t down,
        uint32_t pressed)
        : m_key(key)
        , _pressedPin(pressed)
        , _downPin(down)
    {
        _down = false;
        _pressed = false;
    }

    bool process(KeyEvent &event, KeyShift *shift)
    {
        uint32_t value = ~shift->getCurrent();

        if (_pressed)
        {
            // We are touching the key
            if (_down)
            {
                // The key was last registered down. Is is still
                // down?
                if (!(value & downPin()))
                {
                    // We've let up on the key.
                    _down = false;
                    _pressed = (value & pressedPin());
                    _pressTime = ElapsedMicros();

                    event.key = m_key;
                    event.pressed = 0;
                    event.velocity = 0;
                    return true;
                }

                if (!(value & pressedPin()))
                {
                    // We've let up on the key completely
                    _pressed = false;
                    _down = false;

                    // We need to release (just in case)
                    // event.key = m_key;
                    // event.pressed = 0;
                    // event.velocity = 0;
                    return false;
                }
            }
            else if (value & downPin())
            {
                // We've completed the full press, we can queue the key
                // event as required.
                _down = true;
                event.key = m_key;
                event.pressed = 1;
                event.velocity = calculate_velocity();
                return true;
            }
            else if (!(value & pressedPin()))
            {
                _pressed = false;
                if (_down)
                {
                    // We need to release (just in case)
                    event.key = m_key;
                    event.pressed = 0;
                    event.velocity = 0;
                    _down = false;
                    return true;
                }

                _down = false;
                return false;
            }

            return false;
        }
        else if (value & pressedPin())
        {
            // We've begone the pressing process
            _pressed = true;
            _pressTime = ElapsedMicros();
        }
        else
        {
            // Clear everything just in case
            _pressed = false;
            _down = false;
        }
        return false;
    }

    uint32_t downPin() const
    {
        return 1 << _downPin;
    }

    uint32_t pressedPin() const
    {
        return 1 << _pressedPin;
    }

private:
    uint8_t calculate_velocity() const
    {
        unsigned long diff = _pressTime;
        diff = max(0UL, min(diff, kMaxPressTime));
        unsigned long mapped = map(diff, 0, kMaxPressTime, 127, 1);
        return mapped;
    }

    uint8_t m_key;

    bool _pressed : 1;
    bool _down : 1;
    ElapsedMicros _pressTime;

    uint32_t _pressedPin;
    uint32_t _downPin;
};

/**
 * A piano key octave interface.
 */
class MidiOctave : public _MidiInterface
{
public:

    struct Config
    {
        int loadPin;
        int clockEnablePin;
        int dataPin;
        int clickPin;
    };

    explicit MidiOctave(const Config &config, MidiCommander *command)
        : _MidiInterface(OCTAVE_ID, command)
    {
        _shift.begin(
            config.loadPin,
            config.clockEnablePin,
            config.dataPin,
            config.clickPin
        );
    }

    void add_key(MidiKey *key)
    {
        _keys.push(key);
    }

    void runtime() override
    {
        if (_shift.update())
        {
            KeyEvent event;
            event.type = KEY_EVENT_ID;
            event.address = _command->address();

            for (size_t i = 0; i < _keys.count(); i++)
            {
                if (_keys[i]->process(event, &_shift))
                    queue_event(event);
            }
        }
    }

    virtual Parameters *parameters(size_t &size) const
    {
        size = sizeof(OctaveParameters);
        return new OctaveParameters();
    }

    virtual void setParameters(Parameters *parameters, size_t size)
    {
        if (size < sizeof(OctaveParameters))
            return;

        // OctaveParameters *parms = static_cast<OctaveParameters*>(parameters);
        // todo: fill out parameters
    }

private:
    // Keys we've already registered as pressed
    lutil::Vec<MidiKey*> _keys;

    // Register for our piano keys
    mk::KeyShift _shift;
};

} // namespace mk
