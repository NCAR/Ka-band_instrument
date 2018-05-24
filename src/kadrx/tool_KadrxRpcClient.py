#
# Client class to allow simple access to kadrx status via XML-RPC calls
#
import os

tools = Split("""
    archive_xmlrpc_c
    boost_serialization
    xmitclient
    xmlrpc_client++
    doxygen
""")
env = Environment(tools=['default'] + tools)

# The object file and header file live in this directory.
tooldir = env.Dir('.').srcnode().abspath    # this directory
includeDir = tooldir

sources = Split("""
    KadrxRpcClient.cpp
    KadrxStatus.cpp
""")

headers = Split("""
    KadrxRpcClient.h
    KadrxStatus.h
""")
lib = env.Library('kadrxrpcclient', sources)

def KadrxRpcClient(env):
    env.Require(tools)
    env.AppendUnique(CPPPATH = [includeDir])
    env.AppendUnique(LIBS = [lib])
#     env.AppendDoxref("KadrxRpcClient")

Export('KadrxRpcClient')
