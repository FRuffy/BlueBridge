"""Custom topology for BlueBridge
"""
from mininet.node import CPULimitedHost
from mininet.topo import Topo
from mininet.net import Mininet
from mininet.log import setLogLevel, info
from mininet.node import RemoteController
from mininet.cli import CLI
from mininet.link import TCLink
from mininet.topolib import TreeNet
import os
import time
from mininet.node import Host
from mininet.term import makeTerm

from functools import partial


class BlueBridge(Topo):
    "Simple topology example."

    def __init__(self):
        "Create custom topo."

        # Initialize topology
        Topo.__init__(self)

        switch = self.addSwitch('s1')
        # Create a network topology of a single switch
        # connected to three nodes.
        for hostNum in range(1, 4):  # TODO: change back to 1, 4
            # Add hosts and switches
            host = self.addHost('h' + str(hostNum))
            self.addLink(host, switch)


topos = {'BlueBridge': (lambda: BlueBridge())}


def configureHosts(net):
    hostNum = 1
    hosts = net.hosts
    for host in hosts:
        print(host)

        # Insert host configuration
        configString = "\"INTERFACE=h" + \
            str(hostNum) + \
            "-eth0\n\HOSTS=0:0:102::,0:0:103::\n\SERVERPORT=5000\n\SRCPORT=0\n\SRCADDR=0:0:01" + \
            '{0:02x}'.format(hostNum) + "::\n\DEBUG=0\" > ./tmp/config/distMem.cnf"
        host.cmdPrint('echo ' + configString)

        # Configure the interface and respective routing
        host.cmdPrint('ip address change dev h' + str(hostNum) +
                      '-eth0 scope global 0:0:01' + '{0:02x}'.format(hostNum) + '::/48')
        host.cmdPrint('ip -6 route add local 0:0:0100::/40  dev h' +
                      str(hostNum) + '-eth0')
        # host.cmdPrint('ip -6 route add local 0:0:01' +
        #               '{0:02x}'.format(hostNum) + '::/48 dev lo')
        # Gotta get dem jumbo frames
        host.cmdPrint('ifconfig h' + str(hostNum) + '-eth0 mtu 9000')
        if hostNum != 1:
                # Run the server
            host.cmdPrint('xterm  -T \"server' + str(hostNum) +
                          '\" -e \"./applications/bin/server; bash\" &')
        hostNum += 1


def run():
    privateDirs = [('./tmp/config', '/tmp/%(name)s/var/config')]
    # c = RemoteController('c', '0.0.0.0', 6633)

    host = partial(Host,
                   privateDirs=privateDirs)
    topo = BlueBridge()

    # controller is used by s1-s3
    net = Mininet(topo=topo, host=host, build=False, controller=None)
    # net = TreeNet(depth=1, fanout=3, host=host, controller=RemoteController)
    # net.addController(c)
    net.build()
    net.start()
    directories = [directory[0] if isinstance(directory, tuple)
                   else directory for directory in privateDirs]
    info('Private Directories:', directories, '\n')
    configureHosts(net)
    # net.startTerms()

    makeTerm(net.hosts[0])
    # switch.cmdPrint('ifconfig -a')
    # switch = net.switch(name=('s1'))
    # switch.cmdPrint('ip -6 route add local 0:0:01' +
    #                 '{0:02x}'.format(hostNum) + '::/48 dev s1-eth' + str(hostNum))
    # switch.cmdPrint('ifconfig s1-eth' + str(hostNum) +' mtu 5000')
    # Our current "switch"
    hostNum = 3  # TODO: change back to 3
    i = 1
    while i <= hostNum:
        # Routing entries per port
        cmd = "ovs-ofctl add-flow s1 dl_type=0x86DD,ipv6_dst=0:0:10%d::/48,priority=1,actions=output:%d" % (i, i)
        os.system(cmd)
        cmd = "ovs-ofctl add-flow s1 dl_type=0x86DD,ipv6_src=0:0:10%d::/48,ipv6_dst=0:0:10%d::/48,priority=2,actions=output:in_port" % (
            i, i)
        os.system(cmd)
        # Gotta get dem jumbo frames
        os.system('ifconfig s1-eth' + str(i) + ' mtu 9000')
        i += 1
    # Flood ndp request messages (Deprecated)
    os.system("ovs-ofctl add-flow s1 dl_type=0x86DD,ipv6_dst=ff02::1:ff00:0,priority=1,actions=output:flood")

    # Run the testing script on all clients simultaneously
    # hosts = net.hosts
    # hostNum = 1
    # time.sleep(15)
    # for host in hosts:
    #     host.cmdPrint('xterm  -T \"client' + str(hostNum) +
    #                   '\" -e \"./messaging/bin/testing; bash\" &')
    #     hostNum += 1
    CLI(net)
    net.stop()


if __name__ == '__main__':
    setLogLevel('info')
    run()
