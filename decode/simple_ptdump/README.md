# simple_ptdump

libipt repo의 [ptdump/src/ptdump.c](https://github.com/intel/libipt/blob/master/ptdump/src/ptdump.c)를 참고해 만든 간략한 ptdump 툴입니다.

## Requirements

``` shell
sudo apt install cmake g++ make
```

[libipt v2.1](https://github.com/intel/libipt.git)

+ 아래 명령어를 이용하여 libipt를 설치할 수 있습니다.

``` shell
git clone https://github.com/intel/libipt.git
cd libipt
mkdir build && cd build
cmake ..
make
sudo make install
```

## Build

반드시 libipt가 **home** directory에 설치되어 있어야 합니다.

+ CMakeList.txt에는 다음 경로가 지정되어 있습니다. 정상적인 사용을 위해 Local 환경에서 다음 경로들이 일치하는지 확인하세요.

1. `~/libipt/libipt/internal/include`
2. `~/libipt/libipt/src/pt_last_ip.c`
3. `~/libipt/libipt/src/pt_cpu.c`
4. `~/libipt/libipt/src/pt_time.c`

+ 위의 경로들이 일치한다면, build를 진행하시면 됩니다.

``` shell
mkdir build && cd build
cmake ..
make
```

## Run

``` shell
./simple_ptdump <ptfile>
```
