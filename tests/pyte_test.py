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

import pexpect
import pyte
import os
import re

ROWS, COLS = 50, 174

screen = pyte.Screen(COLS, ROWS)
stream = pyte.Stream(screen)

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


def run(T, CMD):
    child.sendline(CMD)
    child.expect(pexpect.TIMEOUT, timeout=T)
    out = emulate_ansi_terminal(child.before+child.buffer, clean=False)
    pprint(out)


def screen_exit():
    child.send('	\\')
    try:
        run(1, '')
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


child = spawn_process('../src/screen')
child.expect("Press")
out = emulate_ansi_terminal(child.before+child.buffer, clean=False)
pprint(out)
child.sendline('')

print("\nRun 'pwd':")
run(1, "pwd")

print ("\nChange PATH to ../src:")
run(1, "cd ../src/")

print ("\nRun 'ls':")
run(1, "ls")

print("\nRun 'mc':")
run(1, "mc")

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
