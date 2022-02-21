#!/bin/bash

# pm policy: "coarse_demand", "always_on"

sudo echo "coarse_demand" > /sys/devices/platform/e82c0000.mali/power_policy
cat /sys/devices/platform/e82c0000.mali/power_policy

make all KDIR=/usr/src/linux-headers-$(uname -r) \
	CONFIG_MALI_MIDGARD=m \
	CONFIG_MALI_MIDGARD_ENABLE_TRACE=y \
	CONFIG_MALI_GATOR_SUPPORT=y \
	CONFIG_MALI_SYSTEM_TRACE=y \
	CONFIG_MALI_MIDGARD_DVFS=y \
	CONFIG_MALI_EXPERT=y \
	CONFIG_MALI_PLATFORM_FAKE=y \
	CONFIG_MALI_PLATFORM_THIRDPARTY=y \
	CONFIG_MALI_PLATFORM_THIRDPARTY_NAME=devicetree \
	-j$(nproc)

#make all KDIR=/usr/src/linux-headers-$(uname -r) CONFIG_MALI_MIDGARD=m CONFIG_MALI_GATOR_SUPPORT=y CONFIG_MALI_MIDGARD_DVFS=y CONFIG_MALI_EXPERT=y CONFIG_MALI_PLATFORM_FAKE=y CONFIG_MALI_PLATFORM_THIRDPARTY=y CONFIG_MALI_PLATFORM_THIRDPARTY_NAME=devicetree

sudo rmmod gr
sudo rmmod mali_kbase
#sudo cp mali_kbase.ko /lib/modules/$(uname -r)/extras/
#sudo depmod -a
#sudo modprobe mali_kbase
sudo insmod mali_kbase.ko

