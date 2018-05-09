#!/bin/bash
set +x
USER_R="fruffy"
DIR_LOCAL="./"
DIR_REMOTE="/home/$USER_R/bluebridge"
SERVERS=("192.168.79.41" "192.168.79.27" "192.168.79.53" "192.168.79.43")

IP6TARGET="$DIR_LOCAL/ip6/lib "
IP6TARGET+="$DIR_LOCAL/ip6/*.mk "
APPTARGET+="$DIR_LOCAL/ip6/apps/*.c "
APPTARGET+="$DIR_LOCAL/ip6/apps/*.mk "

BBTARGET="makefile"

for server in "${SERVERS[@]}"
do
    scp -r $IP6TARGET $USER_R@$server:$DIR_REMOTE/ip6/ &
    scp -r $APPTARGET $USER_R@$server:$DIR_REMOTE/ip6/apps &
    scp -r $BBTARGET $USER_R@$server:$DIR_REMOTE/ & 
done
for job in `jobs -p`
do
    echo "Waiting for $job"
    wait $job
done
echo "Done with copying!"
for server in "${SERVERS[@]}"
do
    ssh $USER_R@$server make -C $DIR_REMOTE &
done