#!/bin/bash
TESTDIR="$(dirname "$0")"

# check for QEMU if QEMU_RUN is set
if [ ! -z "${QEMU_RUN}" ]; then
    QEMU_VERSION=$(${QEMU_RUN} --version 2> /dev/null)
    if [ -z "${QEMU_VERSION}" ]; then
        echo "**** You need QEMU to run tests on non-native platform"
        exit 1
    fi
fi

CVEs="CVE-2002-0059 CVE-2004-0797 CVE-2005-1849 CVE-2005-2096"

for CVE in $CVEs; do
    fail=0
    for testcase in ${TESTDIR}/${CVE}/*.gz; do
    ${QEMU_RUN} ../minigzip${EXE} -d < "$testcase"
    # we expect that a 1 error code is OK
    # for a vulnerable failure we'd expect 134 or similar
    if [ $? -ne 1 ] && [ $? -ne 0 ]; then
        fail=1
    fi
    done
    if [ $fail -eq 0 ]; then
    echo "          --- zlib not vulnerable to $CVE ---";
    else
    echo "          --- zlib VULNERABLE to $CVE ---"; exit 1;
    fi
done
