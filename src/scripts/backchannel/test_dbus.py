# You must initialize the gobject/dbus support for threading
# before doing anything.
import gobject
gobject.threads_init()

from dbus import glib
glib.init_threads()

# Create a session bus.
import dbus
bus = dbus.SessionBus()

# Create an object that will proxy for a particular remote object.
cbsim = bus.get_object("org.ganesha.nfsd",
                       "/org/ganesha/nfsd/MATT1")

#print cbsim.Introspect()

# call method
method2 = cbsim.get_dbus_method('method2')
print method2('MaGiC')
