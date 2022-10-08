from PySide6 import QtCore
Qt = QtCore.Qt

import serial
import serial.threaded

import mk

class Command(object):
    """
    Mirror the mk::Commmand structure to handle packets of
    variable size. These are then tranlated to the correct
    object type based on the id
    """
    def __init__(self):
        # The id of the command (e.g. mk.GetPreferences)
        self.id = None

        # The total size of the payload
        self.size = None

        # The payload itself (in bytes)
        self.payload = None


class MidiKitiProtocol(QtCore.QObject):
    """
    Class designed to be run on a thread to handle transmitting data
    and then handling it on the main thread via a connection
    """
    received = QtCore.Signal(Command)

    def __init__(self, controller, lock, parent=None):
        super().__init__(parent)
        self._lock = lock
        self._controller = controller
        self._command = None

        self._buffer = []
        self._payload_index = 0
 
        self.error = None

    @QtCore.Slot()
    def execute(self):
        """
        The primary runtime of the thread. This is where we
        receive data and transmit that over a the received()
        signal.
        """
        data = None

        # Critical Section
        with self._lock:
            if not self._controller.is_open:
                return # We've closed the controller!
            
            # Read everything into our buffer. We have a 
            # proper computer, so we're not worried about
            # overflow here.
            try:
                data = self._controller.read(
                    self._controller.in_waiting or 1
                )
            except serial.SerialException as err:
                self.error = err
                return

        if not data:
            # If we're not doing anything on the line
            # give ourselves a heartbeat to avoid over
            # tapping. 10ms should do it.
            QtCore.QThread.msleep(10)

        for byte in data:

            if self._command is None:
                if byte == mk.StartFlag:
                    self._command = Command()
                    self._command.id = mk.StartFlag
                    self._command.size = mk.StartFlag
            
            elif self._command.id == mk.StartFlag:
                self._command.id = byte

            elif self._command.size == mk.StartFlag:
                self._command.size = byte

            else:
                self._buffer.append(byte)
                self._payload_index += 1

                if self._payload_index >= self._command.size:
                    
                    # We've obtained a complete command.
                    self._command.payload = bytes(self._buffer)
                    self.received.emit(self._command)

                    self._command = None
                    self._buffer = []
                    self._payload_index = 0

        # If we're here, we want to queue up this process for
        # the another pass in the event loop. This allows us
        # to process other connections (e.g. request(...))
        # while repeating this process. This also makes
        # cleanup quite trivial as well
        QtCore.QTimer.singleShot(0, self.execute)


    @QtCore.Slot(int, bytes)
    def request(self, command, payload):
        """
        Submit a request to the controller. We'll get the
        response at some point in the future. This detaches
        us from a synchronous response cycle but makes for
        much nicer scale of functionality.
        """
        with self._lock:
            mk.request(self._controller, command, payload)

        print (" >> Req: ", command, payload)
