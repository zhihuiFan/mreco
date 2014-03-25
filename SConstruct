dul = Environment(CXXFLAGS=["-w","-ggdb"],
                LIBPATH=['/opt/mongo-2.2.2/lib'],
                LIBS=['pthread','mongoclient','boost_thread','boost_filesystem','boost_program_options','boost_system', 'boost_regex'],
                CPPPATH='/opt/mongo-2.2.2/include'
                )
dul.Program('mreco', ['pdfile.cpp', 'mreco.cpp'])


test = Environment(CXXFLAGS=["-w","-ggdb"],
                LIBPATH=['/opt/mongo-2.2.2/lib'],
                LIBS=['pthread','mongoclient','boost_thread','boost_filesystem','boost_program_options','boost_system', 'boost_regex'],
                CPPPATH='/opt/mongo-2.2.2/include'
                )
test.Program('t.bin','t.cpp')

c11 = Environment(CPPFLAGS='-O0 -g -std=c++0x')
