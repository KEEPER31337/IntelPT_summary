# simple_ptdump

libipt repo의 [ptdump/src/ptdump.c](https://github.com/intel/libipt/blob/master/ptdump/src/ptdump.c)를 참고해 만든 간략한 ptdump 툴입니다.

가상머신이 아닌 Linux를 설치한 Intel CPU 컴퓨터에서만 동작이 가능합니다.
Hyper-V를 이용하면 가상머신에서도 가능할 수도 있으니 참고하시기 바랍니다.( [Enable Intel Performance Monitoring Hardware in a Hyper-V virtual machine](https://learn.microsoft.com/en-us/windows-server/virtualization/hyper-v/manage/performance-monitoring-hardware) ).

## Requirements

``` shell
sudo apt install cmake g++ make
```

[libipt v2.1](https://github.com/intel/libipt.git) (for library)
+ optional : If you want to use tools in libipt ptdump, ptxed and other things, you should modify CMakeLists.txt in libipt before run `cmake`. 
    ex) `option(PTDUMP "Enable ptdump, a packet dumper")` -> `option(PTDUMP "Enable ptdump, a packet dumper" ON)`

``` shell
git clone https://github.com/intel/libipt.git
cd libipt
mkdir build && cd build
cmake ..
make
sudo make install
```


## Build

You have to install 'libipt'.

``` shell
mkdir build && cd build
cmake ..
make
```

## Run

``` shell
sudo ./simple_ptdump <executable file>
```

