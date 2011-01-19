#!/usr/bin/python
# coding: utf-8
import os, sys
home = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.join(home, "client/python"))
import time
import subprocess
from memlinkclient import *

MASTER_READ_PORT  = 11011
MASTER_WRITE_PORT = 11012

SLAVE_READ_PORT  = 11021
SLAVE_WRITE_PORT = 11022

memlink_master_start = ''
memlink_slave_start  = ''

def stat_check(client2master, client2slave):
    ret1, stat1 = client2master.stat_sys()
    ret2, stat2 = client2slave.stat_sys()
    if stat1 and stat2:
        if stat1.keys != stat2.keys or stat1.values != stat2.values or stat1.data_used != stat2.data_used:
            print 'stat error!'
            print 'ret1: %d, ret2: %d' % (ret1, ret2)
            print 'master stat', stat1
            print 'slave stat', stat2
            return -1
    else:
        print 'stat error!'
        print 'ret1: %d, ret2: %d' % (ret1, ret2)
        return -1

    return 0

def result_check(client2master, client2slave, key):
    ret, st1 = client2master.stat(key)
    num1 = st1.data_used
    ret1, rs1 = client2master.range(key, MEMLINK_VALUE_VISIBLE, '', 0, num1)

    ret, st2 = client2master.stat(key)
    num2 = st2.data_used
    ret2, rs2 = client2master.range(key, MEMLINK_VALUE_VISIBLE, '', 0, num2)

    if rs1 and rs2:
        pass
    else:
        print 'error: rs1 or rs2 is null'
        return -1

    if rs1.count != rs2.count:
        print 'error ', rs1.count, rs2.count
        return -1
        
    it1 = rs1.root
    it2 = rs2.root

    while it1 and it2:
        if it1.value != it2.value:
            print 'error ', it1.value, it2.value
            return -1
        if it1.mask != it2.mask:
            print 'error', it1.mask, it2.mask
            return -1
        it1 = it1.next
        it2 = it2.next

    if it1 or it2:
        print 'error: it1 or it2 is not null!'
        return -1

    return 0
    
def test_init():
    global memlink_master_start
    global memlink_slave_start
    
    cmd = "cp ../memlink ../memlink_master -rf"
    print cmd
    os.system(cmd)
    
    cmd = "cp ../memlink ../memlink_slave -rf"
    print cmd
    os.system(cmd)
    
    home = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(home)

    memlink_master_file  = os.path.join(home, 'memlink_master')
    memlink_master_start = memlink_master_file + ' etc/memlink.conf'

    memlink_slave_file  = os.path.join(home, 'memlink_slave')
    memlink_slave_start = memlink_slave_file + ' etc_slave/memlink.conf'

def start_a_new_master():
    global memlink_master_start
    #start a new master
    print 'start a new master:'
    cmd = 'bash clean.sh'
    print '   ',cmd
    os.system(cmd)
    cmd = "killall memlink_master"
    print '   ',cmd
    os.system(cmd)
    print '   ', memlink_master_start
    x1 = subprocess.Popen(memlink_master_start, stdout=subprocess.PIPE, stderr=subprocess.PIPE, 
                             shell=True, env=os.environ, universal_newlines=True)
    return x1

def restart_master():
    cmd = "killall memlink_master"
    print '   ',cmd
    os.system(cmd)
    global memlink_master_start
    print 'restart master:', memlink_master_start
    x1 = subprocess.Popen(memlink_master_start, stdout=subprocess.PIPE, stderr=subprocess.PIPE, 
                             shell=True, env=os.environ, universal_newlines=True)
    return x1

def start_a_new_slave():
    global memlink_slave_start
    #start a new slave
    print 'start a new slave:'    
    cmd = 'bash clean_slave.sh'
    print '   ',cmd
    os.system(cmd)
    cmd = "killall memlink_slave"
    print '   ',cmd
    os.system(cmd)
    print '   ', memlink_slave_start
    x2 = subprocess.Popen(memlink_slave_start, stdout=subprocess.PIPE, stderr=subprocess.PIPE, 
                             shell=True, env=os.environ, universal_newlines=True)
    return x2

def restart_slave():
    cmd = "killall memlink_slave"
    print '   ',cmd
    os.system(cmd)
    global memlink_slave_start
    print 'restart slave: ', memlink_slave_start
    x2 = subprocess.Popen(memlink_slave_start, stdout=subprocess.PIPE, stderr=subprocess.PIPE, 
                             shell=True, env=os.environ, universal_newlines=True)
    return x2
