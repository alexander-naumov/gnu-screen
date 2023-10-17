#!/usr/bin/env python3

'''
Copyright (c) 2024 Alexander Naumov <alexander_naumov@opensuse.org>
Copyright (c) 2024 Philip Hands <phil@hands.com>

This runs screen under pexpect with pyte, an ANSI terminal emulator.

The script is intnded to allow one to run screen, perform various actions, and
then dump the resulting state of the tty-screen as text that can be compared
with known-good test runs.

PEXPECT LICENSE

    This license is approved by the OSI and FSF as GPL-compatible.
        http://opensource.org/licenses/isc-license.txt

    Copyright (c) 2012, Noah Spurrier <noah@noah.org>
    PERMISSION TO USE, COPY, MODIFY, AND/OR DISTRIBUTE THIS SOFTWARE FOR ANY
    PURPOSE WITH OR WITHOUT FEE IS HEREBY GRANTED, PROVIDED THAT THE ABOVE
    COPYRIGHT NOTICE AND THIS PERMISSION NOTICE APPEAR IN ALL COPIES.
    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

'''

import datetime
import time
import pexpect
import pyte
import os
import re
import sys


ROWS, COLS = 50, 174

screen = pyte.Screen(COLS, ROWS)
stream = pyte.Stream(screen)


def entry_log(log, message):
    try:
        log = open(log, 'a+')
        log.write("[" + datetime.datetime.now().strftime("%H:%M:%S") + "] " + message + "\n")
        log.close()
    except:
        print("can't open/write logfile")
        sys.exit(1)


def error_exit(log, message):
    print("§§§§§§§§§§§§§§§ ERROR §§§§§§§§§§§§§§§§§§§§§")
    print(message)
    print("§§§§§§§§§§§§§§§ ERROR §§§§§§§§§§§§§§§§§§§§§")
    entry_log(log, message)
    sys.exit(1)


def spawn_process(cmd):
    env = os.environ.copy()
    env.update(
            {'LINES'  : str(ROWS),
             'COLUMNS': str(COLS),
             'SHELL'  : '/bin/bash',
             'TERM'   : 'vt102'})

    return pexpect.spawn(cmd, echo=False, encoding='utf-8', dimensions=(ROWS, COLS), env=env)


def emulate_ansi_terminal(raw_output, clean=True):
    stream.feed(raw_output)

    lines = screen.display
    screen.reset()

    if clean:
        lines = (line.rstrip() for line in lines)
        lines = (line for line in lines if line)

    return '\n'.join(lines)


def pprint(out):
    print("-" * COLS)
    print(out)
    print("-" * COLS)


def run(s, T, CMD):
    s.sendline(CMD)
    s.expect(pexpect.TIMEOUT, timeout=T)
    out = emulate_ansi_terminal(s.before + s.buffer, clean=False)
    pprint(out)
    return out


def screen_exit(s):
    s.send('	\\')
    try:
        run(s, 1, '')
    except pexpect.exceptions.EOF as error:
        pass
        #print(type(error.args))
        #print(error.args)


def screen_detach():
    child.send('	d')
    try:
        run(1, '')
    except pexpect.exceptions.EOF as err:
        for i in err.args[0].split('\n'):
            if re.search("detached", i):
                ret = i.split(" ")[-1]
                ret = ret.split("]")[0]
                return ret


def screen_attach(session_id):
    child = spawn_process('../src/screen -r ' + session_id)
    child.expect("Password:")
    child.sendline("pass")
    out = emulate_ansi_terminal(child.before+child.buffer, clean=False)
    pprint(out)
    child.sendline('')


def test_welcome(s, log):
    out = emulate_ansi_terminal(s.before + s.buffer, clean=False)
    pprint(out)
    if not re.search('Screen version 5.0.0', out):
        error_exit(log, sys._getframe().f_code.co_name + ": wrong version")
    else:
        entry_log(log, "OK (" + sys._getframe().f_code.co_name + ") correct version found")
    s.sendline('')


def test_cmd_run(s, log):
    #print("\nRun 'pwd':")
    out = run(s, 1, "pwd")
    if not re.search('tests', out):
        error_exit(log, sys._getframe().f_code.co_name + "output from 'pwd' should include 'tests'")
    else:
        entry_log(log, "OK (" + sys._getframe().f_code.co_name + ") correct 'pwd' output")

    #print ("\nChange PATH to ../src:")
    out = run(s, 1, "cd ../src/")

    #print ("\nRun 'ls':")
    out = run(s, 1, "ls")
    if not re.search('ChangeLog', out):
        error_exit(log, sys._getframe().f_code.co_name + "output from 'ls' should include 'ChangeLog' file")
    else:
        entry_log(log, "OK (" + sys._getframe().f_code.co_name + ") correct 'ls' output")

    #print("\nRun 'mc':")
    out = run(s, 1, "mc")
    if not re.search('Edit', out):
        error_exit(log, sys._getframe().f_code.co_name + "output from 'mc' should include 'Edit'")
    else:
        entry_log(log, "OK (" + sys._getframe().f_code.co_name + ") correct 'mc' output")


