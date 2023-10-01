#!/usr/bin/env python3

import pexpect
import pyte
import os

ROWS, COLS = 45, 160

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
    except pexpect.exceptions.EOF as error:
        print(error)


child = spawn_process('../src/screen')
child.expect("Press")
out = emulate_ansi_terminal(child.before+child.buffer, clean=False)
pprint(out)
child.sendline('')

child.send('	')

run(1, "mc")

child.send('|	c')

run(1, "echo $TERM")
run(5, "ping -c 5 google.de")

#os.system('clear')

run(1, 'top')
child.send('\x03') #ctrl-c
run(1, 'true')

#screen_detach()

screen_exit()


