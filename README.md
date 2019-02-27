# noah.pp

## highlights
### NOAH blockchain node
+ portable executable
+ separate library, implementing p2p protocol, rpc server, distributed consensus algorithm
+ minimal, tiny http protocol implementation. rpc server provides the same JSON API both over TCP and HTTP (POST)
+ *action log*, interface that allows relational database maintainer to build the blockchain state in any desired DBMS
### commander
+ separate RPC JSON and HTTP APIs
+ blockchain explorer
+ wallet functionality

## details on supported/unsupported features/technologies
+ *dependencies?* [publiq.pp](https://github.com/publiqnet/publiq.pp "publiq.pp")
+ *portable?* yes! it's a goal. clang, gcc, msvc working.
+ *build system?* cmake. refer to cmake project generator options for msvc, xcode, etc... project generation.
+ *static linking vs dynamic linking?* both supported, refer to BUILD_SHARED_LIBS cmake option
+ c++11

## how to build noah.pp?
First build [publiq.pp](https://github.com/publiqnet/publiq.pp "publiq.pp").
It's important to use the same *CMAKE_INSTALL_PREFIX* both for publiq.pp and for noah.pp, 
for noah.pp build to be able to find the external dependencies.


```console
user@pc:~$ mkdir projects
user@pc:~$ cd projects
user@pc:~/projects$ git clone https://github.com/publiqnet/noah.pp
user@pc:~/projects$ cd noah.pp
user@pc:~/projects/noah.pp$ git submodule update --init --recursive
user@pc:~/projects/noah.pp$ cd ..
user@pc:~/projects$ mkdir noah.pp.build
user@pc:~/projects$ cd noah.pp.build
user@pc:~/projects/noah.pp.build$ cmake -DCMAKE_INSTALL_PREFIX=/the/install/dir ../noah.pp
user@pc:~/projects/noah.pp.build$ cmake --build . --target install
```

Again - this implies that */the/install/dir* has been used for publiq.pp build as *CMAKE_INSTALL_PREFIX*.

### how to use noahd?
there is a command line arguments help, use it wisely!
also pay attention to all the console logging.

### more details?
refer to the wiki page.
