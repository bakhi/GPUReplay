#!/bin/sh

PM_POLICY=/sys/devices/platform/e82c0000.mali/power_policy

sudo echo "coarse_demand" > ${PM_POLICY}
cat ${PM_POLICY}

sudo rmmod mali_kbase

make $1 
sudo rmmod gr
sudo insmod gr.ko

