from pox.core import core
import pofmaple_pox

log = core.getLogger()

class handler (object):
  def __init__ (self, conn):
    self.connection = conn
    self.sw = pofmaple_pox.going_up(conn.dpid, len(conn.ports), conn.send)
    conn.addListeners(self)

  def _handle_PacketIn (self, event):
    packet = event.parsed
    pofmaple_pox.packet_in(self.sw, event.port, event.data)

  def _handle_ConnectionDown (self, event):
    pofmaple_pox.going_down(self.sw)


class pofmaple_handler (object):
  def __init__ (self):
    core.openflow.addListeners(self)

  def _handle_ConnectionUp (self, event):
    handler(event.connection)


def launch ():
  core.registerNew(pofmaple_handler)
