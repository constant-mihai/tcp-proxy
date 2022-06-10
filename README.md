# tcp-proxy

To build tcp-proxy you need the shared library libdsaa.so.
```
git submodule update --init
cd c_exercises/data-structures-and-algorithms/ && make shared && cd ../../
```

In order to test the library use:
```
cd c_exercises/data-structures-and-algorithms/ && make test && ./dsaa_test && cd ../../
```

The dsaa library uses google tests. In order to test a specific scenario use: 
```
 ./dsaa_test --gtest_filter=TestList*
```
