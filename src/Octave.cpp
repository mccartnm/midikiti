#include "MidiKiti.h"

/**
 * The piano module. This is a pretty killer little ditty
 * for working with the 
 */

struct PianoKey
{
    uint8_t index; // e.g. 0 == C, 1 == C#, ...
    const char *name; // e.g. "A " | "Ab" | "A#"
};

static const PianoKey s_keys[] = {
    { 0, "C" },
    { 1, "C#" },
    { 2, "D" },
    { 3, "D#" },
    { 4, "E" },
    { 5, "F" },
    { 6, "F#" },
    { 7, "G" },
    { 8, "G#" },
    { 9, "A" },
    { 10, "A#" },
    { 11, "B" }
};
constexpr uint8_t s_keyCount = 12;

class MidiOctave : public mk::_MidiInterface
{
public:
    explicit MidiOctave(int _ready_in, int _ready_out)
        : mk::_MidiInterface(OCTAVE_ID, _ready_in, _ready_out)
    {
        _shift.begin(
            load_pin,
            load_pin,
            data_pin,
            clock_pin
        );
    }

    void runtime() override
    {
        using namespace mk;

        if (_shift.update())
        {
            // We've pressed _something_ - read them all
            // and see what elements have been pressed

            // We have 24 inputs, 2 for each key

            // __START_HERE__ >> ONCE WE HAVE MORE INFO...

            mk::RawEvent event;
            event.type = KEY_EVENT_ID;

            KeyEvent *key = event_cast<KeyEvent>(&event);

            key->address = address();
            key->key = keyId;
            key->velocity = velocity;
            key->pressed = pressed;

            queue_event(event);
        }
    }

private:
    // Keys we've already registered as pressed
    struct KeyPress
    {
        size_t _init_time;
        bool _pressed : 1; // true if started
        bool _down : 1;    // true of played
    };
    KeyPress _presses[s_keyCount];

    // Register for our piano keys
    mk::ShiftIn<4> _shift;
};

MidiOctave *octave = nullptr;

void setup()
{
  octave = new MidiOctave();
  mk::_MidiInterface::SetInterfaceInstance(octave);

  lutil::Processor::get().init();
}

void loop()
{
  lutil::Processor::get().process();
}