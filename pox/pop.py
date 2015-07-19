from sys import setdlopenflags, getdlopenflags
from ctypes import RTLD_GLOBAL
setdlopenflags(getdlopenflags() | RTLD_GLOBAL)

from pox.core import core
import pox.openflow.libopenflow_01 as of
from pox.lib.recoco import Timer
import pop_pox

log = core.getLogger()

class handler (object):
  def __init__ (self, conn, portids):
    self.connection = conn
    self.sw = pop_pox.going_up(conn.dpid, portids, len(portids), conn.send)
    conn.addListeners(self)

  def _handle_PacketIn (self, event):
    pop_pox.packet_in(self.sw, event.port, event.data)

  def _handle_PortStatus(self, event):
    desc = event.ofp.desc
    if desc.openflowEnable != 0:
      mask = of.ofp_port_state_rev_map['OFPPS_LINK_DOWN']
      pop_pox.port_status(self.sw, desc.portId, desc.state & mask)

  def _handle_ConnectionDown (self, event):
    pop_pox.going_down(self.sw)


class pop_handler (object):
  def __init__ (self):
    core.openflow.addListeners(self)
    self.timer = Timer(5, pop_pox.timeout, recurring = True)

  def _handle_ConnectionUp (self, event):
    ports = core.PofManager.get_all_ports(event.dpid)
    portids = [];
    for port in ports:
      phy_port = port.desc
      if phy_port.openflowEnable == 0:
        core.PofManager.set_port_pof_enable(event.dpid, phy_port.portId)
        portids += [phy_port.portId]
    handler(event.connection, portids)

def launch (algo_file="./l3_multi.so", spec_file="header.spec"):
  pop_pox.init(algo_file, spec_file)
  core.registerNew(pop_handler)
