from __future__ import (division, print_function, absolute_import)

import cpyuda

from ._signal import Signal
from ._string import String
from ._structured import StructuredData
from ._video import Video

from six import with_metaclass
import logging
from collections import namedtuple
import sys
try:
    from enum import Enum
except ImportError:
    Enum = object


class ClientMeta(type):
    """
    Metaclass used to add class level properties
    """
    def __init__(cls, what, bases=None, dict=None):
        type.__init__(cls, what, bases, dict)

    @property
    def port(cls):
        return cpyuda.get_server_port()

    @port.setter
    def port(cls, value):
        cpyuda.set_server_port(value)

    @property
    def server(cls):
        return cpyuda.get_server_host_name()

    @server.setter
    def server(cls, value):
        cpyuda.set_server_host_name(value)





class Client(with_metaclass(ClientMeta, object)):
    """
    A class representing the IDAM client.

    This is a pythonic wrapper around the low level c_uda.Client class which contains the wrapped C++ calls to
    UDA.
    """
    __metaclass__ = ClientMeta

    def __init__(self, debug_level=logging.ERROR):
        logging.basicConfig(level=debug_level)
        self.logger = logging.getLogger(__name__)

        self._registered_subclients = {}

        try:
            from mast.geom import GeomClient
            from mast import MastClient
            self._registered_subclients['geometry'] = GeomClient(self)
            self._registered_subclients['listGeomSignals'] = GeomClient(self)
            self._registered_subclients['listGeomGroups'] = GeomClient(self)
            self._registered_subclients['list'] = MastClient(self)
            self._registered_subclients['list_signals'] = MastClient(self)
            self._registered_subclients['put'] = MastClient(self)
        except ImportError:
            pass

    def get_file(self, source_file, output_file=None):
        """
        Retrieve file using bytes plugin and write to file
        :param source_file: the full path to the file
        :param output_file: the name of the output file
        :return:
        """

        result = cpyuda.get_data("bytes::read(path=%s)" % source_file, "")

        with open(output_file, 'wb') as f_out:
            result.data().tofile(f_out)

        return

    def get_text(self, source_file):
        """
        Retrive a text file using the bytes plugin and decode as string
        :param source_file: the full path to the file
        :return:
        """

        result = cpyuda.get_data("bytes::read(path=%s)" % source_file, "")

        if sys.version_info[0] <= 2:
            result_str = result.data().tostring()
        else:
            result_str = result.data().tobytes().decode('utf-8')
        return result_str

    def get(self, signal, source, **kwargs):
        """
        IDAM get data method.

        :param signal: the name of the signal to get
        :param source: the source of the signal
        :param kwargs: additional optional keywords for geometry data
        :return: a subclass of pyuda.Data
        """
        # Standard signal
        result = cpyuda.get_data(str(signal), str(source))

        if result.is_tree():
            tree = result.tree()
            if tree.data()['type'] == 'VIDEO':
                return Video(StructuredData(tree))
            else:
                return StructuredData(tree.children()[0])
        elif result.is_string():
            return String(result)
        return Signal(result)

    def __getattr__(self, item):
        if item in self._registered_subclients:
            return getattr(self._registered_subclients[item], item)

        raise AttributeError("%s method not found in registered subclients" % item)

    @classmethod
    def get_property(cls, prop):
        return cpyuda.get_property(prop)

    @classmethod
    def set_property(cls, prop, value):
        cpyuda.set_property(prop, value)
