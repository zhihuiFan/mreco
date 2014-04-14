dul = Environment(CXXFLAGS=["-w","-ggdb"],
                LIBPATH=['/opt/mongo-2.2.2/lib'],
                LIBS=['pthread','mongoclient','boost_thread','boost_filesystem','boost_program_options','boost_system', 'boost_regex'],
                CPPPATH='/opt/mongo-2.2.2/include'
                )
dul.Program('mreco-0.8-redhat-64.bin', ['pdfile.cpp', 'mreco.cpp'])
