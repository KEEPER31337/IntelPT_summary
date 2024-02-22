# Intel PT를 이용해 trace하는 방법

* [Linux](#linux)
  + [perf command](#perf-command)
  + [perf system call ( sample tool )](#perf-system-call---sample-tool--)
  + [simple pt](#simple-pt)
* [Windows](#windows)
  + [winipt](#winipt)


## Intel PT features check

Intel PT를 사용하기 위해서는 CPU에서 Intel PT를 지원하는지 확인해야합니다.

[Quick and dirty check for Intel Processor Trace (PT) features](https://gist.github.com/carter-yagemann/d8718f7f6f6b7fd99d325a1c89542c61)

## Linux

### perf command

[Perf tools support for Intel® Processor Trace](https://perf.wiki.kernel.org/index.php/Perf_tools_support_for_Intel%C2%AE_Processor_Trace)

[Adding Processor Trace support to Linux](https://lwn.net/Articles/648154/)

[perf-intel-pt (linux man page)](https://man7.org/linux/man-pages/man1/perf-intel-pt.1.html)

### perf system call ( sample tool )

[example-tool](./trace_tool_example)

### simple pt

[simple-pt](https://github.com/andikleen/simple-pt)

## Windows

### winipt

[winipt](https://github.com/ionescu007/winipt)