def test_split(s, log):
    '''                                                             ___
    Split screen (ctrl-a |), switch to new window (ctrl-a tab) and | |X|
    create there a new session (ctrl-a c)                          |_|_|
    '''

    s.send('|	c')
    out = emulate_ansi_terminal(s.before + s.buffer, clean=False)
    if not re.search('tests', out):
        error_exit(log, sys._getframe().f_code.co_name + "after split and create new window we shlould see 'tests' PATH")
    else:
        entry_log(log, "OK (" + sys._getframe().f_code.co_name + ") correct view after vertical split")

    #print("\nRun 'echo $TERM':")
    #run(s, 1, "echo $TERM")

    #print("\nRun 'ping -c 5 localhost':")
    run(s, 1, "ping localhost")
    time.sleep(3)
    out = emulate_ansi_terminal(s.before + s.buffer, clean=False)
    pprint(out)


    '''                                                             ___
    Split screen (ctrl-a S), switch to new window (ctrl-a tab) and | |_|
    create there a new session (ctrl-a c)                          |_|X|
    '''
    s.send('S	c')

    #print("\nRun 'top':")
    out = run(s, 1, 'top')
    if not re.search('sleeping', out) and not re.search('64 bytes from localhost', out):
        error_exit(log, sys._getframe().f_code.co_name + "output from 3er window with 'top' is wrong")
    else:
        entry_log(log, "OK (" + sys._getframe().f_code.co_name + ") second split (vertical and horisontal) works fine")


def main():
    FILE = 'screen-' + str(datetime.datetime.now().strftime("%Y-%m-%d-%H-%M-%S")) + '.log'

    s = spawn_process('../src/screen')
    s.expect("Press")

    entry_log(FILE, "RUN TEST RUN")

    test_welcome(s, FILE)
    test_cmd_run(s,FILE)
    test_split(s,FILE)

    screen_exit(s)

'''
print("\nSplit screen (ctrl-a |), switch to new window (ctrl-a tab) and create there a new session (ctrl-a c):")
child.send('|	c')

print("\nRun 'echo $TERM':")
run(1, "echo $TERM")

print("\nRun 'ping -c 5 localhost':")
run(1, "ping -c 5 localhost")

#os.system('clear')

print("\nSplit screen (ctrl-a S), switch to new window (ctrl-a tab) and create there a new session (ctrl-a c):")
child.send('S	c')

print("\nRun 'top':")
run(1, 'top')

print("come back to first window")
child.send('	2')
pprint(emulate_ansi_terminal(child.before+child.buffer, clean=False))
print("\nsend ctrl-c to 'top':")
child.send('\x03') #ctrl-c
run(2, 'true')

print("Swith to 'mc' on first window")
child.send('0')
run(1, 'true')

print("\nSplit screen (ctrl-a S), switch to new window (ctrl-a tab) and create there a new session (ctrl-a c):")
child.send('S	c')

print("\nRun 'ls' on the last window")
child.sendline('')
run(1, 'ls -la')

print("\nTest command :next")
child.send(':next')
child.sendline('')
pprint(emulate_ansi_terminal(child.before+child.buffer, clean=False))
child.send(':next')
child.sendline('')



print("Show license:")
child.send(',')
pprint(emulate_ansi_terminal(child.before+child.buffer, clean=False))
child.expect(pexpect.TIMEOUT, timeout=3)
child.sendline('')


print("Test 'list of sessions':")
child.send('	')
child.send('"')
pprint(emulate_ansi_terminal(child.before+child.buffer, clean=False))
child.expect(pexpect.TIMEOUT, timeout=3)
pprint(emulate_ansi_terminal(child.before+child.buffer, clean=False))
child.sendline('')
child.sendline('')
#run(1, 'true')

#FIXME
print("Test 'help':")
#child.send('	')
child.send('?')
pprint(emulate_ansi_terminal(child.before+child.buffer, clean=False))
child.expect(pexpect.TIMEOUT, timeout=3)
pprint(emulate_ansi_terminal(child.before+child.buffer, clean=False))
child.expect(pexpect.TIMEOUT, timeout=3)
child.sendline('')
child.sendline('')

run(1, 'true')

#session_id = screen_detach()
#screen_attach(session_id)

screen_exit()
'''


if __name__ == "__main__":
    main()
