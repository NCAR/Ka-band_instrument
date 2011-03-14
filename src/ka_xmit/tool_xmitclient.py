#
# Rules to build XmitStatus class and export it (and its header) as a tool
#
import os

tools = ['xmlrpc']
env = Environment(tools=['default'] + tools)

# The object file and header file live in this directory.
tooldir = env.Dir('.').srcnode().abspath    # this directory
includeDir = tooldir

lib = env.Library('xmitclient', 'XmitClient.cpp')
    
def xmitclient(env):
    env.Require(tools)
    env.AppendUnique(CPPPATH = [includeDir])
    env.AppendUnique(LIBS = [lib])

Export('xmitclient')
