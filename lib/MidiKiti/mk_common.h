#pragma once

#include <Arduino.h>
#include "mk_util.h"

#include "lutil.h"
#include "lu_memory/managed_ptr.h"

#define ENGAGE_COMMAND "###"
#define EVENT_COMMAND '!'

#define OCTAVE_ID 0xF1
#define POT_ID 0xF2
#define BUTTON_ID 0xF3

#define KEY_EVENT_ID 1
#define POT_EVENT_ID 2
#define BUTTON_EVENT_ID 2

#define Command_StartFlag 0xFF
#define Command_GetLayout 0x01
#define Command_GetPreferences 0x02
#define Command_SetPreferences 0x03
#define Command_Message 0x04

constexpr char Off[] = "Off";
constexpr char Boot[] = "Boot";
constexpr char Engage[] = "Engage";
constexpr char Connect[] = "Connect";
constexpr char Wait[] = "Wait";
constexpr char Runtime[] = "Runtime";

namespace mk {

#pragma pack(push, 1)

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

// Events come in a few shapes but they are never larger
// than 64 bytes so that's how we'll store them.
union RawEvent{
    uint8_t type;
    uint64_t payload;
};

// --------------------------------------------------------------------
// -- Event Structures
// --------------------------------------------------------------------

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

struct PotEvent
{
    // Required for all events
    uint8_t type;     // Event Type
    uint8_t address;  // I2C address

    // Specific to the KeyEvent
    uint8_t command;   // Command
    uint8_t control;   // CC Control
    uint8_t value;     // Value
};

struct ButtonEvent
{
    // Required for all events
    uint8_t type;     // Event Type
    uint8_t address;  // I2C address

    uint8_t control;  // CC Control
    uint8_t pressed;  // MIDI Note On/Off
};

// --------------------------------------------------------------------
// -- Event Casting
// --------------------------------------------------------------------

template<typename T>
T *event_cast(RawEvent *e)
{
    static_assert(sizeof(RawEvent) >= sizeof(T), "Invalid event_cast!");
    return reinterpret_cast<T*>(e);
}

template<typename T>
RawEvent *rawevent_cast(T *e)
{
    static_assert(sizeof(RawEvent) >= sizeof(T), "Invalid rawevent_cast!");
    return reinterpret_cast<RawEvent*>(e);
}

// --------------------------------------------------------------------
// -- Parameter Settings
// --------------------------------------------------------------------

struct Parameters
{
    uint16_t parameter_type; // Aligns with the UUID

    Parameters(uint16_t id)
        : parameter_type(id)
    {}
};

struct OctaveParameters : public Parameters
{
    OctaveParameters()
        : Parameters(OCTAVE_ID)
    {}
};

struct PotParameters : public Parameters
{
    PotParameters()
        : Parameters(POT_ID)
    {}

    uint8_t control = 1;

    uint16_t high = 1023;
    uint16_t low = 0;

    uint8_t midi_high = 127;
    uint8_t midi_low = 0;

    uint16_t threshold = 5;
    bool invert = false;
};

struct ButtonParameters : public Parameters
{
    ButtonParameters()
        : Parameters(BUTTON_ID)
    {}

    uint8_t control = 1;

    bool toggle = false;
};

// --------------------------------------------------------------------
// -- Command Structures
// --------------------------------------------------------------------

// The "raw" command that comes from the serial input
struct Command
{
    uint8_t id;
    uint8_t size;
    lutil::managed_data payload;
};

struct PreferencesHeader
{
    // The address of the MidiCommander
    uint8_t commander;
    
    // The index of the interface in our commander
    uint8_t index;
};

inline void Message(const char *message)
{
    Serial.write(Command_StartFlag);
    Serial.write(Command_Message);
    Serial.write(strlen(message));
    Serial.write(message);
}

#pragma pack(pop)

}
