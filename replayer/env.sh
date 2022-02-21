#!/bin/bash

export ROOTDIR=${PWD}
export MODDIR=${ROOTDIR}/nano_driver

rm -f ${ROOTDIR}/CMakeCache.txt

config-gr() {
	cd ${ROOTDIR}/build-dbg
	cmake \
	-DCMAKE_BUILD_TYPE=Debug \
	${ROOTDIR}

	cd ${ROOTDIR}/build
	cmake \
	-DCMAKE_BUILD_TYPE=Release \
	${ROOTDIR}

	cd ${ROOTDIR}
}

build-dbg() {
	cmake \
	--build ${ROOTDIR}/build-dbg \
	--target all -- \
	-j `nproc`
}

build-rls() {
	cmake \
	--build ${ROOTDIR}/build \
	--target all -- \
	-j `nproc`
}

build-mod-dbg() {
	cd ${MODDIR}
	./build.sh debug

	cd ${ROOTDIR}
}

build-mod-rls() {
	cd ${MODDIR}
	./build.sh

	cd ${ROOTDIR}
}

build-all() {
	build-dbg
	build-rls
}

mkdir -p build
mkdir -p build-dbg
