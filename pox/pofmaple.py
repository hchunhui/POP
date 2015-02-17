from pox.core import core
from pox.openflow import libopenflow_01
from pox.lib.recoco import Timer
import pofmaple_pox

log = core.getLogger()

class handler (object):
  def __init__ (self, conn):
    self.connection = conn
    self.sw = pofmaple_pox.going_up(conn.dpid, len(conn.ports), conn.send)
    conn.addListeners(self)

  def _handle_PacketIn (self, event):
    pofmaple_pox.packet_in(self.sw, event.port, event.data)

  def _handle_PortStatus(self, event):
    desc = event.ofp.desc
    if desc.openflowEnable != 0:
      mask = libopenflow_01.ofp_port_state_rev_map['OFPPS_LINK_DOWN']
      pofmaple_pox.port_status(self.sw, desc.portId, desc.state & mask)

  def _handle_ConnectionDown (self, event):
    pofmaple_pox.going_down(self.sw)


class pofmaple_handler (object):
  def __init__ (self):
    core.openflow.addListeners(self)
    self.timer = Timer(5, pofmaple_pox.timeout, recurring = True)

  def _handle_ConnectionUp (self, event):
    handler(event.connection)


def launch ():
  core.registerNew(pofmaple_handler)
