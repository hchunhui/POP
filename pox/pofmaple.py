from sys import setdlopenflags, getdlopenflags
from ctypes import RTLD_GLOBAL
setdlopenflags(getdlopenflags() | RTLD_GLOBAL)

from pox.core import core
import pox.openflow.libopenflow_01 as of
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
      mask = of.ofp_port_state_rev_map['OFPPS_LINK_DOWN']
      pofmaple_pox.port_status(self.sw, desc.portId, desc.state & mask)

  def _handle_ConnectionDown (self, event):
    pofmaple_pox.going_down(self.sw)


class pofmaple_handler (object):
  def __init__ (self):
    core.openflow.addListeners(self)
    self.timer = Timer(5, pofmaple_pox.timeout, recurring = True)

  def _handle_ConnectionUp (self, event):
    ports_list = core.PofManager.get_all_ports(event.dpid)
    for port in ports_list:
      phy_port = port.desc
      if phy_port.openflowEnable == 0:
        core.PofManager.set_port_pof_enable(event.dpid, phy_port.portId)
    handler(event.connection)

def launch (algo_file="./l3_multi.so", spec_file="header.spec"):
  pofmaple_pox.init(algo_file, spec_file)
  core.registerNew(pofmaple_handler)
