#include "mk_button.h"
#include "mk_command.h"

namespace mk
{

MidiButton::MidiButton(const Config &config, MidiCommander *command)
    : lutil::Button(config.pin)
    , _command(command)
{
    _control = config.control;
    _toggle = config.toggle;

    _command->add(this);

    _ledPin = config.ledPin;
    if (_ledPin >= 0)
        pinMode(_ledPin, OUTPUT);
}

uint8_t MidiButton::interface_id() const
{
    return BUTTON_ID;
}

Parameters *MidiButton::parameters(size_t &size) const
{
    size = sizeof(ButtonParameters);

    ButtonParameters *parms = new ButtonParameters();
    parms->toggle = _toggle;
    return parms;
}

void MidiButton::setParameters(Parameters *parameters, size_t size)
{
    if (size <= sizeof(ButtonParameters))
        return;

    ButtonParameters *parms = static_cast<ButtonParameters*>(parameters);
    parms->toggle = _toggle;
    parms->control = _control;
}

void MidiButton::pressed()
{
    ButtonEvent event;
    event.type = BUTTON_EVENT_ID;
    event.address = _command->address();
    event.control = _control;

    if (_toggle)
    {
        _on = !_on;
        event.pressed = _on ? 1 : 0;
    }
    else
    {
        event.pressed = 1;
    }
    
    _command->queue_event(*rawevent_cast(&event));
}

void MidiButton::released()
{
    if (_toggle)
        return; // Nothing to do here.
    
    ButtonEvent event;
    event.type = BUTTON_EVENT_ID;
    event.address = _command->address();
    event.control = _control;
    event.pressed = 0;

    _command->queue_event(*rawevent_cast(&event));
}

}
