#pragma once

#include <Wire.h>

#include "mk_common.h"

#include "lutil.h"
#include "lu_state/state.h"
#include "lu_storage/vector.h"

constexpr char Off[] = "Off";
constexpr char Boot[] = "Boot";
constexpr char Connect[] = "Connect";
constexpr char Wait[] = "Wait";
constexpr char Runtime[] = "Runtime";

namespace mk {

/**
 * Required non-pure-virtual class for the state driver template.
 */
class _MidiDriver : public lutil::StateDriver<_MidiInterface>
{
public:
    _MidiDriver()
        : lutil::StateDriver<_MidiInterface>()
    {}
};

/**
 * Abstract interface for the devices in the midi deck
 */
class _MidiInterface : public _MidiDriver
{
public:
    static void SetInterfaceInstance(_MidiInterface *instance);

    explicit _MidiInterface(uint8_t id, int ready_in, int ready_out);

    uint8_t address() const;
    void events_requested();

    // -- Transitions

    bool initialize();

    // Check to see if we are on deck to be added to the
    // network.
    bool ready_to_connect();
    bool connect_complete();
    bool runtime_engage();

    // -- Runtimes

    void connect();

    // Each device will have it's own runtime, but needs to
    // call this to clear any pooled events
    virtual void runtime();

protected:
    void queue_event(const RawEvent &event);

private:
    void flush();

    int _ready_in;
    int _ready_out;
    uint16_t _uuid;

    // All events we're waiting to send to the controller
    lutil::Vec<RawEvent> _pooled_events;
    bool _requested;

    bool _connected;
    uint8_t _address;
};

} // namespace mk