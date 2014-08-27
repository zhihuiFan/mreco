dul = Environment(CXXFLAGS=["-w","-ggdb"],
                LIBPATH=['/opt/mongo-2.2.2/lib', '/usr/local/lib'],
                LIBS=['pthread','mongoclient','boost_thread','boost_filesystem','boost_program_options','boost_system', 'boost_regex', 'boost_system'],
                CPPPATH='/opt/mongo-2.2.2/include'
                )
dul.Program('mreco-redhat-64.bin', ['pdfile.cpp', 'mreco.cpp'])
