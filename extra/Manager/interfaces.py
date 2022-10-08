from PySide6 import QtCore

import threading
import struct
import time

import serial
import serial.threaded

from protocol import Command
import mk

class MidiInterface(QtCore.QObject):
    """
    Aligns with an mk::_AbstractMidiInterface on the mk::MidiCommander
    """
    preferencesLoaded = QtCore.Signal()
    
    def __init__(self, commander, index, type):
        super().__init__()
        self._command = commander
        self._index = index
        self._type = type

        self._parameters = None
        self._parameter_class = None
        if type in mk.ParameterTypes:
            self._parameter_class = mk.ParameterTypes[type]


    def __str__(self):
        return "MidiInterface({}, {})".format(
            mk.TypeNames[self._type],
            self._index
        )

    @property
    def type(self):
        return self._type

    @property
    def parameters(self):
        return self._parameters

    @property
    def parameter_types(self):
        if not self._parameter_class:
            return []
        
        return [
            (n,a) for n,a in vars(self._parameter_class).items() \
            if isinstance(a, mk._unit)
        ]
    
    @property
    def command(self):
        return self._command

    @property
    def index(self):
        return self._index


    def header(self):
        """
        2 byte header

        :return: ``bytes``
        """
        return struct.pack(
            'BB', self.command.index, self.index
        )


    def process_preferences(self, payload):
        """
        The payload looks something like:

        [ parameter_type, data... ]
        """
        if not self._parameter_class:
            return

        param_type, = struct.unpack('H', payload[:2])
        if param_type not in mk.ParameterTypes:
            return

        self._parameters = self._parameter_class(payload[2:])
        self.preferencesLoaded.emit()


    def parameter_payload(self):
        """
        The UI layer will make adjustments to the _parameters
        instance, we just need to package that and send it back
        to the midi controller.
        """
        if not self._parameter_class:
            return None

        size = struct.calcsize('<H' + self._parameter_class.format())

        payload = [
        #                        Set Parmeter Header
        #   +----------------------------------------------------------+
        #   |          PrefrenceHeader       |       Data Header       |
        #   +----------------------------------------------------------+
        #   |     Commander     |    Index   |   Pref Size  |    ID    |
            self.command.index,  self.index ,     size     , self._type
        ]

        # <BBB marks little-endian and the 4 byte header
        full_format = '<BBBH'+ self._parameter_class.format()

        print (full_format)

        for name, parm in self.parameter_types:
            payload.append(getattr(self._parameters, name))

        print (payload)

        return struct.pack(
            full_format, *payload
        )


class MidiCommander(QtCore.QObject):
    """
    Aligns with a mk::MidiCommand on the mk::MidiController
    """
    def __init__(self, index, count, types):
        super().__init__()
        self._index = index
        self._items = []
        
        for i in range(count):
            self._items.append(MidiInterface(self, i, types[i]))

    def __str__(self):
        return "MidiCommander({})".format(
            ', '.join(str(i) for i in self._items)
        )

    @property
    def interfaces(self):
        return self._items


    @property
    def index(self):
        return self._index



class MidiLayout(QtCore.QObject):
    """
    A utility structure for laying out and building the
    MidiController preferences widget.
    """
    @classmethod
    def initialize(cls, controller):
        """
        Initialize a layout structure based on the input
        controller

        :param controller: serial.Serial
        :return: :class:`MidiLayout`
        """
        mk.request(controller, mk.GetLayout, bytes(1))

        # Wait a moment for a response
        time.sleep(0.05)

        while controller.in_waiting:
            # Wait for it!
            if controller.read()[0] == mk.StartFlag:
                break
        
        return cls(controller)


    def __init__(self, controller):
        self._commanders = []

        # First byte will be the number of commanders
        # For each commander, the first byte is the
        # number of interfaces, and each byte after
        # is the interface type id.

        if controller.read()[0] != mk.GetLayout:
            return

        count = controller.read()[0]

        offset = 0
        for i in range(count):

            item_count = controller.read()[0]
            items = []

            for item_index in range(item_count):
                items.append(controller.read()[0])

            self._commanders.append(MidiCommander(
                i,
                item_count,
                items
            ))


    @property
    def commanders(self):
        return self._commanders


    def __str__(self):
        return "MidiController({})".format(
            ', '.join(str(c) for c in self._commanders)
        )


    @QtCore.Slot(Command)
    def process_command(self, command):
        """
        Handle a command sent from the mk::MidiController

        This sort-of works in reverse from the
        mk::MidiController::process_command. For example, the
        mk.GetPreferences means we've received preference data
        from the controller.
        """

        if command.id == mk.GetPreferences:
            # We've received preference data for a specific
            # interface. Find that interface and ask it to
            # handle the processing
            data = command.payload
            
            # The first two bytes are going to be the commander
            # and interface indexes.
            command = self._commanders[data[0]]
            interface = command.interfaces[data[1]]

            # All subsiquent data is the preference buffer
            interface.process_preferences(data[2:])

        elif command.id == mk.Message:

            print('[MESSAGE]: ', command.payload.decode('utf-8'))
