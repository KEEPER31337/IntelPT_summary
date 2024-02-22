# trace_tool_example

libipt repo의 [Capturing Intel PT via Linux perf_event](https://github.com/intel/libipt/blob/master/doc/howto_capture.md#capturing-intel-pt-via-linux-perf_event)를 참고해 만든 예제 툴입니다.

가상머신이 아닌 Linux를 설치한 Intel CPU 컴퓨터에서만 동작이 가능합니다.
Hyper-V를 이용하면 가상머신에서도 가능할 수도 있으니 참고하시기 바랍니다.( [Enable Intel Performance Monitoring Hardware in a Hyper-V virtual machine](https://learn.microsoft.com/en-us/windows-server/virtualization/hyper-v/manage/performance-monitoring-hardware) ).

## Requirements

``` shell
sudo apt install cmake g++ make
```

## Build

``` shell
mkdir build && cd build
cmake ..
make
```

## Run

``` shell
sudo ./trace_test <executable file>
```

실행 후  `traced_data.bin` 와  `maps_data`파일이 생성됩니다.

`traced_data.bin`파일은 libipt같은 라이브러리를 이용해 decode 할 수 있습니다.

