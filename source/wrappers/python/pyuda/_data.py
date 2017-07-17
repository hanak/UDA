import abc


class Data():
    """
    The base class of data that can be returned by the pyuda Client.
    """

    __metaclass__ = abc.ABCMeta

    @abc.abstractmethod
    def plot(self):
        pass

    @abc.abstractmethod
    def widget(self):
        pass

    def jsonify(self, indent=None):
        raise NotImplementedError("jsonify has not been implement for this data class")