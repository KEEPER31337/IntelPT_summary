# simple_ptdump

libipt repo�� [ptdump/src/ptdump.c](https://github.com/intel/libipt/blob/master/ptdump/src/ptdump.c)�� ������ ���� ������ ptdump ���Դϴ�.

����ӽ��� �ƴ� Linux�� ��ġ�� Intel CPU ��ǻ�Ϳ����� ������ �����մϴ�.
Hyper-V�� �̿��ϸ� ����ӽſ����� ������ ���� ������ �����Ͻñ� �ٶ��ϴ�.( [Enable Intel Performance Monitoring Hardware in a Hyper-V virtual machine](https://learn.microsoft.com/en-us/windows-server/virtualization/hyper-v/manage/performance-monitoring-hardware) ).

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

