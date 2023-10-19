#!/usr/bin/python3
import logging
import sys
import re

def fileparser(FILE: str) -> list[str]:
    file = []
    try:
        fd = open(FILE, 'r+')
        file = fd.read().split("\n")
        fd.close()
    except:
        print("can't open/read file " + FILE)

    return file


def man_comm() -> list[str]:
    man_full = fileparser("../../src/doc/screen.1")
    man_comm = []

    rec = 0
    for i in man_full:
        if rec:
            man_comm.append(i)
        if i == "The following commands are available:":
            rec = 1
        if re.match('.SH \"THE MESSAGE LINE', i):
            rec = 0

    return man_comm


def check_doc(debug, man, commands) -> list[str]:
    ERR = []
    for comm in commands:
        ret = 0
        if debug: print("===== " + comm + " =============================")
        for line in man:
            #FIXME: one universal regex?
            if re.search('^\.B. "' + comm + '"*', line):
                if debug: print(line)
                ret = 1
                continue
            if re.search('^\.B. ' + comm + '*', line):
                if debug: print(line)
                ret = 1
                continue
            if re.search('^\.B ' + comm + '*', line):
                if debug: print(line)
                ret = 1
                continue
            if re.search('^\.B "' + comm + '"*', line):
                if debug: print(line)
                ret = 1
                continue
        if ret == 0:
            ERR.append(comm)
    return ERR


def get_comm() -> list[str]:
    comm = []
    for i in fileparser("../../src/comm.c"):
        if re.search("{", i):
            i = i.split('"')
            if len(i) > 1:
                comm.append(i[1])
    return comm


def main():
    debug = True if 'debug' in sys.argv else False

    comm = get_comm()
    man  = man_comm()
    ndoc = check_doc(debug, man, comm)

    if len(ndoc) > 0:
        print("\nNo documentation:\n")
        for i in ndoc: 
            print(i)
        sys.exit(2)
    else:
        print("everything fine :)")
        sys.exit(0)


if __name__ == '__main__':
    main()
