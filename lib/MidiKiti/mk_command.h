#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "mk_common.h"
#include "lu_state/state.h"

namespace mk
{

class _AbstractMidiInterface;

class MidiCommander : public lutil::StateDriver<MidiCommander>
{
public:
    static void SetInterfaceInstance(MidiCommander *instance);

    explicit MidiCommander(int ready_in = -1, int ready_out = -1)
        : lutil::StateDriver<MidiCommander>()
        , _ready_in(ready_in)
        , _ready_out(ready_out)
    {
        // TODO: Setup the ready-in-out wire tooling
        add_runtime(Off, &MidiCommander::runtime);
    }

    bool is_local() const;
    bool is_connected() const;
    bool initialize();

    // Check to see if we are on deck to be added to the
    // network.
    bool ready_to_connect();
    bool runtime_engage();
    void connect();
    void flush();

    void set_address(uint8_t address);
    uint8_t address() const;

    void events_requested();

    // Currently we do nothing, but we'll probably
    // use this in the future to handle pushing events
    void runtime() {}

    // Called via the interface constructors
    void add(_AbstractMidiInterface *interface);

    // Post a new event. Called via the interfaces
    void queue_event(const RawEvent &event);

    // Take any pooled events
    void take_events(lutil::Vec<RawEvent> &events);

    void push_layout(Stream *stream);
    void query_preferences(uint8_t index);
    void set_preferences(
        uint8_t index,
        uint8_t size,
        uint8_t *buffer
    );

protected:
    friend class MidiController;
    lutil::Vec<_AbstractMidiInterface*> &components();

private:
    uint8_t _address;

    bool _connected = false;
    int _ready_in;
    int _ready_out;

    lutil::Vec<_AbstractMidiInterface*> _interfaces;

    // All events we're waiting to send to the controller
    lutil::Vec<RawEvent> _pooled_events;
    bool _requested;
};

}
