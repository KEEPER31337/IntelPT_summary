# simple_ptdump

libipt repo�� [ptdump/src/ptdump.c](https://github.com/intel/libipt/blob/master/ptdump/src/ptdump.c)�� ������ ���� ������ ptdump ���Դϴ�.

## Requirements

``` shell
sudo apt install cmake g++ make
```

[libipt v2.1](https://github.com/intel/libipt.git)

+ �Ʒ� ��ɾ �̿��Ͽ� libipt�� ��ġ�� �� �ֽ��ϴ�.

``` shell
git clone https://github.com/intel/libipt.git
cd libipt
mkdir build && cd build
cmake ..
make
sudo make install
```

## Build

�ݵ�� libipt�� **home** directory�� ��ġ�Ǿ� �־�� �մϴ�.

+ CMakeList.txt���� ���� ��ΰ� �����Ǿ� �ֽ��ϴ�. �������� ����� ���� Local ȯ�濡�� ���� ��ε��� ��ġ�ϴ��� Ȯ���ϼ���.

1. `~/libipt/libipt/internal/include`
2. `~/libipt/libipt/src/pt_last_ip.c`
3. `~/libipt/libipt/src/pt_cpu.c`
4. `~/libipt/libipt/src/pt_time.c`

+ ���� ��ε��� ��ġ�Ѵٸ�, build�� �����Ͻø� �˴ϴ�.

``` shell
mkdir build && cd build
cmake ..
make
```

## Run

``` shell
./simple_ptdump <ptfile>
```
