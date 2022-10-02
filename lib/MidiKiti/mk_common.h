#pragma once

#define ENGAGE_COMMAND "###"
#define EVENT_COMMAND '!'
#define OCTAVE_ID 0xF1

#define KEY_EVENT_ID 1


namespace mk {

// Events come in a few shapes but they are never larger
// than 64 bytes so that's how we'll store them.
union RawEvent{
    uint8_t type;
    uint64_t payload;
};

struct KeyEvent
{
    // Required for all events
    uint8_t type;     // Event Type
    uint8_t address;  // I2C address

    // Specific to the KeyEvent
    uint8_t key;      // MIDI Key
    uint8_t velocity; // MIDI Velocity Reading
    uint8_t pressed;  // MIDI Note On/Off
};

template<typename T>
T *event_cast(RawEvent *e)
{
    return reinterpret_cast<T*>(e);
}

}
