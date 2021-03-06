#!/bin/bash

# @FRuffy what is the purpose of this script
sudo sysctl vm.swappiness=0
ID=`id -u`
if [ $ID -ne 0 ]; then
   echo "This command must be run as root."
   exit 1
fi
#ethtool -N enp66s0f0 rx-flow-hash udp6 sdfn
sudo sysctl -w net.core.rmem_max=16777216
sudo ethtool -C enp66s0f0 rx-usecs 0
sudo ethtool -C enp66s0f0 tx-usecs 0
sudo ethtool -K enp66s0f0 tso off
sudo ethtool -K enp66s0f0 gro off

# sudo ethtool -K enp66s0f0 rx off 
# sudo ethtool -K enp66s0f0 tx off 
#sudo ethtool -K enp66s0f0 gso off
#sudo ethtool -K enp66s0f0 sg off
# sudo ethtool -G enp66s0f0 rx 4096 tx 4096
# sudo ethtool --offload  enp66s0f0  rx off tx off
# sudo ethtool -K enp66s0f0 ntuple off
# sudo ethtool -A enp66s0f0 autoneg off rx off tx off
#ip6tables -t raw -I PREROUTING 1 --src 100::/8 -j NOTRACK
#ip6tables -I INPUT 1 --src 100::/8 -j ACCEPT

# ncpus=`grep -ciw ^processor /proc/cpuinfo`
# test "$ncpus" -gt 1 || exit 1
# n=0
# for irq in `cat /proc/interrupts | grep eth | awk '{print $1}' | sed s/\://g`
# do
#     f="/proc/irq/$irq/smp_affinity"
#     test -r "$f" || continue
#     cpu=$[$ncpus - ($n % $ncpus) - 1]
#     if [ $cpu -ge 0 ]
#             then
#                 mask=`printf %x $[2 ** $cpu]`
#                 echo "Assign SMP affinity: eth queue $n, irq $irq, cpu $cpu, mask 0x$mask"
#                 echo "$mask" > "$f"
#                 let n+=1
#     fi
# done
# echo 32768 > /proc/sys/net/core/rps_sock_flow_entries  
# for RX in `seq 0 7`; do  
#             echo 2048 > /sys/class/net/enp66s0f0/queues/rx-$RX/rps_flow_cnt
# done
# XPS=("0 12" "1 13" "2 14" "3 15" "4 16" "5 17" "6 18" "7 19");
# (let TX=0; for CPUS in "$XPS[@]"; do
#      let mask=0
#      for CPU in $CPUS; do let mask=$((mask | 1 << $CPU)); done
#      printf %X $mask > /sys/class/net/enp66s0f0/queues/tx-$TX/xps_cpus
#      let TX+=1
#done)