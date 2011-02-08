import cc

def set_options(opt):
  opt.tool_options("compiler_cc")
  opt.tool_options("compiler_cxx")

def configure(conf):
  # To build hiredis
  conf.check_tool("compiler_cc")
  conf.env.append_unique('CCFLAGS', ['-Wall', '-fPIC', '-O3'])

  # To build the ext
  conf.check_tool("compiler_cxx")
  conf.check_tool("node_addon")
  conf.env.append_unique('CXXFLAGS', ['-Wall', '-O3'])
  conf.env['LIBPATH_HIREDIS'] = 'default'
  conf.env['LIB_HIREDIS'] = 'hiredis'

# Use build groups to make sure hiredis is linked before compiling the ext.
def build(bld):
  bld.add_group("hiredis")
  hiredis = bld.new_task_gen("cc", "staticlib")
  hiredis.source = ["deps/hiredis/%s.c" % file for file in ["sds", "net", "hiredis"]]
  hiredis.target = "hiredis"

  bld.add_group("ext")
  ext = bld.new_task_gen("cxx", "shlib", "node_addon", uselib="HIREDIS")
  ext.cxxflags = ["-I../deps", "-g", "-Wall"]
  ext.source = "hiredis.cc reader.cc"
  ext.target = "hiredis"

